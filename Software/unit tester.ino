/*
 This code uses bogde's library: https://github.com/bogde/HX711, 
 and LCD library: https://github.com/mathertel/LiquidCrystal_PCF8574
 bogde's library is released under a GNU GENERAL PUBLIC LICENSE
 
 Arduino pin 2 -> HX711 CLK
 3 -> DAT
 5V -> VCC
 GND -> GND
*/
#include <LiquidCrystal_PCF8574.h>

LiquidCrystal_PCF8574 lcd(0x27);
const int DT_PIN = 6;
const int SCK_PIN = 5;

#include "HX711.h"

#define calibration_factor 9000//

#define DOUT  3
#define CLK  2
float Force;
float M;
float mmhg;
HX711 scale;

void setup() {
  Serial.begin(9600);
  Serial.println("Unit tester");

  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.tare(); //Assuming there is no weight on the scale at start up, reset the scale to 0

  Serial.println("Readings:");

  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.clear();
}

void loop() {
  M = scale.get_units(10);
  Force = M * 9.8 *0.1;//0.1 for 100g to 1kg
  //put the value of S in the euqation below
  mmhg = Force /0.002423266110171/133.32;//133.32 factors for Pa to mmHg, 0.002423266110171 area for button. 
  //0.002423266110171 This number can be obtained by calibration factor S.m
  Serial.print("Force: ");
  Serial.print(Force); //scale.get_units() returns a float
  Serial.print(" N"); 
  Serial.println();

  lcd.clear();
  lcd.setCursor(0, 0);  //set the position of the cursor (word,line)
  lcd.print("Force:");
  lcd.setCursor(8, 0);

  lcd.print(Force,0);
  lcd.setCursor(13, 0);
  lcd.print("N");
  lcd.setCursor(0,1);
  lcd.print("Pressure:");
  lcd.setCursor(0,2);
  lcd.print(mmhg,0);
  lcd.setCursor(4,2);
  lcd.print("mmHg");
  
}
