/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/

// Libraries for LoRa Module
#include <SPI.h>            
#include <LoRa.h>

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2);

//DS18B20 libraries
#include <OneWire.h>
#include <DallasTemperature.h>

// LoRa Module pin definition
// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

const int LED1 = 16;   // indicator LED
const int LED2 = 4;

String inString = "";    // string to hold input
int val = 0;


// LoRa message variable
String message;

// Save reading number on RTC memory
RTC_DATA_ATTR int readingID = 0;

// Define deep sleep options
//uint64_t uS_TO_S_FACTOR = 1000000;  // Conversion factor for micro seconds to seconds
//// Sleep for 30 minutes = 0.5 hours  = 1800 seconds
//uint64_t TIME_TO_SLEEP = 10;

// Data wire is connected to ESP32 GPIO15
#define ONE_WIRE_BUS 33
// Setup a oneWire instance to communicate with a OneWire device
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Moisture Sensor variables
const int moisturePin = 12;
const int moisturePowerPin = 27;
int soilMoisture;

// Temperature Sensor variables
float tempC;
float tempF;

//Variable to hold battery level;
float batteryLevel;
const int batteryPin = 15;
int timecho = 20000;
unsigned long hientai = 0;
unsigned long thoigian;


void setup() {
  pinMode(moisturePowerPin, OUTPUT);

  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  
  digitalWrite(LED1 , LOW);
  digitalWrite(LED2 , LOW);
  
  // Start serial communication for debugging purposes
  Serial.begin(115200);

  lcd.init(); //Khởi động LCD                    
  lcd.backlight(); //Mở đèn

  // Enable Timer wake_up
//  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Start the DallasTemperature library
  sensors.begin();
  
  // Initialize LoRa
  //replace the LoRa.begin(---E-) argument with your location's frequency 
  //note: the frequency should match the sender's frequency
  //433E6 for Asia
  //866E6 for Europe
  //915E6 for North America
  LoRa.setPins(ss, rst, dio0);
  Serial.println("initializing LoRa");
  
  int counter = 0;  
  while (!LoRa.begin(433E6) && counter < 10) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == 100) {
    // Increment readingID on every new reading
    readingID++; 
//     Start deep sleep
    Serial.println("Failed to initialize LoRa. Going to sleep now");
    esp_deep_sleep_start();
  }
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  LoRa.setSpreadingFactor(12);           // ranges from 6-12,default 7 see API docs
  LoRa.setSignalBandwidth(62.5E3 );           // for -139dB (page - 112)
  LoRa.setCodingRate4(8);                   // for -139dB (page - 112)
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa initializing OK!");

  getReadings();
  Serial.print("Battery level = "); 
  Serial.println(batteryLevel, 2);
  Serial.print("Soil moisture = ");
  Serial.println(soilMoisture);
  Serial.print("Temperature Celsius = ");
  Serial.println(tempC);
  Serial.print("Temperature Fahrenheit = ");
  Serial.println(tempF);
  Serial.print("Reading ID = ");
  Serial.println(readingID);

  lcd.setCursor(0,0);
  lcd.print("S:");
  lcd.print(soilMoisture);
//  lcd.print("-%");
  lcd.setCursor(0,1);
  lcd.print("T|C/F:");
  lcd.print(tempC);
  lcd.print("-C");
//  lcd.print(tempF);
//  lcd.println("-F");
  
  
  sendReadings();
  Serial.print("Message sent = ");
  Serial.println(message);
  
  // Increment readingID on every new reading
  readingID++;
   thoigian= millis();
if(thoigian - hientai > timecho)
 {
  hientai = millis();
   ESP.restart();
   Serial.println("Restarting in 20 secons");
 }
  // Start deep sleep
//  Serial.println("DONE! Going to sleep now.");
//   
//  esp_deep_sleep_start();
} 

  bool i=0;
  int priviousValue = 0;
  int liveValue = 0;

void loop() {
  // The ESP32 will be in deep sleep
  // it never reaches the loop()
  
  int packetSize = LoRa.parsePacket();
  if (packetSize) { 
    // read packet    
    while (LoRa.available())
    {
      int inChar = LoRa.read();
      inString += (char)inChar;
      val = inString.toInt();  
      digitalWrite(LED1 , HIGH);
      delay(10);
      digitalWrite(LED1 , LOW);
      delay(10);    
      digitalWrite(LED2 , HIGH);
      delay(10);
      digitalWrite(LED2 , LOW);
      delay(10); 
    }
    inString = "";     
    LoRa.packetRssi();    
  }
  
//  Serial.println(val);  
  liveValue = val;
    
  if(priviousValue != liveValue)
  {
    priviousValue = liveValue;
  
    if(val == 11)
    {
      digitalWrite(LED1 , LOW);
    }
    
    if(val == 22)
    {
      digitalWrite(LED1 , HIGH);
    }
    if(val == 33)
    {
      digitalWrite(LED2 , LOW);
    }
    
    if(val == 44)
    {
      digitalWrite(LED2 , HIGH);
    }
    delay(50);
  }

}

void getReadings() {
  digitalWrite(moisturePowerPin, HIGH);
   
  // Measure temperature
  sensors.requestTemperatures(); 
  tempC = sensors.getTempCByIndex(0); // Temperature in Celsius
  tempF = sensors.getTempFByIndex(0); // Temperature in Fahrenheit
   
  // Measure moisture
  soilMoisture = analogRead(moisturePin);
  digitalWrite(moisturePowerPin, LOW);

  //Measure battery level
  batteryLevel = map(analogRead(batteryPin), 0.0f, 4095.0f, 0, 100);
}

void sendReadings() {
  // Send packet data
  // Send temperature in Celsius
  message = String(readingID) + "/" + String(tempC) + "&" + 
            String(soilMoisture) + "#" + String(batteryLevel);
  // Uncomment to send temperature in Fahrenheit
  //message = String(readingID) + "/" + String(tempF) + "&" + 
  //          String(soilMoisture) + "#" + String(batteryLevel);
  delay(1000);
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
}
