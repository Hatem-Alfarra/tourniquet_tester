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

// functions prototypes
void calibrationAsk();
void confirm(void (*functionPass)(), void (*functionFail)());
void drawBar(int, int);
void calibrate();



HX711 scale;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// HX711 pins
#define DT_PIN  3
#define SCK_PIN 2

// Global variables
const int ADDRESS_CALIBRATION_SIGNATURE = 0;
// Arbitrary number that would be found in address ADDRESS_CALIBRATION_SIGNATURE of EERPOM if calibration set before at least once
const uint32_t CALIBRATION_SIGNATURE = 0xA5A5A5A5;       

const long THRESHOLD = 50000;                       // adjust this during first set up
long zeroOffset;
bool isPositiveDirectionality;

bool isCalibrateMode = false;


// functions

void setup() {
  
  Serial.begin(9600);

  // Initialize HX711
  scale.begin(DT_PIN, SCK_PIN);

   // Initialize LCD
  lcd.init();
  lcd.backlight();
  // Collect offset
  lcd.setCursor(0, 0);
  lcd.print("Zeroing...");
  zeroOffset = scale.read_average(15);
  lcd.clear();

  calibrationAsk();
}

void calibrationAsk(){
   int refreshRatePerSec = 10;
   int timeLeftInSec = 5;

   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Calibrate?");

   int timeLeft = refreshRatePerSec * timeLeftInSec;
   while(timeLeft--){
      int intentFill, total;
      total = 7;

      float raw = scale.read();
      long adjusted = raw - zeroOffset;
      long absAdjusted = abs(adjusted);

      // CHANGE: Move to later in the code. Directionality determined at the threshold and not by fluctuations
      if (adjusted >= 0){
         isPositiveDirectionality = true;
      } else { isPositiveDirectionality = false; }
      
      int intentLevel = THRESHOLD / total;          // value needed per intentLevel (ie. step or "#")

      intentFill = absAdjusted / intentLevel;
      
      timeLeftInSec = (int) ((timeLeft/refreshRatePerSec)+1);
      // timer
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

void confirm(void (*functionPass)(), void (*functionFail)()){
   int refreshRatePerSec = 10;
   int timeLeftConfirmInSec = 3;

   float raw = scale.read();
   long adjusted = raw - zeroOffset;
   long absAdjusted = abs(adjusted);
   
   int timeLeftConfirm = refreshRatePerSec * timeLeftConfirmInSec;
   while (timeLeftConfirm-- && (absAdjusted >= THRESHOLD/2))
   {  
      raw = scale.read();
      adjusted = raw - zeroOffset;
      absAdjusted = abs(adjusted);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Hold for ");
      lcd.print(((timeLeftConfirm/refreshRatePerSec)+1));
      lcd.setCursor(0, 1);
      lcd.print("sec to confirm");
      delay(1000/refreshRatePerSec);
   }
   if (absAdjusted < THRESHOLD/2) 
   {
       (*functionFail)(); 
   } else {
       (*functionPass)(); 
   }
}

void drawBar(int intentFill, int total){
   lcd.setCursor(0, 1);
   lcd.print("No [");
   for (int i=0; i < total; i++)
   {
      if (i < intentFill)
      {
         lcd.print("#");                            // Intent bar can later be changed to look prettier
      } 
      else 
      {
         lcd.print(" ");
      }
   }
   lcd.print("] Yes");
}


void calibrate(){
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Calibration Mode");

   // loop over desired pressures
   // check if value is valid and if so store each in code
   // if all values are valid setCalibrationValues()
   // set calibration signiture if not set already

}

float adjustedToPressure(long sensorValAdjusted){
   Serial.print(sensorValAdjusted);
}

// TODO: Might convert code to a state machine
void loop(){
   lcd.setCursor(0, 0);
   lcd.print("In loop()");
   // Check if calibration values were set before
   // if not message that Unit tester never calibrated before. back to calibrationAsk().
   // if so, read values once and store them in code (array) to use.

   // call adjustedToPressure()

}

// setup()
   // initial setup
   // call CalibrationAsk()

// calibrateAsk()
   // Calibrate? No [||||   ] Yes. 5 sec. button push could be negative so use abs() and determine scale factor directionality.
   // If yes, confirm then go to calibration(). No confirm then go back to CalibrateAsk().
   // If no, check EERPOM for setUpConfirmationNumber 
   //             -> if foundConfirmationNum == setUpConfirmationNum then call SetCalibrationValues(), then loop()
   //             -> else call NotInitializedBefore()

// confirm()
   // Keep holding for 2 seconds. "Hold to confirm". 
   // -> success then go to Calibration()
   // -> else back to calibrate ask.

// calibrate() "calibration mode"
   // Instruction: Set cuff pressure to #.# mmHg. Stable? "save" readings : wait
   // Confirm reading is higher than 0 reading and previous reading (raw should increase every intentLevel) until all readings done.
   // Only if done successfully save (write) to EERPOM.
   // --> Store values in code (arrays). SetCalibrationValues()
   // --> Success message, then normal mode (ie. loop())
   // If failed then go back to CalibrateAsk()

// setCalibrationValues()
   // set EERPOM. Set values.
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

// Issues: how do I make sure threshold is not too low during CalibrateAsk()? I do not want yes to be accidental (maybe very unlikely)



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
