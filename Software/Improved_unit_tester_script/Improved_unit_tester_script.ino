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


HX711 scale;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// HX711 pins
#define DT_PIN  3
#define SCK_PIN 2

void setup() {
  
  Serial.begin(9600);

   // Initialize LCD
  lcd.init();
  lcd.backlight();
  // Tare / Zero out scale
  lcd.setCursor(0, 0);
  lcd.print("Zeroing...")
  scale.tare(20);
  


}

// setup()
   // initial setup
   // call CalibrationAsk()

// CalibrateAsk()
   // Calibrate? No [||||   ] Yes. 5 sec. button push could be negative so use abs() and determine scale factor directionality.
   // If yes, confirm then go to calibration(). No confirm then go back to CalibrateAsk().
   // If no, check EERPOM for setUpConfirmationNumber 
   //             -> if foundConfirmationNum == setUpConfirmationNum then call SetCalibrationValues(), then loop()
   //             -> else call NotInitializedBefore()

// Confirm()
   // Keep holding for 2 seconds. "Hold to confirm"
   // -> success then go to Calibration()
   // -> else back to calibrate ask.

// Calibration() "Calibration mode"
   // Instruction: Set cuff pressure to #.# mmHg. Stable? "save" readings : wait
   // Confirm reading is higher than 0 reading and previous reading (raw should increase every step) until all readings done.
   // Only if done successfully save (write) to EERPOM.
   // --> Store values in code (arrays). SetCalibrationValues()
   // --> Success message, then normal mode (ie. loop())
   // If failed then go back to CalibrateAsk()

// SetCalibrationValues()
   // read from EERPOM. Set values.
// 

// NotInitializedBefore()
   // Through warning that the unit has not been initialized before with calibration data
   // call calibration ask.

// RawToPressure()
   // convert raw measurment to pressure using Pressures[] and rawValues[].
   // Interpolation in range, extrapolation outside range (outside not accurate)

// loop()
   // read raw values
   // call rawtoPressure() to get pressure
   // display pressure

// Issues how do I make sure threshold is not too low during CalibrateAsk()? I do not want yes to be accidental (maybe very unlikely)



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
