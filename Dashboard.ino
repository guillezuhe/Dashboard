/*
CONTROLADOR DE DASHBOARD ARDUINO. Work in progress
Este código recibe los datos de SimHub y los envia al dashboard
El dashboard consiste en un display de 8 dígitos y 7 segmentos que muestra la marcha, la vuelta y el porcentaje de gasolina.
Las revoluciones se controlan mediante un led RGB que cambia de color según las revoluciones.
Las banderas se controlan mediante un led RGB que cambia de color según la bandera.

El display de 8 dígitos y 7 segmentos tiene tres modos, controlados por un botón:
- Modo 1: Muestra la marcha, la vuelta y el porcentaje de gasolina
- Modo 2: Muestra la marcha y el tiempo de vuelta
- Modo 3: Muestra la marcha y el delta respecto al mejor tiempo de vuelta

Video para la distribución de las marchas
https://www.youtube.com/watch?v=_knr0dHRixg&list=PL5QT34daNj2BPI0Rsjdg3WpJXgK8_UPrP&index=10

*/

/**********
 * LIBRARIES
 *********/
#include <LiquidCrystal_I2C.h>

/**********
 * VARIABLES
 *********/

int revPins[] = {3, 5, 6}; // Pins for the RGB LED that indicates the RPM
int flagPins[] = {1, 2, 4}; // Pins for the RGB LED that indicates the flag
// Shift register pins for the 7-segment display
int latchPin = 10;
int clockPin = 11;
int dataPin = 12;

// Another Shift register pins to control the rev leds separately
int latchPin2 = 7;
int clockPin2 = 8;
int dataPin2 = 9;

int i;

int freq = 10; // Frequency of the data sent by SimHub
int* colorRevs; // RGB color of the LED that indicates the RPM
int* colorFlag; // RGB color of the LED that indicates the flag
int  colorOff[] = {0, 0, 0}; // RGB color to turn off the LED

int ledRevsState = 0; // State of the LED that indicates the RPM
unsigned long previousMillis = 0; // Will store the last time the LED was updated
const long interval = 80; // Interval at which to blink the LED (milliseconds)

// Pullup button
int buttonPin = 13;

int nModes = 0;
int displayMode = 2; // Mode of the display
int buttonVal = HIGH;
int buttonValOld = HIGH;

LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD display


struct ParsedData {
  int speed;
  int gear;
  int rpmPC;
  bool pitLimiter;
  int lap;
  int totalLaps;
  int fuelLaps;
  int lapTimeH;
  int lapTimeMin;
  float lapTimeSec;
  float deltaSec;
  int pos;
  int opp;
  int BlackFlag;
  int YellowFlag;
  int GreenFlag;
  int BlueFlag;
  int WhiteFlag;
  int CheckeredFlag;
  int Flag; // 0: No flag, 1: Black, 2: Yellow, 3: Green, 4: Blue, 5: White, 6: Checkered
};

ParsedData parsedData;

// Numbers for the 7-segment display
byte numbers[11] = {
                  0b00101010, //n
                  0b01100000, 0b11011010, //1, 2
                  0b11110010, 0b01100110, //3, 4
                  0b10110110, 0b11111010, //5, 6
                  0b11100000, 0b11111110, //7, 8
                  0b11110110, 0b00001010  //9, r
                  
                  }; 



/**********
 * SETUP
 *********/

void setup() {
  Serial.begin(9600); // Initialize serial communication at 9600 baud rate
  for (i = 0; i < 3; i++) {
    pinMode(revPins[i], OUTPUT);
    pinMode(flagPins[i], OUTPUT);
  }

  // Initialize the 7-segment display
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  // Initialize the rev leds shift register
  pinMode(latchPin2, OUTPUT);
  pinMode(clockPin2, OUTPUT);
  pinMode(dataPin2, OUTPUT);

  // Initialize the button
  pinMode(buttonPin, INPUT_PULLUP);

  // Initialize the LCD display
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dashboard");
  lcd.setCursor(0, 1);
  lcd.print("guillezuhe");
  delay(2000);

}

/**********
 * LOOP
 *********/

void loop() {

  unsigned long currentMillis = millis();

  if (Serial.available() > 0) {
    /************
     * PARSE DATA
     * **********/
    String receivedData = Serial.readStringUntil('\n'); // Read data from SimHub
    // Serial.println(receivedData); // Send the received data back to SimHub
    parsedData = parseData(receivedData); // Parse the received data

    /************
     * GEAR
     * **********/
    // Display the gear on the 7-segment display
    setGear(parsedData.gear);

    /************
     * REVS COLOR
     * **********/

    //Blink the LED if the RPM is over the red line (98%)
    if (parsedData.rpmPC > 98) {
      // Blink the LED without delay
      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (ledRevsState == 0) {
          // setLedsRevs(0);
          setLedsRevsShifter(0);
          ledRevsState = 1;
        }
        else {
          // setLedsRevs(parsedData.rpmPC);
          setLedsRevsShifter(parsedData.rpmPC);
          ledRevsState = 0;
        }
      }
    }
    else{
      // setLedsRevs(parsedData.rpmPC);
      setLedsRevsShifter(parsedData.rpmPC);
    }
    
    /************
     * FLAG COLOR
     * **********/
    colorFlag = flagColor(parsedData.Flag); // Get the color of the LED based on the flag
    // Set the color of the LED
    setFlagColor(colorFlag);

    /************
     * DISPLAY
     * **********/
    checkButtonMode();
    setDisplay(displayMode, parsedData.lap, parsedData.totalLaps, parsedData.fuelLaps, parsedData.lapTimeH, parsedData.lapTimeMin, parsedData.lapTimeSec, parsedData.deltaSec, parsedData.pos, parsedData.opp);
  }
}





/************

FUNCTIONS

*************/

ParsedData parseData(String data) {
  // Parse the data received from SimHub
  ParsedData parsedData;

  // The data follows the following format:
  // '('+[SpeedKmh]+';'+[Gear]+';'+[CarSettings_CurrentDisplayedRPMPercent]+';'+[PitLimiterOn]+';'+[CurrentLap]+';'+[TotalLaps]+';'+[DataCorePlugin.Computed.Fuel_RemainingLaps]+';'+[CurrentLapTime]+';'+[PersistantTrackerPlugin.SessionBestLiveDeltaSeconds]+';'+[Position]+';'+[OpponentsCount]+';'+[Flag_Black]+';'+[Flag_Yellow]+';'+[Flag_Green]+';'+[Flag_Blue]+';'+[Flag_White]+';'+[Flag_Checkered]+')\n'
  parsedData.speed = data.substring(1, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  String gear_S = data.substring(0, data.indexOf(';'));
  
  if (gear_S == "N") {
    parsedData.gear = 0;
  }
  else if (gear_S == "R") {
    parsedData.gear = -1;
  }
  else {
    parsedData.gear = gear_S.toInt();
  }

  data = data.substring(data.indexOf(';') + 1);
  parsedData.rpmPC = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.pitLimiter = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.lap = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.totalLaps = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.fuelLaps = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.lapTimeMin = data.substring(0, data.indexOf(':')).toInt();
  data = data.substring(data.indexOf(':') + 1);
  parsedData.lapTimeMin = data.substring(0, data.indexOf(':')).toInt();
  data = data.substring(data.indexOf(':') + 1);
  parsedData.lapTimeSec = data.substring(0, data.indexOf(';')).toFloat();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.deltaSec = data.substring(0, data.indexOf(';')).toFloat();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.pos = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.opp = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.BlackFlag = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.YellowFlag = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.GreenFlag = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.BlueFlag = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.WhiteFlag = data.substring(0, data.indexOf(';')).toInt();
  data = data.substring(data.indexOf(';') + 1);
  parsedData.CheckeredFlag = data.substring(0, data.indexOf(')')).toInt();

  // Determine the flag
  if (parsedData.BlackFlag == 1) {
    parsedData.Flag = 1;
  }
  else if (parsedData.YellowFlag == 1) {
    parsedData.Flag = 2;
  }
  else if (parsedData.GreenFlag == 1) {
    parsedData.Flag = 3;
  }
  else if (parsedData.BlueFlag == 1) {
    parsedData.Flag = 4;
  }
  else if (parsedData.WhiteFlag == 1) {
    parsedData.Flag = 5;
  }
  else if (parsedData.CheckeredFlag == 1) {
    parsedData.Flag = 6;
  }
  else {
    parsedData.Flag = 0;
  }

  return parsedData;

}



int* revsColorRGB(int revsPC) {
  // Return the color of the LED based on the RPM
  static int color[3]; // RGB color
  if (revsPC < 75) {
    // LOW RPM - LED is OFF
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;
  }
  else if (revsPC < 85) {
    // MEDIUM RPM - LED is GREEN
    color[0] = 0;
    color[1] = 255;
    color[2] = 0;
  }
  else if (revsPC < 95) {
    // HIGH RPM - LED is YELLOW
    color[0] = 255;
    color[1] = 80;
    color[2] = 0;
  }
  /*
  else if (revsPC < 95) {
    // VERY HIGH RPM - LED is RED
    color[0] = 255;
    color[1] = 0;
    color[2] = 0;
  }
  */
  else {
    // MAX RPM - LED is purple or blinking red
    color[0] = 255;
    color[1] = 0;
    color[2] = 0;
  }

  return color;

}



void setRevsColorRGB(int color[]) {
  // Change the color of the LED that indicates the RPM
  for (i = 0; i < 3; i++) {
    analogWrite(revPins[i], color[i]);
  }
}



void setLedsRevs(int revsPC) {
  if (revsPC < 75) {
    // LOW RPM - LED is OFF
    digitalWrite(revPins[0], LOW);
    digitalWrite(revPins[1], LOW);
    digitalWrite(revPins[2], LOW);
  }
  else if (revsPC < 85) {
    // MEDIUM RPM - LED is GREEN
    digitalWrite(revPins[0], HIGH);
    digitalWrite(revPins[1], LOW);
    digitalWrite(revPins[2], LOW);
  }
  else if (revsPC < 95) {
    // HIGH RPM - LED is YELLOW
    digitalWrite(revPins[0], HIGH);
    digitalWrite(revPins[1], HIGH);
    digitalWrite(revPins[2], LOW);
  }
  else {
    // VERY HIGH RPM - LED is RED
    digitalWrite(revPins[0], HIGH);
    digitalWrite(revPins[1], HIGH);
    digitalWrite(revPins[2], HIGH);
  }
  

}



void setLedsRevsShifter(int revsPC) {
  // Set the number of leds turned on based on the RPM. If rpm PC is <75, no leds are turned on. If rpm PC is >95, all leds are turned on.
  // The leds are turned on in a sequence from left to right
  int numLedsOn = 0;
  if (revsPC < 75) {
    numLedsOn = 0;
  }
  else {
    numLedsOn = map(revsPC, 75, 95, 1, 8);
  }

  byte leds = 0;
  for (i = 0; i < numLedsOn; i++) {
    bitSet(leds, i);
  }


  digitalWrite(latchPin2, LOW);
  shiftOut(dataPin2, clockPin2, MSBFIRST, leds);
  digitalWrite(latchPin2, HIGH);
}



int* flagColor(int Flag) {
  // Return the color of the LED based on the flag
  // 0: No flag, 1: Black, 2: Yellow, 3: Green, 4: Blue, 5: White, 6: Checkered
  static int color[3]; // RGB color

  if (Flag == 1) {
    // BLACK FLAG - LED is BLACK
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;
  }
  else if (Flag == 2) {
    // YELLOW FLAG - LED is YELLOW
    color[0] = 255;
    color[1] = 80;
    color[2] = 0;
  }
  else if (Flag == 3) {
    // GREEN FLAG - LED is GREEN
    color[0] = 0;
    color[1] = 255;
    color[2] = 0;
  }
  else if (Flag == 4) {
    // BLUE FLAG - LED is BLUE
    color[0] = 0;
    color[1] = 0;
    color[2] = 255;
  }
  else if (Flag == 5) {
    // WHITE FLAG - LED is WHITE
    color[0] = 255;
    color[1] = 255;
    color[2] = 255;
  }
  else if (Flag == 6) {
    // CHECKERED FLAG - LED is PURPLE
    color[0] = 255;
    color[1] = 0;
    color[2] = 255;
  }
  else {
    // NO FLAG - LED is OFF
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;
  }

  return color;
}



void setFlagColor(int color[]) {
  // Change the color of the LED that indicates the flag
  for (i = 0; i < 3; i++) {
    analogWrite(flagPins[i], color[i]);
  }
}



void setGear(int gear) {
  // Display the gear in the 7-segment display
  // using the shift register
  
  if (gear == -1) {
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, LSBFIRST, numbers[10]);
    digitalWrite(latchPin, HIGH);
  }
  else {
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, LSBFIRST, numbers[gear]);
    digitalWrite(latchPin, HIGH);
  }
}



void checkButtonMode() {
  // Check if the button has been pressed
  buttonVal = digitalRead(buttonPin);
  // Debounce should be done
  if (buttonVal == LOW && buttonValOld == HIGH){
    changeDisplayMode();
  }
  buttonValOld = buttonVal;
}



void changeDisplayMode() {
  displayMode = (displayMode + 1) % nModes;
  lcd.clear();

  if (displayMode == 0){
    lcd.setCursor(0, 0);
    lcd.print("Crono mode 1");
    lcd.setCursor(0, 1);
    lcd.print("Lap time");
    delay(750);
    lcd.clear();
  }
  else if (displayMode == 1){
    lcd.setCursor(0, 0);
    lcd.print("Crono mode 2");
    lcd.setCursor(0, 1);
    lcd.print("Delta");
    delay(750);
    lcd.clear();
  }
  else if (displayMode == 2){
    lcd.setCursor(0, 0);
    lcd.print("Race mode 1");
    lcd.setCursor(0, 1);
    lcd.print("Position");
    delay(750);
    lcd.clear();
  }
}



void setDisplay(int mode, int lap, int totalLaps, int fuelLaps, int lapTimeH, int lapTimeMin, float lapTimeS, float deltaS, int pos, int opp) {
  if (mode == 0) {
    // Crono mode 1. Show lap time in the first line and lap and fuel in the second line
    lcd.setCursor(0, 0);
    lcd.print(lapTimeMin+60*lapTimeH);
    lcd.print(":");
    if (lapTimeS < 10) {
      lcd.print("0");
    }
    lcd.print(lapTimeS);

    // Complete the line with spaces
    lcd.print("              ");

    // Show lap and fuel in the second line
    lcd.setCursor(0, 1);
    lcd.print("L");
    lcd.print(lap);
    lcd.print("   F");
    lcd.print(fuelLaps);
    lcd.print("              ");
  }
  else if (mode == 1) {
    // Crono mode 2. Show delta in the first line and lap and fuel in the second line
    lcd.setCursor(0, 0);
    lcd.print(deltaS);
    lcd.print("              ");

    // Show lap and fuel in the second line
    lcd.setCursor(0, 1);
    lcd.print("L");
    lcd.print(lap);
    lcd.print("   F");
    lcd.print(fuelLaps);
    lcd.print("              ");

  }
  else if (mode == 2) {
    // Race mode. Show position/total and lap in the first line and fuel and gap to previous car in the second line
    lcd.setCursor(0, 0);
    lcd.print("P");
    lcd.print(pos);
    lcd.print("/");
    lcd.print(opp);
    lcd.print("  L");
    lcd.print(lap);
    lcd.print("/");
    lcd.print(totalLaps);
    lcd.print("              ");

    // Show fuel in the second line
    lcd.setCursor(0, 1);
    lcd.print("F");
    lcd.print(fuelLaps);
    lcd.print("              ");

  }
}