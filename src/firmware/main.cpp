/**
 * MIDI Show Control
 * Arduino firmware v0.0.1
 *
 * This file contains the code that processes received commands and generally
 * manages the Arduino.
 *
 * Last modified March 25, 2015
 *
 * Copyright (C) 2015. All Rights Reserved.
 */

/******************************************************************************
 * Includes
 ******************************************************************************/

#include "Arduino.h"
#include "LiquidCrystal.h"
#include "MIDI.h"
#include "midi_Namespace.h"
#include "msc.h"

/******************************************************************************
 * Internal constants
 ******************************************************************************/

//MIDI
//https://www.sparkfun.com/products/9598
#define MIDI_IN_PIN                 0
#define MIDI_OUT_PIN                1
#define MAX_PACKET_LENGTH           128 //bytes

//LCD
//http://learn.adafruit.com/character-lcds
#define LCD_RED_BACKLIGHT_PIN       6
#define LCD_GREEN_BACKLIGHT_PIN     5
#define LCD_BLUE_BACKLIGHT_PIN      3
#define LCD_CONTROL_PIN             7
#define LCD_ENABLE_PIN              8
#define LCD_DATA_BIT_4_PIN          9
#define LCD_DATA_BIT_5_PIN          10
#define LCD_DATA_BIT_6_PIN          11
#define LCD_DATA_BIT_7_PIN          12
#define LCD_COLUMNS                 20
#define LCD_ROWS                    4
#define SYSEX_FLASH_TIME            1000 //milliseconds

//Colors (0xRRGGBB)
#define BACKLIGHT_OFF               0x000000
#define RED                         0xff0000
#define GREEN                       0x00ff00
#define BLUE                        0x0000ff
#define WHITE                       0xff9090 //(calibrated)

//Pushbutton
#define BUTTON_PIN                  2
#define DEBOUNCE_TIME               300 //milliseconds
#define NORMALLY_OPEN               0
#define NORMALLY_CLOSED             1

//Compile-time options
#define BUTTON_MODE                 NORMALLY_OPEN //How the button is wired
#define USE_MIDI                    1 //1 to use MIDI, 0 to use serial

//Relay options
#define RELAY_PIN A2
#define HAS_RELAY

//Backlight Fader
#define BL_POT 1
#define HAS_BL_POT

//Constants - do not modify
#if BUTTON_MODE == NORMALLY_OPEN
#define BUTTON_DOWN                 RISING
#define BUTTON_UP                   FALLING
#else
#define BUTTON_DOWN                 FALLING
#define BUTTON_UP                   RISING
#endif

#define BTN_LOW 20
#define BTN_HIGH 50
#define BTN_MAX 70

unsigned int button_ctr = 0;
unsigned int button_state = 0;
long lastButtonPress = 0;

unsigned int enable_button = 0;

//String constants for MSC command types
char* invalid_str = "INVALID";
const char* msc_command_type_strings[] = {
  invalid_str,
  "GO",
  "STOP",
  "RESUME",
  "TIMED GO",
  "LOAD",
  "SET",
  "FIRE",
  "ALL OFF",
  "RESTORE",
  "RESET",
  "GO-OFF",
  invalid_str,
  invalid_str,
  invalid_str,
  invalid_str,
  invalid_str,
  "STANDBY+",
  "STANDBY-",
  "SEQUENCE+",
  "SEQUENCE-",
};

/******************************************************************************
 * Internal function prototypes
 ******************************************************************************/

void setupLCD();
void updateLCD(MSC packet);
void displayCue(char* cue);
void displayList(char* list);
void displayType(TYPE type);
void displayID(byte id);
void displayCommand(COMMAND cmd);
void displayPacket(const byte* data, int len);

void setBacklight(int red, int green, int blue);
void setBacklight(long rgb);

void setupButton();
void buttonInterrupt();
void pauseMIDI();
void passMIDI();

void lcdPrintHex(byte c);

/******************************************************************************
 * Internal global variables
 ******************************************************************************/

//Create the global LCD object
LiquidCrystal LCD(LCD_CONTROL_PIN,
                  LCD_ENABLE_PIN,
                  LCD_DATA_BIT_4_PIN,
                  LCD_DATA_BIT_5_PIN,
                  LCD_DATA_BIT_6_PIN,
                  LCD_DATA_BIT_7_PIN);

//Create the global MIDI object
#if USE_MIDI
MIDI_CREATE_DEFAULT_INSTANCE();
#endif

//Whether MIDI passthrough is paused or not
volatile bool paused = false;

/******************************************************************************
 * Function definitions
 ******************************************************************************/

/**
 *  Sets up MIDI, the LCD, and the button
 */
void setup() {

  pinMode(A5, INPUT_PULLUP);
#if USE_MIDI
  MIDI.begin();
#else
  Serial.begin(115200);
#endif

  #ifdef HAS_RELAY
    pinMode(RELAY_PIN,OUTPUT); // If we have a relay, set its control pin to be output
  #endif

  setupLCD();

  passMIDI();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

/**
 *  Checks for new MIDI data, parses it, and updates the LCD accordingly
 */
void loop() {
  static long lastSysExTime = 0;
  static bool lcdIsBlue = false;

  enable_button = digitalRead(A5);

  // Progress the button debounce
  if(digitalRead(BUTTON_PIN) == LOW) { // is pressed
    if(button_ctr < BTN_MAX) { // is less than max
      button_ctr++;
    }
  } else { // is not pressed
    if(button_ctr > 0) { // is greater than 0
      button_ctr--;
    }
  }

  if(enable_button) {
    if(button_ctr < BTN_LOW && button_state == 1) { // button was pressed and is now not
      lastButtonPress = millis();
      button_state = 0;
    } else if(button_ctr > BTN_HIGH && button_state == 0) { // button was not pressed and now is
      if(millis() - lastButtonPress > DEBOUNCE_TIME) {
        button_state = 1;
        paused = !paused;

        if (paused) {
          pauseMIDI();
        } else {
          passMIDI();
        }
      }
    }
  }

#if USE_MIDI
  if (MIDI.read()) {
#else
  if (Serial.available()) {
#endif
    //Flash the LCD blue
    lastSysExTime = millis();
    setBacklight(BLUE);
    lcdIsBlue = true;

    //Get the MSC data and update the LCD
#if USE_MIDI
    MSC parsedData(MIDI.getSysExArray(), MIDI.getSysExArrayLength());
#else
    char buffer[MAX_PACKET_LENGTH];
    int len = Serial.readBytesUntil(SYSEX_END_BYTE, buffer, MAX_PACKET_LENGTH);
    MSC parsedData((byte*)buffer, len);
#endif
    updateLCD(parsedData);
  }

  //Check whether the display has been blue for long enough
  if (lcdIsBlue && millis() - lastSysExTime > SYSEX_FLASH_TIME) {
    //Decide which color to revert to
    if(enable_button) {
      if (paused) {
        setBacklight(RED);
      } else {
        setBacklight(GREEN);
      }      
    } else {
        setBacklight(0x00FFFF);
    }
    lcdIsBlue = false;
  }
}




///////////////////////////////   LCD   ////////////////////////////////////////

/**
 *  Sets up the user interface on the LCD
 */
void setupLCD() {
  //Prepare the LCD
  LCD.begin(LCD_COLUMNS, LCD_ROWS);
  LCD.noDisplay();
  LCD.clear();

  //Set LCD options
  LCD.noCursor();
  LCD.noAutoscroll();

  //Display the user interface
  LCD.print("CUE#:               ");
  LCD.setCursor(0, 1); //Newline
  LCD.print("LIST:          ID:  ");
  LCD.setCursor(0, 2); //Newline
  LCD.print("WAITING FOR DATA... ");

  //Setup the backlight
  pinMode(LCD_RED_BACKLIGHT_PIN, OUTPUT);
  pinMode(LCD_GREEN_BACKLIGHT_PIN, OUTPUT);
  pinMode(LCD_BLUE_BACKLIGHT_PIN, OUTPUT);
  setBacklight(BACKLIGHT_OFF);

  //Turn on the display
  LCD.display();
}

/**
 *  Updates the LCD user interface with new values
 *  @param packet The parsed MSC packet
 */
void updateLCD(MSC packet) {
  displayCue(packet.getCue());
  displayList(packet.getList());
  displayType(packet.getType());
  displayID(packet.getID());
  displayCommand(packet.getCommand());
  displayPacket(packet.getData(), packet.getLength());
}

/**
 *  Prints a byte's hexadecimal representation to the LCD
 *  @param c The byte to print
 */
void lcdPrintHex(byte c) {
  if (c < 0x10) {
    LCD.print(0);
  }
  LCD.print(c, HEX);
}

/**
 *  Displays the cue number
 *  @param cue The formatted, ASCII cue number
 */
void displayCue(char* cue) {
  LCD.setCursor(5, 0);
  LCD.print(cue);
}

/**
 *  Displays the list number
 *  @param list The formatted, ASCII list number
 */
void displayList(char* list) {
  LCD.setCursor(5, 1);
  LCD.print(list);
}

/**
 *  Displays the type
 *  @param type The type of command that was received
 */
void displayType(TYPE type) {
  LCD.setCursor(15, 0);
  switch(type) {
    case LIGHT:
      LCD.print("LIGHT");
      break;
    case SOUND:
      LCD.print("SOUND");
      break;
    case FIREWORKS:
      LCD.print(" PYRO");
      break;
    case ALL:
      LCD.print("  ALL");
      break;
  }
}

/**
 *  Displays the ID
 *  @param id The ID
 */
void displayID(byte id) {
  LCD.setCursor(18, 1);
  lcdPrintHex(id);
}

/**
 *  Displays the command
 *  @param cmd The command
 */
void displayCommand(COMMAND cmd) {
  LCD.setCursor(9, 3);
  LCD.print("           "); //Clear the space
  LCD.setCursor(9, 3);

  if (cmd < 0 || cmd >= sizeof(msc_command_type_strings)/sizeof(char*)) {
    //Prevent under/overflow of string array
    LCD.print(invalid_str);
  } else {
    LCD.print(msc_command_type_strings[cmd]);
  }
}

/**
 *  Displays the packet
 *  @param packet The packet bytes received
 */
void displayPacket(const byte* data, int len) {
  LCD.setCursor(0, 2);
  int i;
  // 14 = (20 spaces first row + 8 spaces second row) / (2 spaces per byte)
  for (i = 0; i < 14; i++) {
    if (i == 10) {
      LCD.setCursor(0, 3);
    }
    if (i < len) {
      //Print each byte in the packet in hex
      lcdPrintHex(data[i]);
    } else {
      LCD.print("  ");
    }
  }

  //Indicate if the packet is too long to fit on the screen
  if (len > 14) {
    LCD.setCursor(6, 3);
    LCD.print("..");
  }
}

/**
 *  Sets the backlight color of the LCD
 *
 *  @param red    The red value of the backlight
 *  @param green  The green value of the backlight
 *  @param blue   The blue value of the backlight
 */
void setBacklight(int red, int green, int blue) {
  #ifdef HAS_BL_POT

    // So, this is the stupidest way to do this, but it works.
    // Improvements welcome.

    float backlight = analogRead(BL_POT) / 1024.0;

    red = (int)(red * backlight);
    green = (int)(green * backlight);
    blue = (int)(blue * backlight);
  #endif
    analogWrite(LCD_RED_BACKLIGHT_PIN, 0xff - red);
    analogWrite(LCD_GREEN_BACKLIGHT_PIN, 0xff - green);
    analogWrite(LCD_BLUE_BACKLIGHT_PIN, 0xff - blue);

}

/**
 *  Sets the backlight color of the LCD
 *
 *  @param rgb    The red, green, and blue values of the backlight
 *                encoded in hex as RRGGBB
 */
void setBacklight(long rgb) {
  setBacklight((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}



/////////////////////////////   Button   ///////////////////////////////////////

/**
 *  Sets up the pause button
 */
void setupButton() {
  //Setup the button pin
#if BUTTON_MODE == NORMALLY_OPEN
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#else
  pinMode(BUTTON_PIN, INPUT);
#endif

  //Attach an interrupt to the button pin to listen for presses
  attachInterrupt(0, buttonInterrupt, BUTTON_UP);
}

/**
 *  Handles button press events, including debouncing
 */
void buttonInterrupt() {
  //The time in milliseconds of the last button press
  static long lastButtonPress = 0;

  if (millis() - lastButtonPress > DEBOUNCE_TIME) {
    paused = !paused;

    if (paused) {
      pauseMIDI();
    } else {
      passMIDI();
    }

  }
  lastButtonPress = millis();
}

/**
 *  Disables MIDI passthrough
 */
void pauseMIDI() {
#if USE_MIDI
  MIDI.turnThruOff();
#endif

  LCD.setCursor(8, 3);
  LCD.print("-MSC*PAUSED*");

#ifdef HAS_RELAY
  digitalWrite(RELAY_PIN,HIGH); // Turn on relay - normals to passing
#endif

  setBacklight(RED);
}

/**
 *  Enables MIDI passthrough
 */
void passMIDI() {
#if USE_MIDI
  MIDI.turnThruOn();
#endif

#ifdef HAS_RELAY
  digitalWrite(RELAY_PIN,LOW); // Turn off relay - normals to passing
#endif


  LCD.setCursor(8, 3);
  LCD.print("-MSC-PASS >>");
  if(digitalRead(A5) == 1) {
      setBacklight(GREEN);
  } else {
    setBacklight(0x00FFFF);
  }
}
