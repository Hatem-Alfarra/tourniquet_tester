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

// HX711 pins
#define DT_PIN  3
#define SCK_PIN 2

// functions prototypes
void setThreshold();
void calibrationAsk();
void confirm(void (*functionPass)(), void (*functionFail)());
bool isNeverCalibratedBefore();
void calibrate();
long readSensorAdjusted();
long readAdjustedStable();
void getCalibrationValues();
void setCalibrationValues();
float adjustedToPressure(long sensorValAdjusted);
void drawBar(int intentFill, int total);
void printLCD(const char* line1, const char* line2 = nullptr, unsigned long waitMs = 0);


HX711 scale;
LiquidCrystal_I2C lcd(0x27, 16, 2);


// Global variables
const int ADDRESS_CALIBRATION_SIGNATURE = 0;
// Arbitrary number that would be found in address ADDRESS_CALIBRATION_SIGNATURE of EEPROM if calibration set before at least once
const uint32_t CALIBRATION_SIGNATURE = 0xA5A5A5A5;       
long threshold;                                      
long zeroOffset;
int pressures[] = {0, 60, 90, 120, 150, 180, 210, 240, 270};
const int refSize = sizeof(pressures) / sizeof(pressures[0]);
long refValues[refSize];


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
   zeroOffset = scale.read_average(15);

   setThreshold();                                                  
   lcd.clear();

   // Uncomment the following 2 lines to reset calibration as if never calibrated before (for testing purposes).
   // uint32_t resetVal = 0x00000000;
   // EEPROM.put(ADDRESS_CALIBRATION_SIGNATURE, resetVal);
   
   calibrationAsk();
}

void setThreshold()
{
   int samples = 20;
   long sum = 0;
   long sensorValAdjusted = readSensorAdjusted();
   long maxVal = sensorValAdjusted;
   long minVal = sensorValAdjusted;

   for (int i = 0; i < samples; i++)
   {
      sensorValAdjusted = readSensorAdjusted();

      sum += sensorValAdjusted;
      maxVal = max(maxVal, sensorValAdjusted);
      minVal = min(minVal, sensorValAdjusted);
      

      delay(50);
   }  
   long avg = sum / samples;
   long noiseRange = maxVal - minVal;


   threshold = avg + noiseRange * 30 * 7;                          // Set threshold to be above the noise level. The multiplier can be changed based on testing.
}

void calibrationAsk()
{
   // If never calibrated before, go to calibration directly. Otherwise, ask user if they want to calibrate (change current calibration values).
   if (isNeverCalibratedBefore())
   {
      calibrate();
      return;
   }

   // Load calibration values from EEPROM if calibrated before.
   getCalibrationValues();

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


      int total = 7;
      long intentFill, intentLevel;

      long sensorValAdjusted = readSensorAdjusted();
      
      intentLevel = threshold / total;                             // value needed per intentLevel (ie. step or "#").
      intentFill = sensorValAdjusted / intentLevel;                // number of steps (#) filled with the current press.
      
      drawBar(intentFill, total);

      if (intentFill >= total)
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
         break;
      }

      timeLeftInSec = (int) ((timeLeft/refreshRatePerSec)+1); 
      lcd.setCursor(9, 0);
      lcd.print("  ");                                             // Clear previous number
      lcd.setCursor(9, 0);
      lcd.print(timeLeftInSec);

      delay(1000/refreshRatePerSec);
   }

   if (sensorValAdjusted < threshold/2) 
   {
      // call function for failed confirmation
      (*functionFail)(); 

   } else 
   {
      // call function for passed confirmation
      (*functionPass)(); 
   }
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

void calibrate()
{
   printLCD("Calibration Mode", "", 3000);
   // printLCD("Place cuff", "You have 30s", 30000);                  // TODO: decrement on screen
   
   // loop over desired pressures
   // i is declared outside the loop because it is used after the loop to check if all reference values were collected successfully.
   int i;
   for (i = 0; i < refSize; i++)
   {
      int curPressure = pressures[i];
      long curReading;

      int maxTries = 3;
      int numTries = 1;

      long previous;

      if (i == 0) 
      {
         refValues[i] = 0;
         continue;
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
      } else if (curReading > previous)                            // desired behavior during calibration.
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
   if (i == refSize)
   {   
      // write to EEPROM the new ref value (from refValues[])
      setCalibrationValues();
      printLCD("Calibration", "successful", 2000);
      printLCD("Normal mode", "", 2000);
      getCalibrationValues();
      // Arduino logic goes to loop() directly
   } else
   {
      // Disregard changes and set refValues[] back to previous settings
      getCalibrationValues();
   }
}

long readSensorAdjusted()
{
   float raw = scale.read();
   long adjusted = fabs(raw - zeroOffset);                         // fabs() is like abs() but for floats. raw is a float so the subtraction result is a float..
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

void getCalibrationValues()
{
   if (isNeverCalibratedBefore())
   {
      calibrate();
      return;
   }  
   
   int curAddress = ADDRESS_CALIBRATION_SIGNATURE + sizeof(uint32_t);

   for (int i = 0; i < refSize; i++)
   {
      EEPROM.get(curAddress, refValues[i]);
      curAddress += sizeof(refValues[i]);
      Serial.print("Data point ");
      Serial.print(i+1);
      Serial.print(": ");
      Serial.println(refValues[i]);
   }
}

void setCalibrationValues()
{
   uint32_t foundConfirmationNum;
   EEPROM.get(ADDRESS_CALIBRATION_SIGNATURE, foundConfirmationNum);
   
   
   // Set EEPROM to new values in refValues[]. Write (be careful). 
   Serial.println("setting EEPROM to values in refValues[]");
   Serial.println("Writing to EEPROM! ");

   int curAddress = ADDRESS_CALIBRATION_SIGNATURE + sizeof(uint32_t);

   for (int i = 0; i < refSize; i++)
   {
      EEPROM.put(curAddress, refValues[i]);
      curAddress += sizeof(refValues[i]);
      Serial.print("Data point ");
      Serial.print(i+1);
      Serial.print(": ");
      Serial.println(refValues[i]);
   }

   // Avoid rewriting if signature written before. Do not ware out signature data addresses.
   if (foundConfirmationNum != CALIBRATION_SIGNATURE)
   {
      Serial.println("Writing to EEPROM! ");
      Serial.println("First time setting up confirmation number");
      // write signature to signature address;
      EEPROM.put(ADDRESS_CALIBRATION_SIGNATURE, CALIBRATION_SIGNATURE);
   }

}

float adjustedToPressure(long sensorValAdjusted)
{
   // If reading is below first calibration point, extrapolate
   if (sensorValAdjusted <= refValues[0]) 
   {
      float slope = (float) (pressures[1] - pressures[0]) / (float) (refValues[1] - refValues[0]);
      return pressures[0] + slope * (sensorValAdjusted - refValues[0]);
   }

   // If reading is above last calibration point, extrapolate
   if (sensorValAdjusted >= refValues[refSize-1])
   {
      float slope = (float) (pressures[refSize-1] - pressures[refSize-2]) / 
                     (float) (refValues[refSize-1] - refValues[refSize-2]);
      return pressures[refSize-1] + slope * (sensorValAdjusted - refValues[refSize-1]);
   }

   // Interpolation between two calibration points
   for (int i = 0; i < refSize-1; i++) {
      if (sensorValAdjusted >= refValues[i] && sensorValAdjusted <= refValues[i+1]) 
      {
         float slope = (float) (pressures[i+1] - pressures[i]) / (float) (refValues[i+1] - refValues[i]);
         return pressures[i] + slope * (sensorValAdjusted - refValues[i]);
      }
   }
}

void drawBar(int intentFill, int total)                         
{
   lcd.setCursor(0, 1);
   lcd.print("No [");
   for (int i=0; i < total; i++)
   {
      if (i < intentFill)
      {
         lcd.print("#");                                           // Intent bar can later be changed to look prettier
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

   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Pressure:");
   lcd.setCursor(0, 1);
   lcd.print(mmHg, 1);
   lcd.print("mmHg");

    

   delay(500);
}
