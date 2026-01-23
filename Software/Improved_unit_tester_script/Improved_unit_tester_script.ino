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
int pressure[] = {60, 90, 120, 150, 180, 210, 240, 270};
const int refSize = sizeof(pressure) / sizeof(pressure[0]);
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
      int curPressure = pressure[i];
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
      delay(2000);         

      while (curReading == 0 && numTries < maxTries)
      {
         numTries += 1;

         printLCD("Reading unstable!", "", 2000);

         printLCD("Set cuff", "to     mmHg");           
         lcd.setCursor(3, 1);
         lcd.print(curPressure);
         delay(5000);                                              // delay to allow user to set the cuff to correct pressure

         printLCD("Collecting", "values...");
         curReading = readAdjustedStable();
         delay(2000);
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

long readAdjustedStable()
{
   return readSensorAdjusted();
   // To Do: check if stable values. If not return 0.
}

void getCalibrationValues()
{
   // EEPROM.read(ADDRESS_CALIBRATION_SIGNATURE);
   return;
}

void setCalibrationValues()
{
   return;
}

float adjustedToPressure(long sensorValAdjusted)
{
   Serial.print(sensorValAdjusted);
}

long readSensorAdjusted()
{
   float raw = scale.read();
   long adjusted = abs(raw - zeroOffset);
   return adjusted;
}

// TODO: Might convert code to a state machine
void loop()
{
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("In loop()");

   // Check if calibration values were set before
   // if not message that Unit tester never calibrated before. back to calibrationAsk().
   // if so, read values once and store them in code (array) to use.

   // call adjustedToPressure()

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

// Issues: how do I make sure threshold is not too low during CalibrateAsk()? I do not want yes to be accidental. -> If threshold is too low calibration mode would start but not finish (assuming normal sensor fluctuations)



// void setup() {
//   Serial.begin(9600);
//   Serial.println("Unit tester");

//   scale.begin(DOUT, CLK);
//   scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
//   scale.tare(); //Assuming there is no weight on the scale at start up, reset the scale to 0

//   Serial.println("Readings:");

//   lcd.begin(16, 2);
//   lcd.setBacklight(255);
//   lcd.clear();
// }

// void loop() {
//   M = scale.get_units(10);
//   Force = M * 9.8 *0.1;//0.1 for 100g to 1kg
//   //put the value of S in the euqation below
//   mmhg = Force /0.002423266110171/133.32;//133.32 factors for Pa to mmHg, 0.002423266110171 area for button. 
//   //0.002423266110171 This number can be obtained by calibration factor S.m
//   Serial.print("Force: ");
//   Serial.print(Force); //scale.get_units() returns a float
//   Serial.print(" N"); 
//   Serial.println();

//   lcd.clear();
//   lcd.setCursor(0, 0);  //set the position of the cursor (word,line)
//   lcd.print("Force:");
//   lcd.setCursor(8, 0);

//   lcd.print(Force,0);
//   lcd.setCursor(13, 0);
//   lcd.print("N");
//   lcd.setCursor(0,1);
//   lcd.print("Pressure:");
//   lcd.setCursor(0,2);
//   lcd.print(mmhg,0);
//   lcd.setCursor(4,2);
//   lcd.print("mmHg");
  
// }
