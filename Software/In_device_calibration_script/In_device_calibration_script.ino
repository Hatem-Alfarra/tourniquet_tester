/*

 Arduino GND -> LCD GND
 Arduino 3.3V -> LCD VCC
 Arduino analog pin A4 -> LCD SDA 
 Arduino analog pin A5 -> LCD SCL

 Arduino GND -> HX711 GND
 Arduino pin 3 (otherwise change below) -> HX711 DT
 Arduino pin 2 (otherwise change below) -> HX711 SCK
 Arduino 5V -> HX711 VCC

*/
#include <EEPROM.h>
#include "HX711.h"
#include <LiquidCrystal_I2C.h>

// Debug define goes here. Comment out to disable debug prints. [TODO]

// Reset device as new [TODO]


// HX711 pins
#define DT_PIN  3
#define SCK_PIN 2

// functions prototypes
void setThreshold();
void calibrationAsk();
void confirm(void (*functionPass)(), void (*functionFail)());
void calibrate();
long readSensorAdjusted();
long readAdjustedStable();
bool isNeverCalibratedBefore();
bool isCalibrationValid();
void getCalibrationValues();
void setCalibrationValues();
float adjustedToPressure(long sensorValAdjusted);
float calculateAvgSlope();
float calculateIntercept(float slope);
void drawBar(int intentFill);
void printLCD(const char* line1, const char* line2 = nullptr, unsigned long waitMs = 0);


// Global variables
long threshold;                                      
long zeroOffset;
float avgSlope;
float intercept = 0;
int pressures[] = {60, 120, 180, 240, 300};
const int REF_N = sizeof(pressures) / sizeof(pressures[0]);
long refValues[REF_N];
const int ADDRESS_CALIBRATION_SIGNATURE = 0;
const uint32_t CALIBRATION_SIGNATURE = 0xA5A5A5A5;
const int ADDRESS_REF_VALUES_START = ADDRESS_CALIBRATION_SIGNATURE + sizeof(CALIBRATION_SIGNATURE);    
const int ADDRESS_SLOPE = ADDRESS_CALIBRATION_SIGNATURE + sizeof(CALIBRATION_SIGNATURE) + sizeof(refValues);
const int ADDRESS_INTERCEPT = ADDRESS_SLOPE + sizeof(avgSlope);
const int INTENT_LEVELS = 7;
const int NOISE_MULTIPLIER = 10;

// create objects for HX711 and LCD
HX711 scale;
LiquidCrystal_I2C lcd(0x27, 16, 2);


// functions

void setup() 
{
   Serial.begin(9600);

   // Initialize HX711
   scale.begin(DT_PIN, SCK_PIN);

   // Initialize LCD
   lcd.init();
   lcd.backlight();
   // Collect offset
   printLCD("Remove any force", "from the button", 2000);
   printLCD("Zeroing...");
   zeroOffset = scale.read_average(20);
   
   // Debug print. change to tag [TODO] or remove later.
   Serial.print("Zero offset: ");
   Serial.println(zeroOffset);

   setThreshold();                                                  
   lcd.clear();

   // Uncomment the following 2 lines to reset calibration as if never calibrated before (for testing purposes). Clean up [TODO]
   // uint32_t resetVal = 0x00000000;
   // EEPROM.put(ADDRESS_CALIBRATION_SIGNATURE, resetVal);
   
   calibrationAsk();
}

void setThreshold()
{
   printLCD("Threshold", "setting...");
   int samples = 40;
   long sensorValRaw = scale.read();
   long maxVal = sensorValRaw;
   long minVal = sensorValRaw;

   for (int i = 0; i < samples; i++)
   {
      sensorValRaw = scale.read();

      maxVal = max(maxVal, sensorValRaw);
      minVal = min(minVal, sensorValRaw);             

      delay(50);
   }  

   // Debug prints. change to tag [TODO] or remove later.
   Serial.print("Max value: ");
   Serial.println(maxVal);
   Serial.print("Min value: ");
   Serial.println(minVal);

   if (maxVal == minVal)
   {
      printLCD("Sensor error!", "Check connections");
      while(true);
   }

   long dif1  = abs(maxVal - zeroOffset);
   long dif2 = abs(minVal - zeroOffset);
   long difmax = max(dif1, dif2);
   long difmin = min(dif1, dif2);
   float difratio = (float) difmax / (float) difmin;

   Serial.print("Zero offset: ");
   Serial.println(zeroOffset);
   Serial.print("Difference max: ");
   Serial.println(difmax);
   Serial.print("Difference min: ");
   Serial.println(difmin);
   Serial.print("Difference ratio: ");
   Serial.println(difratio);

   if (difmin == 0 || minVal >= zeroOffset || maxVal <= zeroOffset)
   {
      printLCD("Rezeroing...", "", 1000);
      zeroOffset = scale.read_average(20);
      setThreshold();
      return;
   }
   else if (difratio > 2.0){
      printLCD("Keep device", "steady, retrying", 4000);
      setThreshold();
      return;
   }
   else
   {
      threshold = (difmax*2) * NOISE_MULTIPLIER * INTENT_LEVELS;
      Serial.print("Threshold set to: ");
      Serial.println(threshold);
   }
}

void calibrationAsk()
{
   // If never calibrated before or data is corrupted, go to calibration directly. If not the data is loaded.
   if (!isCalibrationValid())
   {
      calibrate();
      return;
   }


   int refreshRatePerSec = 10;
   int timeLeftInSec = 5;
   int timeLeft = refreshRatePerSec * timeLeftInSec;


   printLCD("Calibrate?");

   while(timeLeft--)
   {
      timeLeftInSec = (int) ((timeLeft/refreshRatePerSec)+1); 
      // display time left
      lcd.setCursor(13, 0);
      lcd.print(timeLeftInSec);
      lcd.print("s ");

      
      long intentFill, intentLevel;

      long sensorValAdjusted = readSensorAdjusted();
      
      intentLevel = threshold / INTENT_LEVELS;                             // value needed per intentLevel (ie. step or "#").
      intentFill = sensorValAdjusted / intentLevel;                        // number of steps (#) filled with the current press.
      
      drawBar(intentFill);

      if (intentFill >= INTENT_LEVELS)
      {
         confirm(calibrate, calibrationAsk);
         return;
      }

      delay(1000/refreshRatePerSec);
   }
}

void confirm(void (*functionPass)(), void (*functionFail)())
{
   int refreshRatePerSec = 10;
   int timeLeftInSec = 5;
   int timeLeft = refreshRatePerSec * timeLeftInSec;

   printLCD("Hold for", "sec to confirm");

   long sensorValAdjusted;
   while (timeLeft--)
   {  
      sensorValAdjusted = readSensorAdjusted();

      if (sensorValAdjusted < threshold/2) 
      {
         // call function for failed confirmation
         (*functionFail)(); 
         return;
      }

      timeLeftInSec = (int) ((timeLeft/refreshRatePerSec)+1); 
      lcd.setCursor(9, 0);
      lcd.print("  ");
      lcd.setCursor(9, 0);
      lcd.print(timeLeftInSec);

      delay(1000/refreshRatePerSec);
   }
   
   // call function for passed confirmation
   (*functionPass)(); 
   
}

void calibrate()
{
   printLCD("Calibration Mode", "", 3000);
   printLCD("Set up cuff", "You have   s");
   int timeleftInSec = 20;
   while (timeleftInSec > 0)
   {
      lcd.setCursor(9, 1);
      lcd.print("  ");
      lcd.setCursor(9, 1);
      lcd.print(timeleftInSec);
      delay(1000);
      timeleftInSec--;
   }
   
   
   // loop over desired pressures
   // i is declared outside the loop because it is used after the loop to check if all reference values were collected successfully.
   int i;
   for (i = 0; i < REF_N; i++)
   {
      int curPressure = pressures[i];
      long curReading;

      int maxTries = 3;
      int numTries = 0;

      long previous;

      if (i == 0) 
      {
         previous = 0;
      } else 
      {
         previous = refValues[i-1];
      }

      printLCD("Set cuff", "to     mmHg");           
      lcd.setCursor(3, 1);
      lcd.print(curPressure);
      delay(10000);                                                // delay to allow user to set the cuff to correct pressure

      printLCD("Collecting", "values...");
      curReading = readAdjustedStable();


      while (curReading == -1 && numTries < maxTries)              // while the reading is invalid and user have not ran out of tries
      {
         numTries += 1;

         printLCD("Reading unstable!", "", 2000);

         printLCD("Set cuff", "to     mmHg");           
         lcd.setCursor(3, 1);
         lcd.print(curPressure);
         delay(5000);                                              // delay to allow user to set the cuff to correct pressure

         printLCD("Collecting", "values...");
         curReading = readAdjustedStable();
      }
      
      if (numTries == maxTries)
      {
         printLCD("Unstable readings", "", 2000);

         printLCD("Reverting to pre", "-vious settings", 5000);

         // Disregard changes and set refValues[] back to previous settings
         getCalibrationValues();

         printLCD("Normal mode");
         
         break;
      } else if (curReading > (previous + (threshold / INTENT_LEVELS)))                            // desired behavior during calibration. Instead of 'previous', use previous + threshold/something or something else to make sure same values do not result in valid calibration
      {
         refValues[i] = curReading;
      } else 
      {
         printLCD("Value lower", "than expected", 2000);

         printLCD("Reverting to pre", "-vious settings", 5000);

         // Disregard changes and set refValues[] back to previous settings
         getCalibrationValues();

         break;
      }
   }
   // if all reference values were collected
   if (i == REF_N)
   {   
      // write to EEPROM the new ref value (from refValues[])
      setCalibrationValues();
      printLCD("Calibration", "successful", 2000);
      printLCD("Normal mode", "", 2000);
      getCalibrationValues();
      // Calibration complete, entering normal operation
   } else
   {
      // Disregard changes and set in code values (ie. refValues[]) back to previous settings
      getCalibrationValues();
   }
}

long readSensorAdjusted()
{
   float raw = scale.read();
   long adjusted = fabs(raw - zeroOffset);
   return adjusted;
}

// check if stable values. If not return -1.
long readAdjustedStable()
{
   long sum = 0;
   long avg = 0;
   int samples = 20;
   
   long adjusted = readSensorAdjusted();

   long minVal = adjusted;
   long maxVal = adjusted;

   for (int i = 0; i < samples; i++)
   {
      adjusted = readSensorAdjusted();

      minVal = min(minVal, adjusted);
      maxVal = max(maxVal, adjusted);
      sum += adjusted;

      delay(100);
   }

   avg = sum / samples;

   if ((maxVal - minVal) > threshold / 2 )
   {
      return -1;                                                   // unstable
   }

   return avg;                                                     // If stable return average
}

bool isNeverCalibratedBefore()
{
   uint32_t foundConfirmationNum;
   EEPROM.get(ADDRESS_CALIBRATION_SIGNATURE, foundConfirmationNum);

   if (foundConfirmationNum != CALIBRATION_SIGNATURE)
   {
      printLCD("Never calibrated", "before", 5000);
      return true;
   }
   return false;
}

bool isCalibrationValid()
{
   if (isNeverCalibratedBefore()) 
   {
      return false;
   }

   getCalibrationValues();
   float calcSlope = calculateAvgSlope();
   float calcIntercept = calculateIntercept(calcSlope);

   // check for data corruption and/or unsuccessful changes (ie. removing power during the write process). 
   return calcSlope == avgSlope && calcIntercept == intercept;
}

void getCalibrationValues()
{
   if (isNeverCalibratedBefore())
   {
      calibrate();
      return;
   }  
   
   int curAddress = ADDRESS_REF_VALUES_START;

   for (int i = 0; i < REF_N; i++)
   {
      EEPROM.get(curAddress, refValues[i]);
      curAddress += sizeof(refValues[i]);
      
      // debug print. change to tag [TODO] or remove later.
      // Serial.print("Data point ");
      // Serial.print(i+1);
      // Serial.print(": ");
      // Serial.println(refValues[i]);
   }

   EEPROM.get(ADDRESS_SLOPE, avgSlope);
   EEPROM.get(ADDRESS_INTERCEPT, intercept);
}

void setCalibrationValues()
{
   uint32_t foundConfirmationNum;
   EEPROM.get(ADDRESS_CALIBRATION_SIGNATURE, foundConfirmationNum);
   
   
   // Set EEPROM to new values in refValues[]. Write to memory (be careful of wearout). 
   int curAddress = ADDRESS_REF_VALUES_START;
   for (int i = 0; i < REF_N; i++)
   {
      EEPROM.put(curAddress, refValues[i]);
      curAddress += sizeof(refValues[i]);

      // debug print. change to tag [TODO] or remove later.
      // Serial.print("Data point ");
      // Serial.print(i+1);
      // Serial.print(": ");
      // Serial.println(refValues[i]);
   }

   avgSlope = calculateAvgSlope();
   EEPROM.put(ADDRESS_SLOPE, avgSlope);

   intercept = calculateIntercept(avgSlope);
   EEPROM.put(ADDRESS_INTERCEPT, intercept);

   // Avoid rewriting if signature written before. Do not ware out signature data addresses.
   if (foundConfirmationNum != CALIBRATION_SIGNATURE)
   {
      EEPROM.put(ADDRESS_CALIBRATION_SIGNATURE, CALIBRATION_SIGNATURE);
   }

}

float adjustedToPressure(long sensorValAdjusted)
{
   return (avgSlope * sensorValAdjusted) + intercept;
}

float calculateAvgSlope()
{
   float sumSlopes = 0;
   for (int i = 1; i < REF_N; i++)
   {
      sumSlopes += (float) (pressures[i] - pressures[0]) / (float) (refValues[i] - refValues[0]);

      // TODO: Debug tag
      // Serial.print("Slope for data point ");
      // Serial.print(i+1);
      // Serial.print(": ");
      // Serial.println((float) (pressures[i] - pressures[0]) / (float) (refValues[i] - refValues[0]), 10);

   }
   // Serial.print("Average slope: ");
   // Serial.println(sumSlopes / (REF_N-1), 10);
   return sumSlopes / (REF_N-1);
}

float calculateIntercept(float slope)
{
   float sumIntercepts = 0;
   for (int i = 0; i < REF_N; i++)
   {
      sumIntercepts += pressures[i] - slope * refValues[i];

      // TODO: Debug tag
      // Serial.print("Intercept for data point ");
      // Serial.print(i+1);
      // Serial.print(": ");
      // Serial.println(pressures[i] - slope * refValues[i], 10);
   }
   return sumIntercepts / REF_N;
}

void drawBar(int intentFill)                         
{
   lcd.setCursor(0, 1);
   lcd.print("No [");
   for (int i=0; i < INTENT_LEVELS; i++)
   {
      if (i < intentFill)
      {
         lcd.print("#");                                           // [TODO] Intent bar can later be changed to look prettier
      } else 
      {
         lcd.print(" ");
      }
   }
   lcd.print("] Yes");
}

void printLCD(const char* line1, const char* line2, unsigned long waitMs)
{
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print(line1);
   if (line2)
   {
      lcd.setCursor(0, 1);
      lcd.print(line2);
   }

   if (waitMs > 0) delay(waitMs);
}

void loop()
{
   long sensorVal = readSensorAdjusted();

   float mmHg = adjustedToPressure(sensorVal);

   if (mmHg < 5) {mmHg = 0;}

   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Pressure:");
   lcd.setCursor(0, 1);
   lcd.print(mmHg, 1);
   lcd.print("mmHg");


   delay(500);
}