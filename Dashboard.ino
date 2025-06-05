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

int flagPins[] = {3, 5, 6}; // Pins for the RGB LED that indicates the flag
// Shift register pins for the 7-segment display
int latchPin = 8;
int clockPin = 7;
int dataPin = 9;

// Another Shift register pins to control the rev leds separately
int latchPin2 = 11;
int clockPin2 = 10;
int dataPin2 = 12;

unsigned long previousMillis = 0; // Will store the last time the LED was updated
const long interval = 80; // Interval at which to blink the LED (milliseconds)

// Flag LED colors
int freq = 10; // Frequency of the data sent by SimHub
int* colorRevs; // RGB color of the LED that indicates the RPM
int* colorFlag; // RGB color of the LED that indicates the flag
int  colorOff[] = {0, 0, 0}; // RGB color to turn off the LED

// Pullup button
int buttonPin = 2;

// LCD display
int nModes = 3;
int displayMode = 2; // Mode of the display
int buttonVal = HIGH;
int buttonValOld = HIGH;

LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD display

// Boxes LED
int pinBoxes = 4;

int i;

struct ParsedData {
  float speed;
  int gear;
  float rpmPC;
  bool pitLimiter;
  int lap;
  int totalLaps;
  float fuelLaps;
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
  bool DRS_av;
  bool DRS_act; // DRS available and active
  int prev_lapTimeH;
  int prev_lapTimeMin;
  float prev_lapTimeSec;
};

// Numbers for the 7-segment display
byte numbers[11] = {
                  0b00101010, //n
                  0b01100000, 0b11011010, //1, 2
                  0b11110010, 0b01100110, //3, 4
                  0b10110110, 0b10111110, //5, 6
                  0b11100000, 0b11111110, //7, 8
                  0b11110110, 0b00001010  //9, r
                  }; 


// Serial buffer
#define BUFFER_SIZE 256
char serialBuffer[BUFFER_SIZE];
int bufferIndex = 0;

ParsedData parsedData;


/**********
 * SETUP
 *********/

void setup() {
  Serial.begin(115200); // Initialize serial communication at 9600 baud rate
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

  // Initialize the Boxes LED
  pinMode(pinBoxes, OUTPUT);

  // Initialize the LCD display
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dashboard");
  lcd.setCursor(0, 1);
  lcd.print("guillezuhe");
  //delay(2000);

}

/**********
 * LOOP
 *********/

void loop() {

  unsigned long currentMillis = millis();

  while (Serial.available() > 0) {
    /************
     * PARSE DATA
     * **********/
    // The data follows the following format:
    // '('+[SpeedKmh]+';'+[Gear]+';'+[CarSettings_CurrentDisplayedRPMPercent]+';'+[PitLimiterOn]+';'+[CurrentLap]+';'+[TotalLaps]+';'+[DataCorePlugin.Computed.Fuel_RemainingLaps]+';'+[CurrentLapTime]+';'+[PersistantTrackerPlugin.SessionBestLiveDeltaSeconds]+';'+[Position]+';'+[OpponentsCount]+';'+[Flag_Black]+';'+[Flag_Yellow]+';'+[Flag_Green]+';'+[Flag_Blue]+';'+[Flag_White]+';'+[Flag_Checkered]+')\n'
    char incomingByte = Serial.read();

    // Check for buffer overflow
    if (bufferIndex < BUFFER_SIZE - 1) {
      serialBuffer[bufferIndex++] = incomingByte;
    }
    
    
    if (incomingByte == '\n') {
      serialBuffer[bufferIndex] = '\0';  // Null-terminate the buffer
      parsedData = parseData(serialBuffer);  // Parse the buffer

      /************
       * GEAR
       * **********/
      // Display the gear on the 7-segment display
      setGear(parsedData.gear);

      /************
       * REVS COLOR
       * **********/

      // Call the refactored setLedsRevsShifter function
      setLedsRevsShifter(parsedData.rpmPC, currentMillis);

      /************
       * PIT LIMITER
       * **********/
      // If the pit limiter is on, turn on the Boxes LED
      setLedPitLimiter(parsedData.pitLimiter);

      /************
       * DRS
       * **********/

      
      /************
       * FLAG COLOR
       * **********/
      colorFlag = flagColor(parsedData.Flag); // Get the color of the LED based on the flag
      // Set the color of the LED
      setFlagColor(colorFlag, parsedData.Flag, currentMillis);

      /************
       * DISPLAY
       * **********/
      checkButtonMode();
      setDisplay(displayMode, parsedData.lap, parsedData.totalLaps, parsedData.fuelLaps, parsedData.lapTimeH, parsedData.lapTimeMin, parsedData.lapTimeSec, parsedData.deltaSec, parsedData.pos, parsedData.opp);
    
      bufferIndex = 0; // Reset the buffer index
    }
  }

  setLedsRevsShifter(parsedData.rpmPC, currentMillis);
}





/************

FUNCTIONS

*************/

ParsedData parseData(char* data) {
  ParsedData parsedData;

  // Use strtok() to tokenize the string by ';' and ':' delimiters
  char* token;

  // Skip the initial '('
  token = strtok(data + 1, ";");  // First token (Speed)
  parsedData.speed = atof(token);  // Convert to float

  // Next token (Gear)
  token = strtok(NULL, ";");
  if (strcmp(token, "N") == 0) {
    parsedData.gear = 0;
  }
  else if (strcmp(token, "R") == 0) {
    parsedData.gear = -1;
  }
  else {
    parsedData.gear = atoi(token);  // Convert to integer
  }

  // RPM Percent
  token = strtok(NULL, ";");
  parsedData.rpmPC = atof(token);

  // Pit Limiter
  token = strtok(NULL, ";");
  parsedData.pitLimiter = atoi(token);

  // Lap
  token = strtok(NULL, ";");
  parsedData.lap = atoi(token);

  // Total Laps
  token = strtok(NULL, ";");
  parsedData.totalLaps = atoi(token);

  // Fuel Laps
  token = strtok(NULL, ";");
  parsedData.fuelLaps = atof(token);

  // Lap Time (Hours, Minutes, Seconds)
  token = strtok(NULL, ":");
  parsedData.lapTimeH = atoi(token);  // Hours

  token = strtok(NULL, ":");
  parsedData.lapTimeMin = atoi(token);  // Minutes

  token = strtok(NULL, ";");
  parsedData.lapTimeSec = atof(token);  // Seconds

  // Delta Seconds
  token = strtok(NULL, ";");
  parsedData.deltaSec = atof(token);

  // Position
  token = strtok(NULL, ";");
  parsedData.pos = atoi(token);

  // Opponents Count
  token = strtok(NULL, ";");
  parsedData.opp = atoi(token);

  // Flags
  parsedData.BlackFlag = atoi(strtok(NULL, ";"));
  parsedData.YellowFlag = atoi(strtok(NULL, ";"));
  parsedData.GreenFlag = atoi(strtok(NULL, ";"));
  parsedData.BlueFlag = atoi(strtok(NULL, ";"));
  parsedData.WhiteFlag = atoi(strtok(NULL, ";"));
  parsedData.CheckeredFlag = atoi(strtok(NULL, ")"));  // Final flag before ')'

  // TODO: Get DRS availability and activation status, and previous lap time

  // Determine the flag based on priority
  if (parsedData.BlackFlag == 1) {
    parsedData.Flag = 1;
  } else if (parsedData.YellowFlag == 1) {
    parsedData.Flag = 2;
  } else if (parsedData.GreenFlag == 1) {
    parsedData.Flag = 3;
  } else if (parsedData.BlueFlag == 1) {
    parsedData.Flag = 4;
  } else if (parsedData.WhiteFlag == 1) {
    parsedData.Flag = 5;
  } else if (parsedData.CheckeredFlag == 1) {
    parsedData.Flag = 6;
  } else {
    parsedData.Flag = 0;
  }

  return parsedData;
}



void setLedsRevsShifter(float revsPC, unsigned long currentMillis) {
  static unsigned long previousMillis = 0; // Store the last time the LEDs were updated
  static int ledRevsState = 0; // State of the LEDs
  const long interval = 80; // Interval at which to blink the LEDs (milliseconds)

  // Determine the number of LEDs to turn on based on RPM percentage
  int numLedsOn = 0;
  if (revsPC < 75) {
    numLedsOn = 0;
  } else {
    numLedsOn = map(revsPC, 72, 95, 0, 8);
  }

  // Handle blinking if RPM is over the red line (98%)
  if (revsPC > 97) {
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      ledRevsState = !ledRevsState; // Toggle the LED state
    }
  } else {
    ledRevsState = 1; // Ensure LEDs are steady when not blinking
  }

  // Set the LEDs based on the current state
  byte leds = 0;
  if (ledRevsState) {
    for (int i = 0; i < numLedsOn; i++) {
      bitSet(leds, i);
    }
  }


  digitalWrite(latchPin2, LOW);
  shiftOut(dataPin2, clockPin2, MSBFIRST, leds);
  digitalWrite(latchPin2, HIGH);
}


void setLedDRS(bool DRS_av, bool DRS_act, unsigned long currentMillis) {
  // Set the LED for the DRS
  // If DRS is available, blink the LED
  // If DRS is active, turn on the LED
  if (DRS_av) {
    if (DRS_act) {
      digitalWrite(pinBoxes, HIGH); // Turn on the LED
    } else {
      // Blink the LED
      static unsigned long previousMillis = 0;
      const long interval = 500; // Blink interval in milliseconds

      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        digitalWrite(pinBoxes, !digitalRead(pinBoxes)); // Toggle the LED state
      }
    }
  } else {
    digitalWrite(pinBoxes, LOW); // Turn off the LED
  }
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



void setFlagColor(int color[], int Flag, unsigned long currentMillis) {
  // Change the color of the LED that indicates the flag
  if (Flag == 2) { // Yellow flag. Make the LED blink
    static unsigned long previousMillis = 0; // Store the last time the LED was updated
    const long interval = 500; // Interval at which to blink the LED (milliseconds)

    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      // Toggle the LED state for yellow color
      if (color[0] == 0 && color[1] == 0 ) { //&& color[2] == 0 Is not necessary, but added for clarity
        color[0] = 255; // Red component
        color[1] = 80;  // Green component
        color[2] = 0;   // Blue component
      } else {
        color[0] = 0;
        color[1] = 0;
        color[2] = 0;
      }
    }
  }

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


void setLedPitLimiter(bool pitLimiter) {
  // Set the LED for the pit limiter
  if (pitLimiter) {
    digitalWrite(pinBoxes, HIGH);
  }
  else {
    digitalWrite(pinBoxes, LOW);
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
  Serial.print("Display mode: ");
  Serial.println(displayMode);

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



void setDisplay(int mode, int lap, int totalLaps, float fuelLaps, int lapTimeH, int lapTimeMin, float lapTimeS, float deltaS, int pos, int opp, float prevLap_time) {
  // TODO: Correct previous lap time format
  static int prevLap = 1; // Previous lap to detect lap changes
  static unsigned long lastLapDisplayTime = 0; // Time when the last lap time started displaying
  static bool showingLastLapTime = false; // Flag to indicate if last lap time is being displayed

  unsigned long currentMillis = millis();

  // Check if the lap changed
  if (lap != prevLap) {
    prevLap = lap;
    lastLapDisplayTime = currentMillis;
    showingLastLapTime = true;
  }

  // Show the last lap time for 3 seconds if needed
  if (showingLastLapTime && currentMillis - lastLapDisplayTime <= 3000) {
    lcd.setCursor(0, 0);
    lcd.print("Last Lap:");
    lcd.setCursor(0, 1);
    lcd.print(prevLap_time, 2); // Display the last lap time with 2 decimal places
    lcd.print("              "); // Clear the rest of the line
    return; // Exit the function to avoid overwriting the display
  } else {
    showingLastLapTime = false; // Stop showing the last lap time after 3 seconds
  }

  if (mode == 0) {
    // Crono mode 1. Show lap time in the first line and lap and fuel in the second line
    lcd.setCursor(0, 0);
    lcd.print(lapTimeMin + 60 * lapTimeH);
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
    if (deltaS > 0) {
      lcd.print("+");
    }
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