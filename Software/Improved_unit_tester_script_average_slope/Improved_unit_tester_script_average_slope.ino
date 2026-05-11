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
void calibrationAsk();
void confirm(void (*functionPass)(), void (*functionFail)());
void drawBar(int intentFill, int total);
void calibrate();
float adjustedToPressure(long sensorValAdjusted);
void printLCD(const char* line1, const char* line2 = nullptr, unsigned long int waitMs = 0);
long readAdjustedStable();
void getCalibrationValues();
void setCalibrationValues();
long readSensorAdjusted();
bool checkNeverCalibratedBefore();


HX711 scale;
LiquidCrystal_I2C lcd(0x27, 16, 2);


// Global variables
const int ADDRESS_CALIBRATION_SIGNATURE = 0;
// Arbitrary number that would be found in address ADDRESS_CALIBRATION_SIGNATURE of EEPROM if calibration set before at least once
const uint32_t version_number = 0x001b;                          // version number to force new calibration on major update : 0.0.1-b
const uint32_t CALIBRATION_SIGNATURE = 0xA5A5A5A5 ^ version_number;       
const float THRESHOLD = 50;                                      // 50mmHg threshold so once it's calibrated it depends on the calibration and not the load cell
const float STABILIY_THRESHOLD = 0.07;
long zeroOffset;
int pressures[] = {0, 60, 90, 120, 150, 180, 210, 240, 270};
const int refSize = sizeof(pressures) / sizeof(pressures[0]);
long refValues[refSize];
float avg_slope;


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
   lcd.clear();
   
   bool calibrated_this_run = checkNeverCalibratedBefore();

   if (!calibrated_this_run) {
      getCalibrationValues();
      calibrationAsk();
   }
}

void calibrationAsk()
{
   int refreshRatePerSec = 10;
   int timeLeftInSec = 5;

   printLCD("Calibrate?");

   int timeLeft = refreshRatePerSec * timeLeftInSec;
   while(timeLeft--)
   {
      int intentFill, intentLevel, total;
      total = 7;

      long absAdjusted = readSensorAdjusted();

      float mmHg = adjustedToPressure(absAdjusted);
      
      intentLevel = THRESHOLD / total;                             // value needed per intentLevel (ie. step or "#").

      intentFill = mmHg / intentLevel;                      // number of steps (#) filled with the current press.
      
      timeLeftInSec = (int) ((timeLeft/refreshRatePerSec)+1); 
      // display time left
      lcd.setCursor(13, 0);
      lcd.print(timeLeftInSec);
      lcd.print("s ");

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
   int timeLeftConfirmInSec = 3;

   long absAdjusted = readSensorAdjusted();
   
   float mmHg = adjustedToPressure(absAdjusted);

   int timeLeftConfirm = refreshRatePerSec * timeLeftConfirmInSec;

   printLCD("Hold for", "sec to confirm");

   while (timeLeftConfirm-- && (mmHg >= THRESHOLD/2))
   {  
      absAdjusted = readSensorAdjusted();

      mmHg = adjustedToPressure(absAdjusted);

      int seconds = ((timeLeftConfirm/refreshRatePerSec)+1);
      lcd.setCursor(9, 0);
      lcd.print("  ");                                             // Clear previous number
      lcd.setCursor(9, 0);
      lcd.print(seconds);

      delay(1000/refreshRatePerSec);
   }

   if (mmHg < THRESHOLD/2) 
   {
      // call function for failed confirmation
      (*functionFail)(); 

   } else 
   {
      // call function for passed confirmation
      (*functionPass)(); 
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

void calibrate()
{
   printLCD("Calibration Mode", "", 3000);
   printLCD("Place cuff", "You have 1min", 60000);
   
   int i = 0;
   refValues[i] = 0;
   // loop over desired pressures
   for (i = 1; i < refSize; i++)
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
      delay(10000);                                                 // delay to allow user to set the cuff to correct pressure

      printLCD("Collecting", "values...");
      curReading = readAdjustedStable();
      numTries += 1;

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
      } else if (curReading > previous) 
      {
         refValues[i] = curReading;
         Serial.println(curReading);
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
      printLCD("Normal mode", "", 2000);
      getCalibrationValues();
      // Arduino logic goes to loop() directly
   } else
   {
      // Disregard changes and set refValues[] back to previous settings
      getCalibrationValues();
   }
}

void printLCD(const char* line1, const char* line2, unsigned long int waitMs)
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

// check if stable values. If not return 1.
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

   if ((float)(maxVal - minVal) / (float)avg > STABILIY_THRESHOLD)
   {
      return -1;                                                    // unstable
   }

   return avg;                                           // If stable return average
}

void getCalibrationValues()
{
   checkNeverCalibratedBefore();
   
   int curAddress = ADDRESS_CALIBRATION_SIGNATURE + sizeof(uint32_t);

   EEPROM.get(curAddress, avg_slope);
   Serial.print("Inverse of average slope ");
   Serial.println(1/avg_slope);
}

void setCalibrationValues()
{
   uint32_t foundConfirmationNum;
   EEPROM.get(ADDRESS_CALIBRATION_SIGNATURE, foundConfirmationNum);
   
   // compute average slope
   avg_slope = 0;
   float min_slope = (float)pressures[1] / (float)refValues[1];
   float max_slope = min_slope;
   for (int i = 1; i < refSize; i++)
   {
      float this_slope = (float)pressures[i] / (float)refValues[i];
      avg_slope += this_slope;
      min_slope = min(min_slope, this_slope);
      max_slope = max(max_slope, this_slope);
   }
   avg_slope /= refSize - 1;
   
   // check for max error tolerance
   if ((max_slope - min_slope) / avg_slope > STABILIY_THRESHOLD) {
     Serial.println("Error : calibration data too far off of linear !");
     Serial.println("Not writing to EEPROM! ");
     printLCD("Err : not", "linear !", 2000);
     printLCD("Calibration", "failed", 2000);
     printLCD("Reverting to pre", "-vious settings", 5000);
     
     getCalibrationValues();
   } else
   {
     // Set EEPROM to new values in refValues[]. Write (be careful). 
     Serial.println("setting EEPROM to avg_slope");
     Serial.println("Writing to EEPROM! ");

     int curAddress = ADDRESS_CALIBRATION_SIGNATURE + sizeof(uint32_t);
     
     EEPROM.put(curAddress, avg_slope);
     Serial.print("Inverse of average slope ");
     Serial.println(1/avg_slope);

     // Avoid rewriting if signature written before. Do not ware out signature data addresses.
     if (foundConfirmationNum != CALIBRATION_SIGNATURE)
     {
        Serial.println("Writing to EEPROM! ");
        Serial.print("First time setting up confirmation number");
        // write signature to signature address;
        EEPROM.put(ADDRESS_CALIBRATION_SIGNATURE, CALIBRATION_SIGNATURE);
     }
     
     printLCD("Calibration", "successful", 2000);
   }
}

float adjustedToPressure(long sensorValAdjusted)
{
   return sensorValAdjusted * avg_slope;
}

long readSensorAdjusted()
{
   float raw = scale.read();
   long adjusted = fabs(raw - zeroOffset);                         // fabs() is like abs() but for floats. raw is a float so the subtraction result is a float..
   return adjusted;
}

bool checkNeverCalibratedBefore()
{
   uint32_t foundConfirmationNum;
   EEPROM.get(ADDRESS_CALIBRATION_SIGNATURE, foundConfirmationNum);

   if (foundConfirmationNum != CALIBRATION_SIGNATURE)
   {
      printLCD("Never calibrated", "before", 5000);
      calibrate();
      checkNeverCalibratedBefore();                         //  recurse in case calibration fails

      return true;
   }

   return false;
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
