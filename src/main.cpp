/*
  ESP32 NodeMCU WiFi Board Test

  Includes parts from
  http://www.arduino.cc/en/Tutorial/Button
*/

#include <Arduino.h>
#include <IRremote.h>
#include <LiquidCrystal.h>
#include <DHT.h>
#include <IotWebConf.h>

#define IR_BUTTON_POWER 0xffa25d
#define IR_BUTTON_0 0xff6897
#define IR_BUTTON_1 0xff30cf
#define IR_BUTTON_2 0xff18e7
#define IR_BUTTON_3 0xff7a85
#define IR_BUTTON_4 0xff10ef
#define IR_BUTTON_5 0xff38c7
#define IR_BUTTON_6 0xff5aa5
#define IR_BUTTON_7 0xff42bd
#define IR_BUTTON_8 0xff4ab5
#define IR_BUTTON_9 0xff52ad
#define IR_BUTTON_UP 0xff906f
#define IR_BUTTON_DOWN 0xffe01f
#define IR_BUTTON_VOLUP 0xff629d
#define IR_BUTTON_VOLDOWN 0xffa857
#define IR_BUTTON_PLAY 0xff02fd
#define IR_BUTTON_PREV 0xff22dd
#define IR_BUTTON_NEXT 0xffc23d
#define IR_BUTTON_FUNC 0xffe21d
#define IR_BUTTON_EQ 0xff9867
#define IR_BUTTON_ST 0xffb04f

#define DHT_TYPE DHT22

// Define pins
const int LED_INTERNAL_PIN = 1;       // Internal LED 
const int RGB_RED_PIN = 15;           // RGB LED
const int RGB_GREEN_PIN = 2;
const int RGB_BLUE_PIN = 0;
const int PUSH_BUTTON_PIN = 34;       // Push button
const int IR_RECEIVE_PIN = 35;        // IR input 
const int RS_PIN = 22, EN_PIN = 23, D4_PIN = 5, D5_PIN = 18, D6_PIN = 19, D7_PIN = 21;   // LED pins
const int DHT_PIN = 32;               // DHT sensor 
const int LED_RED_PIN = 4;           // Red LED
const int LIGHT_SENSOR_PIN = 33;      // Light sensor

// Define LED PWM properties
const int LED_FREQ = 5000;
const int LED_RESOLUTION = 8;
const int LED_RED_CHANNEL = 0;
const int LED_GREEN_CHANNEL = 1;
const int LED_BLUE_CHANNEL = 2;

//
// Start IotWebConf
//
// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "testThing";
// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "password";
#define STRING_LEN 128
#define NUMBER_LEN 32
char intDhtIntervalParamValue[NUMBER_LEN];
char intStatusWaitParamValue[NUMBER_LEN];
char floatParamValue[NUMBER_LEN];
char stringParamValue[STRING_LEN];
DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);
IotWebConfSeparator separator1 = IotWebConfSeparator("Config Options");
IotWebConfParameter intDhtIntervalParam = IotWebConfParameter("DHT read interval", "intDhtIntervalParam", intDhtIntervalParamValue, NUMBER_LEN, "number", "1..1000s", NULL, "min='1' max='1000' step='1'");
IotWebConfParameter intStatusWaitParam = IotWebConfParameter("Status wait time", "intStatusWaitParam", intStatusWaitParamValue, NUMBER_LEN, "number", "1..100s", NULL, "min='1' max='100' step='1'");
// -- We can add a legend to the separator
IotWebConfSeparator separator2 = IotWebConfSeparator("IotWebConf Test");
IotWebConfParameter stringParam = IotWebConfParameter("String param", "stringParam", stringParamValue, STRING_LEN);
IotWebConfParameter floatParam = IotWebConfParameter("Float param", "floatParam", floatParamValue, NUMBER_LEN, "number", "e.g. 23.4", NULL, "step='0.1'");

//
// End IotWebConf
//

// Define global variables
unsigned long timeElapsed;
unsigned long dhtDelay = 10, dhtLastRead = 0;
float humidity = 0, temperature = 0;

// Define sensor functions
IRrecv irrecv(IR_RECEIVE_PIN);
decode_results irResults;
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);
DHT dht(DHT_PIN, DHT_TYPE);

// Define functions
void setup();
void loop();
void setRGB(int redValue, int greenValue, int blueValue);
void checkButton();
void checkIR();
void checkDHT();
void checkLight();
void showStatus();
void configSaved();
boolean formValidator();
void handleRoot();

/*************************************************
  
 setup()  
  - Run on board startup
 
*************************************************/
void setup() {
  Serial.begin(115200);
  Serial.println(F("startup() ***************************************"));
  Serial.println(F("Hello World!"));
  Serial.println(F("START " __FILE__ " from " __DATE__));  // Just to know which program is running on my Arduino
  
  // Initialize pins for input/output
  pinMode(LED_INTERNAL_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(PUSH_BUTTON_PIN, INPUT);
  pinMode(IR_RECEIVE_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);

  // Initialse DHT sensor
  dht.begin();

  // Initialse LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Hello world!");

  // Setup RGB LED on GPIO
  ledcSetup(LED_RED_CHANNEL, LED_FREQ, LED_RESOLUTION);
  ledcSetup(LED_GREEN_CHANNEL, LED_FREQ, LED_RESOLUTION);
  ledcSetup(LED_BLUE_CHANNEL, LED_FREQ, LED_RESOLUTION);
  ledcAttachPin(RGB_RED_PIN, LED_RED_CHANNEL);
  ledcAttachPin(RGB_GREEN_PIN, LED_GREEN_CHANNEL);
  ledcAttachPin(RGB_BLUE_PIN, LED_BLUE_CHANNEL);
  setRGB(0, 0, 0);

  // Setup IR Receiver
  // In case the interrupt driver crashes on setup, give a clue
  // to the user what's going on.
  Serial.println(F("Enabling IR Receiver"));
  irrecv.enableIRIn(); // Start the receiver

  // Initializing iotWebConf
  //iotWebConf.setStatusPin(LED_RED_PIN);
  iotWebConf.addParameter(&separator1);
  iotWebConf.addParameter(&intDhtIntervalParam);
  iotWebConf.addParameter(&intStatusWaitParam);
  iotWebConf.addParameter(&separator2);
  iotWebConf.addParameter(&stringParam);
  iotWebConf.addParameter(&floatParam);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.init();
  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  Serial.println("startup() complete *****************************************");
}

/*************************************************
  
 loop()  
  - Run continuously
 
*************************************************/
void loop() {
  timeElapsed = millis();

  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(11, 1);
  // print the number of seconds since reset:
  lcd.print(timeElapsed / 1000);

  checkDHT();
  checkButton();
  checkIR();

  // doLoop should be called as frequently as possible.
  iotWebConf.doLoop();

  delay(150);
}

//
//
//
//
//
// Support functions
//
//
//
//
//

/*************************************************
  
 setRGB()  
  - Take Red, Green and Blue inputs from 0-255 and write to RGB LED
  - Channels attached to pins during setup
 
*************************************************/
void setRGB(int redValue, int greenValue, int blueValue){
  if(redValue < 0){redValue = 0;}
  if(redValue > 255){redValue = 255;}
  if(greenValue < 0){greenValue = 0;}
  if(greenValue > 255){greenValue = 255;}
  if(blueValue < 0){blueValue = 0;}
  if(blueValue > 255){blueValue = 255;}

  ledcWrite(LED_RED_CHANNEL, redValue);
  ledcWrite(LED_GREEN_CHANNEL, greenValue);
  ledcWrite(LED_BLUE_CHANNEL, blueValue);

  Serial.print(F("Set RGB LED : Red "));
  Serial.print(redValue);
  Serial.print(F(" Green "));
  Serial.print(greenValue);
  Serial.print(F(" Blue "));
  Serial.println(blueValue);
}

/*************************************************
  
 checkButton()  
  - Check if the button at PUSH_BUTTON_PIN is pressed and take an action
 
*************************************************/
void checkButton(){
    int buttonState = 0;        // variable for reading the pushbutton status
  
  // read the state of the pushbutton value:
  buttonState = digitalRead(PUSH_BUTTON_PIN);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH) 
  {
    // Turn Internal LED on:
    digitalWrite(LED_INTERNAL_PIN, HIGH);
    digitalWrite(LED_RED_PIN, HIGH);
    Serial.print(F("Button pressed at millis() : "));
    Serial.println(timeElapsed);

    // Change RGB LED to Random colour
    setRGB(random(255), random(255), random(255));

    checkLight();
    delay(5000);
    checkDHT();

    digitalWrite(LED_RED_PIN, LOW);
  } 
  else 
  {
    // Turn Internal LED off:
    digitalWrite(LED_INTERNAL_PIN, LOW);
  }
}

/*************************************************
  
 checkDHT()  
  - Check the DHT sensor and take an action
 
*************************************************/
void checkDHT(){
  dhtDelay = atoi(intDhtIntervalParamValue);
  if(timeElapsed > (dhtLastRead + (dhtDelay * 1000)))
  {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    humidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    temperature = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    Serial.print(F("Humidity: "));
    Serial.print(String(humidity, 2));
    Serial.print(F("%  Temperature: "));
    Serial.print(String(temperature, 2));
    Serial.println(F("°C "));

    lcd.setCursor(0, 1);
    lcd.print(String(temperature, 1) + " " + String(humidity, 1));
    dhtLastRead = timeElapsed;
  }
}

/*************************************************
  
 checkLight()  
  - Check the light sensor and take an action
 
*************************************************/
void checkLight(){
  int lightVal;

  lightVal = analogRead(LIGHT_SENSOR_PIN);

  Serial.print(F("Light: "));
  Serial.println(lightVal);

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(String("Light ") + String(lightVal));
}

/*************************************************
  
 showStatus()  
  - Show the status of everything
 
*************************************************/
void showStatus(){
  int wait = atoi(intStatusWaitParamValue) * 1000;

  Serial.println(F("Status of Everything: "));
  digitalWrite(LED_RED_PIN, HIGH);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Temperature C"));
  lcd.setCursor(0, 1);
  lcd.print(String(temperature, 2));
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Humidity %"));
  lcd.setCursor(0, 1);
  lcd.print(String(humidity, 2));
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Light"));
  lcd.setCursor(0, 1);
  int lightVal = analogRead(LIGHT_SENSOR_PIN);
  lcd.print(lightVal);
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("RGB"));
  lcd.setCursor(0, 1);
  lcd.print(String(ledcRead(LED_RED_CHANNEL)) + " " + String(ledcRead(LED_GREEN_CHANNEL)) + " " + String(ledcRead(LED_BLUE_CHANNEL)));
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IoT AP SSID"));
  lcd.setCursor(0, 1);
  lcd.print(iotWebConf.getThingName());
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IoT AP pwd"));
  lcd.setCursor(0, 1);
  lcd.print(iotWebConf.getApPasswordParameter()->valueBuffer);
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IoT WiFi SSID"));
  lcd.setCursor(0, 1);
  lcd.print(iotWebConf.getWifiSsidParameter()->valueBuffer);
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IoT WiFi pwd"));
  lcd.setCursor(0, 1);
  lcd.print(iotWebConf.getWifiPasswordParameter()->valueBuffer);
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IoT State"));
  lcd.setCursor(0, 1);
  lcd.print(iotWebConf.getState());
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IoT intDhtIntervalParamValue"));
  lcd.setCursor(0, 1);
  lcd.print(intDhtIntervalParamValue);
  delay(wait);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IoT intStatusWaitParamValue"));
  lcd.setCursor(0, 1);
  lcd.print(intStatusWaitParamValue);
  delay(wait);

  digitalWrite(LED_RED_PIN, LOW);
}

/*************************************************
  
checkIR()  
  - Check if anything has been sent to the IR receiver and take an action based on result
 
*************************************************/
void checkIR(){
  if (irrecv.decode(&irResults)) 
  {
    switch(irResults.value)
    {
      case IR_BUTTON_POWER:
        Serial.println(F("IR Button Power"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Power"));        break;
      case IR_BUTTON_0:
        Serial.println(F("IR Button 0"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button 0"));
        setRGB(0, 0, 0);
        break;
      case IR_BUTTON_1:
        Serial.println(F("IR Button 1"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IR Button 1");
        setRGB(ledcRead(LED_RED_CHANNEL) + 10, ledcRead(LED_GREEN_CHANNEL), ledcRead(LED_BLUE_CHANNEL));
        break;
      case IR_BUTTON_2:
        Serial.println(F("IR Button 2"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IR Button 2");
        setRGB(ledcRead(LED_RED_CHANNEL), ledcRead(LED_GREEN_CHANNEL) + 10, ledcRead(LED_BLUE_CHANNEL));
        break;
      case IR_BUTTON_3:
        Serial.println(F("IR Button 3"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IR Button 3");
        setRGB(ledcRead(LED_RED_CHANNEL), ledcRead(LED_GREEN_CHANNEL), ledcRead(LED_BLUE_CHANNEL) + 10);
        break;
      case IR_BUTTON_4:
        Serial.println(F("IR Button 4"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IR Button 4");
        setRGB(ledcRead(LED_RED_CHANNEL) - 10, ledcRead(LED_GREEN_CHANNEL), ledcRead(LED_BLUE_CHANNEL));
        break;
      case IR_BUTTON_5:
        Serial.println(F("IR Button 5"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IR Button 5");
        setRGB(ledcRead(LED_RED_CHANNEL), ledcRead(LED_GREEN_CHANNEL) - 10, ledcRead(LED_BLUE_CHANNEL));
        break;
      case IR_BUTTON_6:
        Serial.println(F("IR Button 6"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IR Button 6");
        setRGB(ledcRead(LED_RED_CHANNEL), ledcRead(LED_GREEN_CHANNEL), ledcRead(LED_BLUE_CHANNEL) - 10);
        break;
      case IR_BUTTON_7:
        Serial.println(F("IR Button 7"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button 7"));
        setRGB(255, 0, 0);
        break;
      case IR_BUTTON_8:
        Serial.println(F("IR Button 8"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button 8"));
        setRGB(0, 255, 0);
        break;
      case IR_BUTTON_9:
        Serial.println(F("IR Button 9"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button 9"));
        setRGB(0, 0, 255);
        break;
      case IR_BUTTON_UP:
        Serial.println(F("IR Button Up"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Up"));
        break;
      case IR_BUTTON_DOWN:
        Serial.println(F("IR Button Down"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Down"));
        break;
      case IR_BUTTON_VOLUP:
        Serial.println(F("IR Button Vol+"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Vol+"));
        break;
      case IR_BUTTON_VOLDOWN:
        Serial.println(F("IR Button Vol-"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Vol-"));
        break;
      case IR_BUTTON_PLAY:
        Serial.println(F("IR Button Play"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Play"));
        break;
      case IR_BUTTON_PREV:
        Serial.println(F("IR Button Prev"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Prev"));
        break;
      case IR_BUTTON_NEXT:
        Serial.println(F("IR Button Next"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Next"));
        break;
      case IR_BUTTON_FUNC:
        Serial.println(F("IR Button Func"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button Func"));
        break;
      case IR_BUTTON_EQ:
        Serial.println(F("IR Button EQ"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button EQ"));
        break;
      case IR_BUTTON_ST:
        Serial.println(F("IR Button ST"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("IR Button ST"));
        showStatus();
        break;
      default:
        Serial.print(F("IR Unknown: HEX "));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IR Unknown");
        Serial.print(irResults.value, HEX);
        Serial.print(F(" DEC "));
        Serial.println(irResults.value, DEC);
        break;
    }

    irrecv.resume(); // Receive the next value
  }
}

/*************************************************
  
configSaved()  
  - Part of IotWebConf
  - Handles web requests to "/" path.
 
*************************************************/
void configSaved()
{
  Serial.println("iotWebConf configuration was updated");
}

/*************************************************
  
formValidator()  
  - Part of IotWebConf
  - Handles web requests to "/" path.
 
*************************************************/
boolean formValidator()
{
  Serial.println("Validating form.");
  boolean valid = true;

  int l = server.arg(stringParam.getId()).length();
  if (l < 3)
  {
    stringParam.errorMessage = "Please provide at least 3 characters.";
    valid = false;
  }

  return valid;
}

/*************************************************
  
handleRoot()  
  - Part of IotWebConf
  - Handles web requests to "/" path.
 
*************************************************/
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>IotWebConf 03 Custom Parameters</title></head><body>Hello world!";
  s += "<ul>";
  s += "<li>DHT read interval param value: ";
  s += atoi(intDhtIntervalParamValue);
  s += "<li>Status wait param value: ";
  s += atoi(intStatusWaitParamValue);
  s += "<li>String param value: ";
  s += stringParamValue;
  s += "<li>Float param value: ";
  s += atof(floatParamValue);
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}