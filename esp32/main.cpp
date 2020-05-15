#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_PCF8574.h>
#include <time.h>
#include <WebSerial.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


#define NUMPIXELS 24 
#define EEPROM_SIZE 103

//0+ INGREDIENTS FLOW RATES (CALIBRATION)
//50+ PUMP INGREDIENTS
//100+ SETTINGS ADDRESSES
int EEPROM_MADE = 101;
int EEPROM_LED = 102;


//GLOBALS
const char host[] = ""; //websocket server
const char path[] = ""; //websocket path
const int port = 80;

long pingInterval = 30000;

String machinename = "Bar Bot";
String machineversion = "v1.0.0"; 
String setState = "starting";
String lastAlexaRequest_ID;

//COCKTAIL LIST          0              1               2         3                4              5                  6          7             8
const char* cocktails[]{"long island", "cosmopolitan", "mojito", "juicy tiger", "tom collins", "tequila sunrise", "margarita", "island taxi", "beach bum"};

//PUMP          0   1   2   3   4   5   6   7
int pump_i[]  { 0,  1,  3,  5,  7,  6, 13, 13}; //default integer of the ingredient loaded vodka,gin,srum,sweetsour,tequila,triple sec
int pump_pin[]{17, 16, 26, 13, 18, 23, 27, 33}; //pinout of pumps
//COUNTS
const int cocktail_count = sizeof(cocktails) / sizeof(cocktails[0]); // number of cocktails
const int pump_count = sizeof(pump_i) / sizeof(pump_i[0]);

//ingredient numbers         0           1         2             3             4           5             6             7               8          9        10           11           12       13
const char* ingredients[]{"vodka",     "gin",  "white_rum",  "spiced_rum", "whiskey",  "triple sec",  "sweetsour",  "tequila",   "sugar_syrup",  "coke", "cranberry", "orange", "tonic", "empty"};
const int ingredient_count = sizeof(ingredients) / sizeof(ingredients[0]);
int flowrates[]{           1300,       1300,     1300,          1300,        1300,        1300,         1300,          1300,           1300,      1300,     1300,       1300,     1300,     0};
int recipe[cocktail_count][ingredient_count]{
                  {          25,         13,       13,            0,            0,         13,           15,           25,              0,        75,       0,          0,          0,      0}, //0- long island
                  {          50,          0,        0,            0,            0,         25,           15,           25,              0,         0,      75,          0,          0,      0}, //1- cosmopolitan
                  {           0,          0,       50,            0,            0,          0,           25,           15,              0,         0,       0,          0,         75,      0}, //2- mojito
                  {          50,          0,        0,            0,            0,          0,           25,            0,              0,        75,       0,          0,          0,      0}, //3- juice tiger
                  {           0,         50,        0,            0,            0,          0,           25,           25,              0,         0,       0,          0,        100,      0}, //4- tom collins
                  {           0,          0,        0,            0,            0,          0,           25,           15,              0,         0,       0,        100,          0,      0}, //5- tequila sunrise
                  {           0,          0,        0,            0,            0,         25,           50,           25,              0,         0,       0,          0,          0,      0}, //6- Margarita
                  {          25,         13,       13,            0,            0,         13,           25,           15,              0,         0,       0,        100,          0,      0}, //7- island taxi
                  {          15,          0,        0,            0,            0,         15,           25,            0,              0,         0,       0,          0,          0,      0}  //8- beach bum
                  };
                

//int BLANK[]{            0,          0,        0,            0,            0,          0,            0,            0,              0,         0,       0,          0};





//BOOLEANS
boolean setDefault = false; //set default values in EEPROM
boolean debug_output = true;
boolean pump_test = false;
boolean safety_relay = true;
boolean pingServer = true;
boolean cupSensor = false;
boolean _cupSensor = true;
boolean isReady = false;
boolean updateState = 1;
boolean cupPresent = 0;
boolean _cupPresent = 1;
boolean display_menu = false;
boolean updateMenuTitle = true;
boolean lcd_backlight = true;

//BUTTON STATES
int buttonState = 0;
int _buttonState = 0;
int menuButton = 0;
int _menuButton = 0;


//TIMERS
unsigned long startMillis = millis();
unsigned long currMillis = 0;
unsigned long lastMillis = 0;
unsigned long cpingMillis = 0;
unsigned long lpingMillis = 0;



//PINOUT
const byte I2C_SDA = 21;
const byte I2C_SCL = 22;
const byte CS01 = 34;     //CUP SENSOR
const byte TM02 = 0;      //THERMISTOR
const byte greenLED = 12; //TRANSISTOR TRIGGER PIN GREEN
const byte redLED = 32;   //TRANSISTOR TRIGGER PIN RED
const byte BT05 = 35;     //POWER BUTTON
const byte BT06 = 19;     //ENCODER BUTTON
const byte powerRelay = 14;
const byte LEDRING = 25;

//ROTARY ENCODER
static int pinA = 15; // Our first hardware interrupt pin is digital pin 2
static int pinB = 4; // Our second hardware interrupt pin is digital pin 3
volatile byte aFlag = 0; // let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set)
volatile byte encoderPos = 0; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte oldEncPos = 0; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
// Button reading, including debounce without delay function declarations
const byte buttonPin = 19; // this is the Arduino pin we are connecting the push button to
byte oldButtonState = HIGH;  // assume switch open because of pull-up resistor
const unsigned long debounceTime = 10;  // milliseconds
unsigned long buttonPressTime;  // when the switch last changed state
boolean buttonPressed = 0; // a flag variable
// Menu and submenu/setting declarations
byte Mode = 0;   // This is which menu mode we are in at any given time (top level or one of the submenus)
byte _Mode = 1; 
byte Level = 0; //Menu Level
const byte modeMax = 8; // This is the number of submenus/settings you want
byte setting0 = 0;
byte setting1 = 0;  // a variable which holds the value we set 
byte setting2 = 0;  // a variable which holds the value we set 
byte setting3 = 0;  // a variable which holds the value we set 
byte setting4 = 0;
byte setting5 = 0;
byte LED_brightness = 75;

//LCD CHARACTERS
byte char_wifion[8] =   {B00000, B00000, B00001, B00001, B00101, B00101, B10101, B10101};
byte char_wifioff[8] =  {B00000, B00100, B00100, B00100, B00100, B00000, B00100, B00000};
byte select_left[8] =   {B01000, B01100, B01110, B01111, B01111, B01110, B01100, B01000};
byte select_right[8] =  {B00010, B00110, B01110, B11110, B11110, B01110, B00110, B00010};
byte cupSensor_on[8] =  {B10001, B10001, B10001, B10001, B10001, B10001, B10001, B01110};
byte cupSensor_off[8] = {B10001, B10101, B10101, B10101, B10001, B10101, B10001, B01110};
byte pumps_on[8] =      {B00000, B00100, B00100, B10101, B10101, B10001, B11111, B00000};
byte pumps_off[8] =     {B00000, B00000, B00000, B10001, B10001, B10001, B11111, B00000};

//OTHER VARIABLES
long pourdelay = 300; //delay between pumps, ensures only one pump operates at a time to avoid mathematical confusion
float Xqty;           //multiplier used from alexa
float sum;            //used for pump calculations
float aps;            //amount poured per seconds
byte error;
const int LCD_X = 20;
const int LCD_Y = 4;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;
int ref;
int LED_MAX = 100;

//DEEP SLEEP
RTC_DATA_ATTR int bootCount = 0;

//EEPROM DATA
RTC_DATA_ATTR int madeCount = 0; //total cocktails made
int madeCount_sinceOn = 0;
//total hours on

//
String selected_ingredient;
int pump_x;
int ingredient_x;

//FUNCTION DEFINITIONS

void readCupSensor();
void powerButton();
void begin_lcd();
void lcd_center(String text, int row);
void printpour(int amount, int row);
void clearline(int row);
void setHome();
void IO_init();
void isready();
void setStateTo(String text);
void powerLED(String colour);
void print_wakeup_reason();
void printLocalTime();
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
void hexdump(const void *mem, uint32_t len, uint8_t cols);
void processWebSocketRequest(String data);
void wifi_connect();
void rotaryMenu();
void setAdmin(byte name, byte setting);
void PinA();
void PinB();
void recvMsg(uint8_t *data, size_t len);
void make(byte n);
void pour(int qty, int pump_n, String x, int i);
void DEBUG(String print);
void ping_websocket();
void ringLED(int mode);
void theaterChaseRainbow(uint8_t wait);
void theaterChase(uint32_t c, uint8_t wait);
void rainbow(int wait);
void colorWipe(uint32_t c, uint8_t wait);


//TYPE DEFINITIONS
LiquidCrystal_PCF8574 lcd(0x27);
WiFiMulti wifiMulti;
WebSocketsClient webSocket;
AsyncWebServer server(80);
StaticJsonDocument<201> req;
Adafruit_NeoPixel pixels(NUMPIXELS, LEDRING, NEO_GRB + NEO_KHZ800);


//CODE
void setup()
{

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Wire.begin();
  pixels.begin();
  EEPROM.begin(EEPROM_SIZE);
  IO_init();
  begin_lcd();

  powerLED("red");
  
  ++bootCount;
  print_wakeup_reason();
  Serial.println("[ESP] - Boot number: " + String(bootCount));
  lcd.createChar(0, char_wifion);
  lcd.createChar(1, char_wifioff);
  lcd.createChar(2, select_left);
  lcd.createChar(3, select_right);
  lcd.createChar(4, cupSensor_on);
  lcd.createChar(5, cupSensor_off);
  lcd.createChar(6, pumps_on);
  lcd.createChar(7, pumps_off);

  for (uint8_t t = 5; t > 0; t--)
  {
    Serial.printf("[SETUP] -  BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  Serial.println("[SETUP] - Connecting to Wifi");
  lcd_center("Connecting to WiFi", 2);
  wifi_connect();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if (wifiMulti.run() != WL_CONNECTED)
  {
    lcd_center("No WiFi Available", 2);
  }
  else
  {
    lcd_center("WiFi Connected", 2);
    delay(200);
    printLocalTime();
    delay(200);
    Serial.print("[SETUP] - IP Address: ");
    Serial.println(WiFi.localIP());
    delay(200);
    WebSerial.begin(&server);
    WebSerial.msgCallback(recvMsg);
    server.begin();
    
  
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      DEBUG("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      DEBUG("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      //WebSerial.println("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Update Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  
  }

  Serial.println("[SETUP] - LOADING SETTINGS");
  lcd_center("loading settings", 2);
  
  if(setDefault == true){
    DEBUG("[SETUP] - LOADING FIRST TIME SETTINGS");
    setStateTo("loading first run");
    for(int i = 0; i < (ingredient_count); i++){ //0+ FLOW DATA
      int def = 1000; //DEFAULT FLOW RATE
      flowrates[i] = def;
      EEPROM.write(i, def);
      Serial.println("[EEPROM] - Flow data in position: " + String(i) + " is: " + String(def));
      delay(10);
    }

    //LOAD PUMP INGREDIENTS
    int x = 0;
    int z = (50 + pump_count); //50+ INGREDIENTS
    for(int i = 50; i < z; i++){
      int read = pump_i[x];;
      EEPROM.write(i, read);
      Serial.println("[EEPROM] - Pump " + String(x) + " integer data in position: " + String(i) + " is: " + String(read));
      x++;
      delay(10);
    } 

    EEPROM.write(EEPROM_MADE, 0);
    EEPROM.write(EEPROM_LED, 75); //LED BRIGHTNESS
    EEPROM.commit();
    DEBUG("EEPROM WRITE SUCCESSFUL");
  }else{ //NOT FIRST RUN
    
    //LOAD FLOW RATES
    for(int i = 0; i < (ingredient_count); i++){
      int read = EEPROM.read(i);
      int corrected = read * 10;
      flowrates[i] = corrected;
      Serial.println("[EEPROM] - Flow data in position: " + String(i) + " is: " + String(corrected));
      delay(10);
    }

    //LOAD PUMP INGREDIENTS
    int x = 0;
    int z = (50 + pump_count);
    for(int i = 50; i < z; i++){
      int read = EEPROM.read(i);
      pump_i[x] = read;
      Serial.println("[EEPROM] - Pump " + String(x) + " integer data in position: " + String(i) + " is: " + String(read));
      x++;
      delay(10);
    }

    //OTHER EEPROM DATA
    madeCount = EEPROM.read(EEPROM_MADE); //TOTAL NUMBER OF COCKTAILS MADE
    LED_brightness = EEPROM.read(EEPROM_LED);
    DEBUG("EEPROM DATA IN POSITION: " + String(EEPROM_LED) + " is " + String(LED_brightness));
  }

  
  madeCount_sinceOn = 0;

  DEBUG("TURNING ON LIGHT RING");
 for(int i=0; i<NUMPIXELS; i++) { // For each pixel...

    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    // Here we're using a moderately bright green color:
    pixels.setPixelColor(i, pixels.Color(LED_brightness, LED_brightness, LED_brightness));

    pixels.show();   // Send the updated pixel colors to the hardware.

    delay(50); // Pause before next pass through loop
  }

  if(pump_test == true){
      lcd_center("testing pumps", 2);
    //TEST PUMPS BEFORE 12v IS SWITCHED ON
    for(int b = 0; b < pump_count; b++){
      DEBUG("Testing pump: " + String(b));
      digitalWrite(pump_pin[b], LOW);
      delay(200);
      digitalWrite(pump_pin[b], HIGH);
      delay(50);
    }
  }
  
  
  //FINISHED
  lcd.clear();
  DEBUG("[SETUP] - COMPLETE");
  lcd_center("setup complete", 2);
  setStateTo("ready");
  digitalWrite(powerRelay, LOW);
  safety_relay = LOW;
}

void loop()
{
  ArduinoOTA.handle();
  webSocket.loop();
  ping_websocket();
  powerButton();
  
  buttonState = digitalRead(BT06);
  if(!buttonState && oldButtonState && !display_menu){
    oldButtonState = buttonState;
    display_menu = true;
    encoderPos = 0;
    buttonPressed = 0;
    Mode = 0;
    _Mode = 1;
    Level = 0;
    lcd.clear();
  }
  
  if(display_menu == true){
    rotaryMenu();
  }else{
    readCupSensor();
    setHome();
  }
  
}

void begin_lcd()
{
  Wire.beginTransmission(0x27);
  error = Wire.endTransmission();
  Serial.print("[I2C] - LCD Error: ");
  Serial.print(error);

  if (error == 0)
  {
    Serial.println(": LCD found.");
    lcd.begin(20, 4);
    delay(1000);
    lcd.setBacklight(255);
    lcd.home();
    lcd.clear();
    delay(50);
    lcd_center("HELLO", 2);
    delay(500);
    lcd.clear();
    delay(50);
    lcd.setBacklight(0);
    delay(200);
    lcd.setBacklight(255);
    lcd_center(machinename + " " + machineversion, 1);
    lcd_center("initializing", 2);
  }
  else
  {
    Serial.println(": LCD not found.");
  }
}

void lcd_center(String text, int row)
{
  int textPosition = (20 - text.length()) / 2;
  clearline(row);
  lcd.setCursor(textPosition, row);
  lcd.print(text);
}

void printpour(int amount, String ingredient)
{
  lcd_center(ingredient + " - " + amount + "ml", 2);
  Serial.print("[BARBOT] - Pouring ");
  Serial.print(amount);
  Serial.print("ml of ");
  Serial.print(ingredient);
  Serial.println();
}

void clearline(int row)
{
  lcd.setCursor(0, row);
  for (int i = 0; i < LCD_X; i++)
  {
    lcd.print(" ");
  }
}

void setHome()
{
  printLocalTime();
  lcd.setCursor(17, 0);

  if(cupSensor == true){
    lcd.write((byte)4);
  }else{
    lcd.write((byte)5);
  }
  if(safety_relay == LOW){
    lcd.write((byte)6);
  }else{
    lcd.write((byte)7);
  }
  if (wifiMulti.run() != WL_CONNECTED)
  {
    lcd.write((byte)1);
  }
  else
  {
    lcd.write((byte)0);
  }
  
  //Serial.print("Current State: ");
  //Serial.println(setState);
  if (updateState == true)
  {
    lcd_center(machinename + " " + machineversion, 1);
    lcd_center(setState, 2);
    updateState = false;
    isready();
    ringLED(1);
  }
}

void IO_init()
{
  pinMode(CS01, INPUT);  //CUPSENSOR
  pinMode(TM02, INPUT);  //THERMISTOR
  pinMode(greenLED, OUTPUT); //TRANSISTOR GREEN
  pinMode(redLED, OUTPUT); //TRANSISTOR RED
  pinMode(BT05, INPUT);  //POWER BUTTON
  pinMode(pinA, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage 
  pinMode(pinB, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage 
  pinMode (buttonPin, INPUT_PULLUP); // setup the button pin
  pinMode(powerRelay, OUTPUT);
  digitalWrite(powerRelay, HIGH);
  attachInterrupt(pinA,PinA,RISING); // set an interrupt on PinA, looking for a rising edge signal and executing the "PinA" Interrupt Service Routine (below)
  attachInterrupt(pinB,PinB,RISING); // set an interrupt on PinB, looking for a rising edge signal and executing the "PinB" Interrupt Service Routine (below)
  
  for(int i = 0; i < pump_count; i++){
    pinMode(pump_pin[i], OUTPUT);
    digitalWrite(pump_pin[i], HIGH);
    delay(500);
  }

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 1); //1 = High, 0 = Low
}

void isready()
{

  if (cupSensor == false)
  {
    if(_cupSensor == true){
    setStateTo("ready for an order");
    powerLED("green");
    _cupSensor = false;
    }else{
      powerLED("green");
    }
  }
  else
  {
    _cupSensor = true;
    if (cupPresent == true)
    {
      Serial.println("[BARBOT] - Glass Present");
      setStateTo("ready for an order");
      powerLED("green");
      //colorWipe(pixels.Color(  0,   50, 0), 50); // Blue
    }
    else
    {
      Serial.println("[BARBOT] - Glass Required");
      powerLED("orange");
      setStateTo("Glass Required");
      //colorWipe(pixels.Color(  50,   40, 0), 50); // Blue
    }
  }

}

void setStateTo(String text)
{
  if (setState != text)
  {
    updateState = true;
    setState = text;
    Serial.print("[BARBOT] - State changed to: ");
    Serial.println(text);
  }
}
void ringLED(int mode){
  
  switch(mode){
    case 1: //home default
      for(int i=0; i<NUMPIXELS; i++) { // For each pixel...
        pixels.setPixelColor(i, pixels.Color(LED_brightness, LED_brightness, LED_brightness));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(10);
      }
      break;
    case 2: //drink ready
      colorWipe(pixels.Color(0, 255, 0), 50); // Wipe Green
      colorWipe(pixels.Color(0, 255, 0), 50); // Wipe Green
      for(int i=0; i<NUMPIXELS; i++) { // For each pixel...
        pixels.setPixelColor(i, pixels.Color(0, LED_brightness, 0));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(10);
      }
      break;
    case 3: //alexa request
      for(int i=0; i<NUMPIXELS; i++) { // For each pixel...
          pixels.setPixelColor(i, pixels.Color(0, 0, LED_brightness));
          pixels.show();   // Send the updated pixel colors to the hardware.
          delay(10);
      }
      colorWipe(pixels.Color(  0,   (LED_brightness/2), LED_brightness), 50); // Blue
      break;
    case 4: //orange
      for(int i=0; i<NUMPIXELS; i++) { // For each pixel...
        pixels.setPixelColor(i, pixels.Color(LED_brightness, LED_brightness, 0));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(10);
      }
      break;

    default: //soft white

      break;
  }
}
void powerLED(String colour)
{
  Serial.println();
  Serial.print("[BARBOT] - LED SET TO: ");
  Serial.println(colour);
  Serial.println();
  
  if (colour == "green")
  {
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
    
  }
  else if (colour == "red")
  {
    digitalWrite(redLED, HIGH);
    digitalWrite(greenLED, LOW);
  }
  else if (colour == "orange")
  {
    digitalWrite(redLED, HIGH);
    digitalWrite(greenLED, HIGH);
  }
  else
  {
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, LOW);
  }
}

void powerButton()
{

  buttonState = digitalRead(BT05);

  if (buttonState != _buttonState)
  {
    _buttonState = buttonState;
    if (buttonState == HIGH)
    {
      //sleep/wake function
      Serial.println("[ESP] - Power Button Pushed");
      lcd.clear();
      lcd_center("Going to sleep now", 1);
      lcd_center("Bye", 2);
      colorWipe(pixels.Color(  (LED_brightness/2),   0, 0), 25); // Blue
      lcd.clear();
      lcd_center("zZzZzZZZ", 2);
      lcd.setBacklight(0);
      colorWipe(pixels.Color(  0,  0, 0), 25); // Blue
      pixels.clear();
      delay(1000);
      esp_deep_sleep_start();
    }
  }
}

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("[ESP] - ");
  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

void readCupSensor()
{
  if(digitalRead(CS01) == HIGH)
  {
    cupPresent = true;     
  }else{
    cupPresent = false;
  }
  
  if(cupPresent != _cupPresent)
  {
    isready();
    _cupPresent = cupPresent;
  }
}

void wifi_connect(){
  //LIST OF KNOWN NETWORKS - WiFiMulti will connect to the strongest one
  wifiMulti.addAP("DEFAULT", "PASSWORD");

  //WiFiMulti.addAP("SSID", "passpasspass"); //add as many more as needed

  //WiFi.disconnect();
  int connection_attempts = 0;
  while(wifiMulti.run() != WL_CONNECTED) {
    delay(100);
    connection_attempts++;
    if(connection_attempts >= 15)
    {
      break;
      Serial.println("[WIFI] - NO NETWORK FOUND");
      setStateTo("NO NETWORK FOUND");
    }
  }

  // server address, port and URL
 webSocket.begin(host, port, path);

  // event handler
  webSocket.onEvent(webSocketEvent);
  //webSocket.beginSSL(host, 80);

  //webSocket.setAuthorization("user", "Password"); //NOT NEEDED

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    delayMicroseconds(10);
    
    switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      powerLED("red");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      webSocket.sendTXT("Connected");
      isready();
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      processWebSocketRequest((char*)payload);
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      //hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
    case WStype_ERROR:      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    case WStype_PING:
    case WStype_PONG:
      break;

  }
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
  const uint8_t* src = (const uint8_t*) mem;
  Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
  for(uint32_t i = 0; i < len; i++) {
    if(i % cols == 0) {
      Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
    }
    Serial.printf("%02X ", *src);
    src++;
  }
  Serial.printf("\n");
}

void processWebSocketRequest(String data){
  
  //WEB SOCKET REQUEST
  String Response = "{\"version\": \"1.0\",\"sessionAttributes\": {},\"response\": {\"outputSpeech\": {\"type\": \"PlainText\",\"text\": \"<text>\"},\"shouldEndSession\": true}}";
  
  DeserializationError Jerror = deserializeJson(req, data);

  // Test if parsing succeeds.
  if (Jerror) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(Jerror.c_str());
    return;
  }

  String choice = req["cocktail"];
  String value = req["value"];
  String multiplier = req["multiplier"];
  String ID = req["id"];
  String cmd = req["cmd"];

  if(madeCount_sinceOn == 0){
    DEBUG("Ignoring first cocktail");
    madeCount_sinceOn++;
    madeCount++;
    isready();
    return;
  }

  powerLED("red");
  ringLED(3);

  

  DEBUG("choice: " + choice + " value: " + value);
  DEBUG("ID: " + ID + " multiplier: " + multiplier);
  DEBUG("cmd: " + cmd);

  if(ID == lastAlexaRequest_ID || ID == "null" || ID == ""){
    DEBUG("DUPLICATE ALEXA REQUEST ID FOUND, EXITING FUNCTION");
    updateState = true;
    display_menu = false;
    lcd.clear();
    return;
  }else{
    lastAlexaRequest_ID = ID;
  }

  

  if(cupSensor == true){
    if(cupPresent == false){
        Serial.print("[BARBOT] - NO CUP PRESENT");
        Response.replace("<text>", "NO CUP PRESENT");
        webSocket.sendTXT(Response);  
        return;
   }
  }
 
  

  //is it a cocktail or just a pour?
    //is choice in cocktails[]
    for(int i = 0; i < cocktail_count; i++){
      String xchoice = cocktails[i];
      if(xchoice == choice){
        make(i);
        updateState = true;
        return;
      }
    }

  //get multiplier to int
  int qty;
  if(multiplier == "single"){
    qty = 25;
  }else if(multiplier == "double"){
    qty = 50;
  }else{
    qty = multiplier.toInt();
  }

  if(qty > 0){
    for(int i = 0; i < ingredient_count; i++){
      String ychoice = ingredients[i];
      if(ychoice == choice){ //this position is the ingredient we are looking for
        for(int q = 0; q < pump_count; q++){ //search through pump ingredients
          if(pump_i[q] == i){
            lcd_center("Alexa Request", 1);
            DEBUG("POURING " + String(qty) + "ml FROM PUMP " + String(q) + " INGREDIENT: " + choice + " # " + String(i));
            pour(qty,q,choice,i);
            lcd.clear();
            powerLED("green");
            lcd_center("READY",1);
            delay(5000);
            lcd.clear();
            updateState = true;
            break;
          }else{
            if((q+1) == pump_count){
            Serial.println("[BARBOT] - " + choice + " not found");
            powerLED("red");
            lcd_center("NOT FOUND",1);
            delay(5000);
            lcd.clear();
            updateState = true;            
            }
          }
        }

      }
    }
  }

  webSocket.sendTXT("DONE");  
  powerLED("green");
  
  display_menu = false;
  Mode = 0;
  Level = 0;
  updateState = true;
  lcd.clear();

}

void printLocalTime()
{
  currMillis = millis();
  if(currMillis - lastMillis >= 1000){
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }
    //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    lcd.setCursor(0,0);
    lcd.print(&timeinfo, "%H:%M");
    lastMillis = currMillis;
  }
}


void rotaryMenu() { //This handles the bulk of the menu functions without needing to install/include/compile a menu library
  //DEBUGGING: Rotary encoder update display if turned
  if(oldEncPos != encoderPos) { // DEBUGGING
    DEBUG("[MENU] - Encoder Position: " + String(encoderPos));
    oldEncPos = encoderPos;// DEBUGGING
  }// DEBUGGING
  // Button reading with non-delay() debounce - thank you Nick Gammon!
  byte buttonState = digitalRead (buttonPin); 
  if (buttonState != oldButtonState){
    if (millis () - buttonPressTime >= debounceTime){ // debounce
      buttonPressTime = millis ();  // when we closed the switch 
      oldButtonState =  buttonState;  // remember for next time 
      if (buttonState == LOW){
        //DEBUG("[MENU] - Button closed"); // DEBUGGING: print that button has been closed
        buttonPressed = 1;
      }
      else {
        //DEBUG("[MENU] - Button opened"); // DEBUGGING: print that button has been opened
        buttonPressed = 0;  
      }  
    }  // end if debounce time up
  } // end of state change

/*------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------LEVEL 0 - MAIN MENU-------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------*/

  //Main menu section
  if (Mode == 0 && Level == 0) {
    if (encoderPos > (modeMax+10)) encoderPos = modeMax; // check we haven't gone out of bounds below 0 and correct if we have
    else if (encoderPos > modeMax) encoderPos = 0; // check we haven't gone out of bounds above modeMax and correct if we have
    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Menu:");
      if (encoderPos == 0) {
          lcd_center("Make Cocktail",1);
          clearline(2); 
      }
      if (encoderPos == 1) {
        lcd_center("Pour Spirit",1);
        clearline(2); 
      }
      if (encoderPos == 2) {
        lcd_center("Pump Control",1);
        clearline(2); 
      }
      if (encoderPos == 3) {
        lcd_center("Flow Rates",1);
        clearline(2); 
      }
      if (encoderPos == 4) {
        lcd_center("Enable Pumps",1);
        clearline(2); 
        lcd.setCursor(9,2);
        if(setting2 % 2 == 0){
          lcd.print("ON");
        }else{
          lcd.print("OFF");
        }
      }
      if (encoderPos == 5) {
        lcd_center("Cup Sensor",1);
        clearline(2);
        lcd.setCursor(9,2);
        if(cupSensor == true){
          lcd.print("ON");
        }else{
          lcd.print("OFF");
        }        
      }
      if (encoderPos == 6) {
        lcd_center("LED BRIGHTNESS",1);
        lcd_center(String(LED_brightness),2);
        
      }
      if (encoderPos == 7) {
        lcd_center("Wi-Fi Info",1);
        clearline(2);
        if(wifiMulti.run() == WL_CONNECTED){
          lcd.setCursor(4,2);
          lcd.print(WiFi.localIP());
        }
        
      }
      if (encoderPos == 8) {
        lcd_center("EXIT",1);
        clearline(2);
      }
      
      lcd.setCursor(0,1);
      lcd.write((byte)3);
      lcd.setCursor(19,1);
      lcd.write((byte)2);
      _Mode = encoderPos;
    }

    if (buttonPressed){ 
      Mode = encoderPos; // set the Mode to the current value of input if button has been pressed
      Serial.print("[MENU] - Level: 0 Mode selected: "); //DEBUGGING: print which mode has been selected
      Serial.println(Mode); //DEBUGGING: print which mode has been selected
      buttonPressed = 0; // reset the button status so one press results in one action
      //CLEAR ARROWS
      lcd.setCursor(0,1);
      lcd.print(" ");
      lcd.setCursor(19,1);
      lcd.print(" ");

      if (Mode == 0) { //COCKTAIL MENU LEVEL 1
        //encoderPos = setting2; // start adjusting Vout from last set point
        encoderPos = 0; 
        Level = 1;
        Mode = 0;
        lcd.clear();
        _Mode = 99; //TO UPDATE POSITION
      }
      if (Mode == 1) { //GO TO POUR SPIRIT LEVEL 2
        encoderPos = 0; 
        Level = 2;
        Mode = 0;
        lcd.clear();
        _Mode = 99; //TO UPDATE POSITION
      }
      if (Mode == 2) { //GO TO PUMP CONTROL LEVEL 2
        encoderPos = 0; 
        Level = 3;
        Mode = 0;
        lcd.clear();
        _Mode = 99; //TO UPDATE POSITION
      }
      if (Mode == 3) { //FLOW RATES
        encoderPos = 0; 
        Level = 4;
        Mode = 0;
        lcd.clear();
        _Mode = 99; //TO UPDATE POSITION
      }
      if (Mode == 4) { //SCREEN BRIGHTNESS
        delay(100);
        //encoderPos = setting3;
      }
      if (Mode == 5) { //CUP SENSOR
        delay(100);
        //encoderPos = setting4; // start adjusting Imax from last set point
      }
      if (Mode == 6) { //LED RING
        delay(100);
        encoderPos = LED_brightness; // start adjusting Imax from last set point
      }
      if (Mode == 7) { //WIFI
        
      }
      if (Mode == 8) { //EXIT
        display_menu = false;
        updateState = true;
        oldButtonState = 1;
        lcd.clear();
        lcd_center("EXITING...",1);
        encoderPos = 0;
        Mode = 0;
        Level = 0;
        buttonPressed = 0;
        //Serial.println("[MENU] - RETURNING HOME");
        delay(500);
        lcd.clear();
      }
    }

  }else if(Level == 0 && Mode != 0){ //LEVEL 0 - MODE IS NOT 0 
    if(Mode == 4 || Mode == 5 || Mode == 6){
      lcd.setCursor(8,2);
      lcd.write((byte)2);
      if (Mode == 4) { //Screen
        if(encoderPos % 2 == 0){
          lcd.print(" ON");
        }else{
          lcd.print("OFF");
        }
      }
      if (Mode == 5) { //Cup Sensor
        if(encoderPos % 2 == 0){
          lcd.print(" ON");
        }else{
          lcd.print("OFF");
        }
      }
      if(Mode == 6){
        lcd.print(String(encoderPos));
      }
      lcd.write((byte)3);
    }
  }

  if(Level == 0){
    if (Mode == 0 && buttonPressed) {
      setting0 = encoderPos; // record whatever value encoder has been turned to, to setting 0
      setAdmin(1,setting0);
      //code to do other things with setting1 here, perhaps update display  
      Serial.println("[BARBOT] - ENTERING COCKTAIL SUBMENU");
      clearline(2);
    }
    if (Mode == 1 && buttonPressed) {
      setting1 = encoderPos; // record whatever value encoder has been turned to, to setting 1
      //setAdmin(1,setting1);
      //code to do other things with setting1 here, perhaps update display  
      Serial.println("[BARBOT] - ENTERING POUR SPIRIT MENU");
      clearline(2);
    }
    if (Mode == 2 && buttonPressed) {
      setting1 = encoderPos; // record whatever value encoder has been turned to, to setting 1
      //setAdmin(1,setting1);
      //code to do other things with setting1 here, perhaps update display  
      Serial.println("[BARBOT] - ENTERING PUMP SUBMENU");
      clearline(2);
    }
    if (Mode == 3 && buttonPressed) {
      setting1 = encoderPos; // record whatever value encoder has been turned to, to setting 1
      //setAdmin(1,setting1);
      //code to do other things with setting1 here, perhaps update display  
      Serial.println("[BARBOT] - ENTERING FLOW RATE SUBMENU");
      clearline(2);
    }
    if (Mode == 4 && buttonPressed) {
      setting2 = encoderPos; // record whatever value encoder has been turned to, to setting 2
      setAdmin(2,setting2);
      if(setting2 % 2 == 0){
        digitalWrite(powerRelay, LOW);
        safety_relay = LOW;
      }else{
        digitalWrite(powerRelay, HIGH);
        safety_relay = HIGH;
      }
      
      clearline(2);
    }
    if (Mode == 5 && buttonPressed) {
      setting3 = encoderPos; // record whatever value encoder has been turned to, to setting 3
      setAdmin(3,setting3);
      if(setting3 % 2 == 0){
        cupSensor = true;
        DEBUG("CUP SENSOR ON");
      }else{
        cupSensor = false;
        DEBUG("CUP SENSOR OFF");
      }
      updateState = true;
      clearline(2);
     
    }
    if (Mode == 6 && buttonPressed){ //CHANGE LED BRIGHTNESS
      if(encoderPos > LED_MAX){
        encoderPos = LED_MAX;
      }
      LED_brightness = encoderPos;
      EEPROM.write(EEPROM_LED, LED_brightness);
      EEPROM.commit();
      DEBUG("EEPROM POSITION: " + String(EEPROM_LED) + " set to " + String(LED_brightness));
      display_menu = false;
      updateState = true;
      oldButtonState = 1;
      Mode = 0;
      _Mode = 99; //force update 
      buttonPressed = 0;
      encoderPos = 0;
      Level = 0;
      ringLED(2);
      lcd.clear();
    }
    if (Mode == 7 && buttonPressed){ 
      setting4 = encoderPos; // record whatever value your encoder has been turned to, to setting 3
      setAdmin(4,setting4);
      clearline(2);
      //WIFI SETTINGS
      //code to do other things with setting3 here, perhaps update display 
      
    }
    if (Mode == 8 && buttonPressed){ 
      //ALREADY EXITED
    }
  }

/*------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------LEVEL 1 - COCKTAILS-------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------*/

  if (Mode == 0 && Level == 1) {

    if (encoderPos > (cocktail_count+10)) encoderPos = cocktail_count; // check we haven't gone out of bounds below 0 and correct if we have
    else if (encoderPos > cocktail_count) encoderPos = 0; // check we haven't gone out of bounds above modeMax and correct if we have
    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Choose Drink:");

      clearline(1);
      if(encoderPos == cocktail_count){
        lcd_center("BACK",1);
      }else{
        String c = cocktails[encoderPos];
        lcd_center(c,1);
      }
      
        
      clearline(2); 
      lcd.setCursor(0,1);
      lcd.write((byte)3);
      lcd.setCursor(19,1);
      lcd.write((byte)2);
      _Mode = encoderPos;
    }

    if (buttonPressed){ 
      Mode = encoderPos; // set the Mode to the current value of input if button has been pressed
      Serial.print("[MENU] - Level 1 - Selection: "); //DEBUGGING: print which cocktail int has been selected
      Serial.println(Mode); //DEBUGGING: print which int cocktail has been selected
      buttonPressed = 0; // reset the button status so one press results in one action

      if(Mode == cocktail_count){
        //GO BACK A LEVEL
        lcd.clear();
        Level = 0;
        Mode = 0;
      }else{
        //POUR COCKTAIL
        String c = cocktails[Mode];
        lcd.clear();
        Serial.print("[BARBOT] - Requested to pour a ");
        Serial.println(c);
        make(Mode);
        
        madeCount++;
        madeCount_sinceOn++;
        //RETURN HOME
        display_menu = false;
        updateState = true;
        oldButtonState = 1;
        encoderPos = 0;
        Mode = 0;
        _Mode = 99;
        Level = 0;
        buttonPressed = 0;
        lcd.clear();
        setHome();
      }
      
    }

  }
/*------------------------------------------------------------------------------------------------------------------------------
----------------------------------------------MENU LEVEL 2 - POUR SPIRIT ------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------*/


  if (Mode == 0 && Level == 2) {

    if (encoderPos > (pump_count+10)) encoderPos = pump_count; // check we haven't gone out of bounds below 0 and correct if we have
    else if (encoderPos > pump_count) encoderPos = 0; // check we haven't gone out of bounds above modeMax and correct if we have

    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Pour:");
      
      if(encoderPos == pump_count){
        lcd_center("BACK",1);
        clearline(2);
      }else{
        int i = pump_i[encoderPos];
        clearline(1);
        lcd_center("Pump: " + String(encoderPos),1);
        pump_x = encoderPos;
        String x = ingredients[i];
        //ingredient_x = x;
        lcd_center(x,2);
      }     
      
      lcd.setCursor(0,1);
      lcd.write((byte)3);
      lcd.setCursor(19,1);
      lcd.write((byte)2);
      _Mode = encoderPos;
    }

    if (buttonPressed){ 

 
      Mode = encoderPos + 1; // set the Mode to the current value of input if button has been pressed
      Serial.print("[MENU] - Pump- Selection: "); //DEBUGGING: print which cocktail int has been selected
      Serial.println(encoderPos); //DEBUGGING: print which int cocktail has been selected
      buttonPressed = 0; // reset the button status so one press results in one action

      //CLEAR ARROWS
      lcd.setCursor(0,1);
      lcd.print(" ");
      lcd.setCursor(19,1);
      lcd.print(" ");

      if(encoderPos == pump_count){
        //GO BACK A LEVEL
        DEBUG("exiting pour menu");
        lcd.clear();
        Level = 0;
        Mode = 0;
      }else{
        //ADJUST PUMP
        int i = pump_i[encoderPos];
        String x = ingredients[i];
        selected_ingredient = x;
        ingredient_x = i;
        lcd_center(x,1);
        Serial.print("[BARBOT] - pouring from pump: ");
        Serial.println(encoderPos);
        ref = pump_i[encoderPos];
        encoderPos = 1;
        _Mode = 99;
      }
      
    }

  }else if(Level == 2 && Mode != 0){ //LEVEL 2 - MODE IS NOT 0 

    if (encoderPos > (40)) encoderPos = 20; // check we haven't gone out of bounds below 0 and correct if we have
    else if (encoderPos > 20) encoderPos = 0; // check we haven't gone out of bounds above modeMax and correct if we have
    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Amount:");
      
      int i = encoderPos * 25;
      clearline(2);
      lcd_center(String(i) + " ml",2);

      lcd.setCursor(0,2);
      lcd.write((byte)3);
      lcd.setCursor(19,2);
      lcd.write((byte)2);
      _Mode = encoderPos;
      
    }
    if(buttonPressed){ //store ingredient in array
      //mode = pump i
      //encoderPos = ingredient i
        int qty = encoderPos * 25;
        powerLED("red");
        ringLED(4);
        pour(qty, pump_x, selected_ingredient, ingredient_x);
        delay(100);
        Serial.println("pouring spirit");
        //RETURN HOME
        display_menu = false;
        updateState = true;
        oldButtonState = 1;
        encoderPos = 0;
        Mode = 0;
        _Mode = 99;
        Level = 0;
        buttonPressed = 0;
        lcd.clear();
        setHome();
    }
  }

/*------------------------------------------------------------------------------------------------------------------------------
----------------------------------------------MENU LEVEL 3 - CHANGE SPIRIT ------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------*/


  if (Mode == 0 && Level == 3) {

    if (encoderPos > (pump_count+10)) encoderPos = pump_count; // check we haven't gone out of bounds below 0 and correct if we have
    else if (encoderPos > pump_count) encoderPos = 0; // check we haven't gone out of bounds above modeMax and correct if we have

    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Pump Control:");
      
      if(encoderPos == pump_count){
        lcd_center("BACK",1);
        clearline(2);
      }else{
        int i = pump_i[encoderPos];
        clearline(1);
        lcd_center("Pump: " + String(encoderPos),1);
        String x = ingredients[i];
        lcd_center(x,2);
      }     
      
      lcd.setCursor(0,1);
      lcd.write((byte)3);
      lcd.setCursor(19,1);
      lcd.write((byte)2);
      _Mode = encoderPos;
    }

    if (buttonPressed){ 
      Mode = encoderPos + 1; // set the Mode to the current value of input if button has been pressed
      Serial.print("[MENU] - Pump- Selection: "); //DEBUGGING: print which cocktail int has been selected
      Serial.println(encoderPos); //DEBUGGING: print which int cocktail has been selected
      buttonPressed = 0; // reset the button status so one press results in one action

      //CLEAR ARROWS
      lcd.setCursor(0,1);
      lcd.print(" ");
      lcd.setCursor(19,1);
      lcd.print(" ");

      if(encoderPos == pump_count){
        //GO BACK A LEVEL
        lcd.clear();
        Level = 0;
        Mode = 0;
        encoderPos = 0;
      }else{
        //ADJUST PUMP

        Serial.print("[BARBOT] - Entering pump control pump: ");
        Serial.println(encoderPos);
        ref = pump_i[encoderPos];
        encoderPos = ref;
        _Mode = 99;
      }
      
    }

  }else if(Level == 3 && Mode != 0){ //LEVEL 2 - MODE IS NOT 0 

    if (encoderPos > (ingredient_count+20)) encoderPos = ingredient_count - 1; // check we haven't gone out of bounds below 0 and correct if we have
    else if (encoderPos >= ingredient_count) encoderPos = 0; // check we haven't gone out of bounds above modeMax and correct if we have
    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Ingredient:");
      
      String i = ingredients[encoderPos];
      clearline(2);
      lcd_center(i,2);

      lcd.setCursor(0,2);
      lcd.write((byte)3);
      lcd.setCursor(19,2);
      lcd.write((byte)2);
      _Mode = encoderPos;
      
    }
    if(buttonPressed){ //store ingredient in array
      //mode = pump i
      //encoderPos = ingredient i

        int loc = Mode - 1;
        String i = ingredients[encoderPos];
        pump_i[loc] = encoderPos;
        int EEPROM_LOC = 50 + loc;
        //Write _Mode to address Mode
        EEPROM.write(EEPROM_LOC,encoderPos);
        EEPROM.commit();
        
        Serial.println("[MENU] - Pump " + String(loc) + " set to " + i);

        Mode = 0;
        _Mode = 99;
        buttonPressed = 0;
        encoderPos = 0;
        Level = 0;
        lcd.clear();
    }
  }

/*------------------------------------------------------------------------------------------------------------------------------
----------------------------------------------MENU LEVEL 4 - FLOW RATES---------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------*/

  if (Mode == 0 && Level == 4) {

    if (encoderPos > (ingredient_count+10)) encoderPos = ingredient_count; // check we haven't gone out of bounds below 0 and correct if we have
    else if (encoderPos > ingredient_count) encoderPos = 0; // check we haven't gone out of bounds above modeMax and correct if we have

    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Flow Rates:");
      
      if((encoderPos) == ingredient_count){
        lcd_center("BACK",1);
        clearline(2);
      }else{
        String i = ingredients[encoderPos];
        clearline(1);
        lcd_center(i,1);
        int x = flowrates[encoderPos];
        lcd_center(String(x),2);
      }     
      
      lcd.setCursor(0,1);
      lcd.write((byte)3);
      lcd.setCursor(19,1);
      lcd.write((byte)2);
      _Mode = encoderPos;
    }

    if (buttonPressed){ 
      Mode = encoderPos + 1; // set the Mode to the current value of input if button has been pressed

      buttonPressed = 0; // reset the button status so one press results in one action
      
      //CLEAR ARROWS
      lcd.setCursor(0,1);
      lcd.print(" ");
      lcd.setCursor(19,1);
      lcd.print(" ");
      if(encoderPos >= ingredient_count){
        //GO BACK A LEVEL
        lcd.clear();
        Level = 0;
        Mode = 0;
        encoderPos = 0;
      }else{
        Serial.print("[MENU] - Ingredient Selection: "); //DEBUGGING: print which cocktail int has been selected
        Serial.println(ingredients[encoderPos]); //DEBUGGING: print which int cocktail has been selected
        int x = flowrates[encoderPos];
        encoderPos = x / 10;
        Serial.print("[BARBOT] - Changing ingredient flow rate: ");
        Serial.println(String(encoderPos));
        _Mode = 99;
      }
      
    }

  }else if(Level == 4 && Mode != 0){ //LEVEL 2 - MODE IS NOT 0 
    
    if(_Mode != encoderPos){
      clearline(0);
      lcd.setCursor(0,0);
      lcd.print("Adjust f/r:");
      
      int y = encoderPos * 10;
      clearline(2);
      lcd_center(String(y),2);

      lcd.setCursor(0,2);
      lcd.write((byte)3);
      lcd.setCursor(19,2);
      lcd.write((byte)2);
      _Mode = encoderPos;
      
    }
    if(buttonPressed){ //store ingredient in array
      //mode = pump i
      //encoderPos = ingredient i

        int y = _Mode * 10;
        int z = Mode - 1;
        flowrates[z] = y;
        //Write _Mode to address Mode
        EEPROM.write(z,_Mode);
        EEPROM.commit();
        Serial.println("[MENU] - Ingredient " + String(ingredients[z]) + " set to " + String(y));

        Mode = 0;
        _Mode = 99; //force update 
        buttonPressed = 0;
        encoderPos = 0;
        Level = 0;
        lcd.clear();
    }
  }



} 

// Carry out common activities each time a setting is changed
void setAdmin(byte name, byte setting){
  Serial.print("[MENU] - Setting "); //DEBUGGING
  Serial.print(name); //DEBUGGING
  Serial.print(" = "); //DEBUGGING
  Serial.println(setting);//DEBUGGING
  encoderPos = 0; // reorientate the menu index - optional as we have overflow check code elsewhere
  buttonPressed = 0; // reset the button status so one press results in one action
  Mode = 0; // go back to top level of menu, now that we've set values
  Serial.println("[MENU] - Main Menu"); //DEBUGGING
}

//Rotary encoder interrupt service routine for one encoder pin
void PinA(){
  cli(); //stop interrupts happening before we read pin values
  //Serial.println("reading interrupt pinA");
  reading = 0;
  if (digitalRead(pinA)) reading+=4;
  if (digitalRead(pinB)) reading+=8;
  if(reading == B00001100 && aFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos --; //decrement the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00000100) bFlag = 1; //signal that we're expecting pinB to signal the transition to detent from free rotation
  sei(); //restart interrupts
}

//Rotary encoder interrupt service routine for the other encoder pin
void PinB(){
  cli(); //stop interrupts happening before we read pin values
  //Serial.println("reading interrupt pinB");
  reading = 0;
  if (digitalRead(pinA)) reading+=4;
  if (digitalRead(pinB)) reading+=8;
  if (reading == B00001100 && bFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos ++; //increment the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00001000) aFlag = 1; //signal that we're expecting pinA to signal the transition to detent from free rotation
  sei(); //restart interrupts
}

void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  Serial.println("[WEB] - Recieved from Webserver");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
  Serial.println(d);
  if(d == "TOGGLE PUMPS"){
    if(safety_relay == LOW){
      safety_relay = HIGH;
      digitalWrite(powerRelay, HIGH);
      DEBUG("PUMPS OFF");
    }else{
      safety_relay = LOW;
      digitalWrite(powerRelay, LOW);
      DEBUG("PUMPS ON");
    }
  }
  if(d == "COCKTAIL COUNT"){
    DEBUG("TOTALS");
    DEBUG("cocktails made: " + String(madeCount));
    DEBUG("made since on: " + String(madeCount_sinceOn));
  }
  if(d == "VERSION"){
    DEBUG("CURRENT VERSION");
    DEBUG(machineversion);
  }
  if(d == "PUMP TEST"){
    if(safety_relay == LOW){
      digitalWrite(powerRelay, HIGH);
    }
    for(int b = 0; b < pump_count; b++){
      DEBUG("Testing pump: " + String(b));
      digitalWrite(pump_pin[b], LOW);
      delay(200);
      digitalWrite(pump_pin[b], HIGH);
      delay(50);
    }
    if(safety_relay == LOW){
      digitalWrite(powerRelay, LOW);
    }
  }
  if(d == "PRIME ALL"){
    for(int b = 0; b < pump_count; b++){
      DEBUG("Priming pump: " + String(b));
      digitalWrite(pump_pin[b], LOW);
      delay(500);
      digitalWrite(pump_pin[b], HIGH);
      delay(50);
    }
  }
}

void make(byte n){
  powerLED("red");
  ringLED(4);
  lcd_center("POURING A",0);
  lcd_center(cocktails[n],1);
  madeCount_sinceOn++;
  madeCount++;
  Serial.println("[BARBOT] - Pouring a " + String(cocktails[n]));
  for(int i = 0; i < ingredient_count; i++){
    // i = ingredient position
    // a = amount
    // x = ingredient
    // p = pump integer
    // l = pump number
    int a = recipe[n][i]; //amount
    String x = ingredients[i]; //title of ingredient
    int q;
    //search for pump
    if(a != 0){ //check if we have some to pour
      for(q = 0; q < pump_count; q++){
        if(pump_i[q] == i){
          pour(a,q,x,i);
          break;
        }else{
          if((q+1) == pump_count){
            Serial.println("[BARBOT] - " + x + " not found");
            lcd_center("add " + String(a) + "ml " + x, 2);
            lcd_center("yourself", 3);
            delay(2500);
          }
        }
      }
    }
    delay(10);
  }
  lcd.clear();
  powerLED("green");
  lcd_center("READY",1);
  //ringLED(2);
  rainbow(10);
  EEPROM.write(EEPROM_MADE,madeCount);
  EEPROM.commit();
  lcd.clear();
}

void pour(int qty, int pump_n, String x, int i){


lcd_center(String(qty) + "ml of " + x,2);


float aps = flowrates[i];
float sum = qty / (aps / 60);
float t = sum * 1000;

unsigned long cMillis = 0;
unsigned long sMillis = millis();
unsigned long pMillis = sMillis + t;
//float rSec;
//DEBUG("aps: " + String(aps) + " qty: " + String(qty) + " sum: " + String(sum) + " t: " + String(t) + " sMillis: " + String(sMillis) + " pMillis: " + String(pMillis));
Serial.println("[BARBOT] - POURING " + String(qty) + "ml of " + x + " from pump " + String(pump_n) + " for " + String(sum) + "s");
lcd_center(String(sum) + "s",3);
//turn pump_n on#
digitalWrite(pump_pin[pump_n], LOW);
//Serial.print("pump pin output on: " + String(pump_pin[pump_n]));
while (cMillis < pMillis){
  //ping
  cMillis = millis();
  //rSec = ((pMillis/1000) - (cMillis/1000));
  //lcd_center(String(rSec) + "s",3);
  delay(100);
}
digitalWrite(pump_pin[pump_n], HIGH);
//turn pump_n off


}

void DEBUG(String print){

  if(debug_output == true){
    Serial.println("[DEBUG] - " + print);
    WebSerial.println("[DEBUG] - " + print);
  }

}

void ping_websocket() {

  cpingMillis = millis();

  if (cpingMillis - lpingMillis >= pingInterval) {
    lpingMillis = cpingMillis;
    if(pingServer==true){
      webSocket.sendTXT("\"heartbeat\":\"keepalive\"");
      DEBUG("Sent ping to websocket");
    }
  }

}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, c);
    pixels.show();
    delay(wait);
  }
}

void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 768) {
    for(int i=0; i<pixels.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / pixels.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
    }
    pixels.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}



//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < pixels.numPixels(); i=i+3) {
        pixels.setPixelColor(i+q, c);    //turn every third pixel on
      }
      pixels.show();

      delay(wait);

      for (uint16_t i=0; i < pixels.numPixels(); i=i+3) {
        pixels.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      pixels.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<pixels.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / pixels.numPixels();
        uint32_t color = pixels.gamma32(pixels.ColorHSV(hue)); // hue -> RGB
        pixels.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      pixels.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}
