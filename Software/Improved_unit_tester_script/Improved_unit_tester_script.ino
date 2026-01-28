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
void printLCD(const char* line1, const char* line2 = nullptr, int waitMs = 0);
long readAdjustedStable();
void getCalibrationValues();
void setCalibrationValues();
long readSensorAdjusted();


HX711 scale;
LiquidCrystal_I2C lcd(0x27, 16, 2);


// Global variables
const int ADDRESS_CALIBRATION_SIGNATURE = 0;
// Arbitrary number that would be found in address ADDRESS_CALIBRATION_SIGNATURE of EEPROM if calibration set before at least once
const uint32_t CALIBRATION_SIGNATURE = 0xA5A5A5A5;       
const long THRESHOLD = 50000;                                      // adjust this during first set up
long zeroOffset;
int pressures[] = {60, 90, 120, 150, 180, 210, 240, 270};
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
  printLCD("Zeroing...");
  zeroOffset = scale.read_average(15);
  lcd.clear();

  calibrationAsk();
}

void calibrationAsk()
{
   int refreshRatePerSec = 10;
   int timeLeftInSec = 5;

   printLCD("Calibrate?");

   int timeLeft = refreshRatePerSec * timeLeftInSec;
   while(timeLeft--)
   {
      int intentFill, total;
      total = 7;

      long absAdjusted = readSensorAdjusted();
      
      int intentLevel = THRESHOLD / total;                         // value needed per intentLevel (ie. step or "#")

      intentFill = absAdjusted / intentLevel;
      
      timeLeftInSec = (int) ((timeLeft/refreshRatePerSec)+1); 
      // display timer
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
   lcd.clear();
}

void confirm(void (*functionPass)(), void (*functionFail)())
{
   int refreshRatePerSec = 10;
   int timeLeftConfirmInSec = 3;

   long absAdjusted = readSensorAdjusted();

   int timeLeftConfirm = refreshRatePerSec * timeLeftConfirmInSec;

   printLCD("Hold for", "sec to confirm");

   while (timeLeftConfirm-- && (absAdjusted >= THRESHOLD/2))
   {  
      absAdjusted = readSensorAdjusted();

      int seconds = ((timeLeftConfirm/refreshRatePerSec)+1);
      lcd.setCursor(9, 0);
      lcd.print("  ");                                             // Clear previous number
      lcd.setCursor(9, 0);
      lcd.print(seconds);

      delay(1000/refreshRatePerSec);
   }

   if (absAdjusted < THRESHOLD/2) 
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
   
   int i;
   // loop over desired pressures
   for (i = 0; i < refSize; i++)
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
      delay(5000);                                                 // delay to allow user to set the cuff to correct pressure

      printLCD("Collecting", "values...");
      curReading = readAdjustedStable();
      numTries += 1;
      // delay(2000);         

      while (curReading == 1 && numTries < maxTries)               // while the reading is invalid and user have not ran out of tries
      {
         numTries += 1;

         printLCD("Reading unstable!", "", 2000);

         printLCD("Set cuff", "to     mmHg");           
         lcd.setCursor(3, 1);
         lcd.print(curPressure);
         delay(5000);                                              // delay to allow user to set the cuff to correct pressure

         printLCD("Collecting", "values...");
         curReading = readAdjustedStable();
         // delay(2000);
      }
      
      if (numTries == maxTries)
      {
         printLCD("Unstable readings", "", 2000);

         printLCD("Reverting to pre", "-vious settings", 4000);

         // Disregard changes and set refValues[] back to previous settings
         getCalibrationValues();

         printLCD("Normal mode");
         
         break;
      } else if (curReading > previous) 
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
      // Arduino logic goes to loop() directly
   } else
   {
      // Disregard changes and set refValues[] back to previous settings
      getCalibrationValues();
   }
}

void printLCD(const char* line1, const char* line2, int waitMs)
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
   int samples = 20;

   long stabilityThreshold = THRESHOLD / 8;
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

   if ((maxVal - minVal) > stabilityThreshold)
   {
      return 1;                                                    // unstable
   }

   return sum / samples;                                           // If stable return average
}

void getCalibrationValues()
{
   uint32_t foundConfirmationNum = EEPROM.read(ADDRESS_CALIBRATION_SIGNATURE);

   if (foundConfirmationNum != CALIBRATION_SIGNATURE)
   {
      neverCalibratedBefore();
   }

   // Set refValues[] to values in EEPROM. Read in a loop.
   Serial.println("setting refValues[] to values in EEPROM");

   return;
}

void setCalibrationValues()
{
   uint32_t foundConfirmationNum = EEPROM.read(ADDRESS_CALIBRATION_SIGNATURE);

   // Avoid rewriting if signature written before. Do not ware out signature data addresses.
   if (foundConfirmationNum != CALIBRATION_SIGNATURE)
   {
      Serial.println("Writing to EEPROM! ");
      Serial.print("CALIBRATION_SIGNATURE: ");
      Serial.println(CALIBRATION_SIGNATURE);
      // write signature to address 0;
   }

   // Set EEPROM to new values in refValues[]. Write (be careful). 
   Serial.println("setting EEPROM to values in refValues[]");
   Serial.println("Writing to EEPROM! ");

   return;
}

float adjustedToPressure(long sensorValAdjusted)
{
   // If reading is below first calibration point, extrapolate
   if (sensorValAdjusted <= refValues[0]) {
      float slope = (float) (pressures[1] - pressures[0]) / (float) (refValues[1] - refValues[0]);
      return pressures[0] + slope * (sensorValAdjusted - refValues[0]);
   }

   // If reading is above last calibration point, extrapolate
   if (sensorValAdjusted >= refValues[refSize-1]) {
      float slope = (float) (pressures[refSize-1] - pressures[refSize-2]) / 
                     (float) (refValues[refSize-1] - refValues[refSize-2]);
      return pressures[refSize-1] + slope * (sensorValAdjusted - refValues[refSize-1]);
   }

   // Interpolation between two calibration points
   for (int i = 0; i < refSize-1; i++) {
      if (sensorValAdjusted >= refValues[i] && sensorValAdjusted <= refValues[i+1]) {
         float slope = (float) (pressures[i+1] - pressures[i]) / (float) (refValues[i+1] - refValues[i]);
         return pressures[i] + slope * (sensorValAdjusted - refValues[i]);
      }
   }
}

long readSensorAdjusted()
{
   float raw = scale.read();
   long adjusted = abs(raw - zeroOffset);
   return adjusted;
}

void neverCalibratedBefore()
{
   printLCD("Never calibrated", "before", 5000);
   calibrationAsk();
}

// TODO: Might convert code to a state machine
void loop()
{
   // Check if calibration values were set before
   // if not message that Unit tester never calibrated before. back to calibrationAsk(). Maybe x 3 times then go to inifinite loop doing nothing
   // if so, read values once and store them in code (array) to use.
   getCalibrationValues();

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

// setup()
   // initial setup
   // call CalibrationAsk()

// calibrateAsk()
   // Calibrate? No [||||   ] Yes. 5 sec. button push could be negative so use abs() and determine scale factor directionality.
   // If yes, confirm then go to calibration(). No confirm then go back to CalibrateAsk().
   // If no, check EEPROM for setUpConfirmationNumber 
   //             -> if foundConfirmationNum == setUpConfirmationNum then call SetCalibrationValues(), then loop()
   //             -> else call NotInitializedBefore()

// confirm()
   // Keep holding for 2 seconds. "Hold to confirm". 
   // -> success then go to Calibration()
   // -> else back to calibrate ask.

// calibrate() "calibration mode"
   // Instruction: Set cuff pressure to #.# mmHg. Stable? "save" readings : wait
   // Confirm reading is higher than 0 reading and previous reading (raw should increase every intentLevel) until all readings done.
   // Only if done successfully save (write) to EEPROM.
   // --> Store values in code (arrays). SetCalibrationValues()
   // --> Success message, then normal mode (ie. loop())
   // If failed then go back to CalibrateAsk()

// setCalibrationValues()
   // set EEPROM. Set values.
// 

// notInitializedBeforeWarning()
   // Through warning that the unit has not been initialized before with calibration data
   // call calibration ask.

// adjustedToPressure()
   // convert raw measurment to pressure using Pressures[] and rawValues[].
   // Interpolation in range, extrapolation outside range (outside not accurate)

// loop()
   // read raw values
   // call adjustedtoPressure() to get pressure
   // display pressure