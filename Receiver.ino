// Import libraries
#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>

//Libraries for OLED Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Libraries to get time from NTP Server
#include <NTPClient.h>
#include <WiFiUdp.h>

// Libraries for SD card
#include "FS.h"
#include "SD.h"

//OLED pins
#define OLED_SDA 21
#define OLED_SCL 22 
#define OLED_RST -1
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Replace with your network credentials
const char* ssid     = "redmi1";
const char* password = "98765432o";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// LoRa Module pin definition
// define the pins used by the LoRa transceiver module
#define ss 5
#define rst 14
#define dio0 2

const int sw1 = 16;
const int sw2 = 4;

// Initialize variables to get and save LoRa data
int rssi;
String loRaMessage;
String temperature;
String soilMoisture;
String batteryLevel;
String readingID;

// Define CS pin for the SD card module
#define SD_CS 15

// Set web server port number to 80
WiFiServer server(80);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// Variable to store the HTTP request
String header;

//Initialize OLED display
void startOLED(){
  //reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
 
  //initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("LORA RECEIVER");
}

void setup() { 
  // Initialize Serial Monitor
  Serial.begin(115200);
  startOLED();

  pinMode(sw1,INPUT);
  pinMode(sw2,INPUT);

  // Initialize LoRa
  int counter;
  //replace the LoRa.begin(---E-) argument with your location's frequency 
  //note: the frequency should match the sender's frequency
  //433E6 for Asia
  //866E6 for Europe
  //915E6 for North America
  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(433E6)) {
    Serial.println(".");
    counter++;
    delay(500);
  }
  if (counter == 10) {
    // Increment readingID on every new reading
    Serial.println("Starting LoRa failed!"); 
  }
  // Change sync word (0xF3) to match the sender
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  LoRa.setSpreadingFactor(12);           // ranges from 6-12,default 7 see API docs
  LoRa.setSignalBandwidth(62.5E3 );           // for -139dB (page - 112)
  LoRa.setCodingRate4(8);                   // for -139dB (page - 112)
  LoRa.setSyncWord(0xF3);           
  Serial.println("LoRa Initializing OK!");

  //Oled
  display.setCursor(0,10);
  display.clearDisplay();
  display.print("LoRa Initializing OK!");
  display.display();
  delay(2000);
  
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.setCursor(0,20);
  display.print("Access web server at: ");
  display.setCursor(0,30);
  display.print(WiFi.localIP());
  display.display();
  server.begin();

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(25200);
   
  // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Reading ID, Date, Hour, Temperature, Soil Moisture (0-4095), RSSI, Battery Level(0-100)\r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
}

  int priviousSwitchValue1 = 1;
  int priviousSwitchValue2 = 1;
  int liveSwitchValue1 = 0;
  int liveSwitchValue2 = 0;
  bool switchPressFlag1 = false;
  bool switchPressFlag2 = false;
  
  int data = 1;

void loop() {

  sendInf ();
  
  // Check if there are LoRa packets available
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    getLoRaData();
    getTimeStamp();
    logSDCard();
  }
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the table 
            client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial;}");
            client.println("table { border-collapse: collapse; width:35%; margin-left:auto; margin-right:auto; }");
            client.println("th { padding: 12px; background-color: #0043af; color: white; }");
            client.println("tr { border: 1px solid #ddd; padding: 12px; }");
            client.println("tr:hover { background-color: #bcbcbc; }");
            client.println("td { border: none; padding: 12px; }");
            client.println(".sensor { color:white; font-weight: bold; background-color: #bcbcbc; padding: 1px; }");
            
            // Web Page Heading
            client.println("</style></head><body><h1>Soil Monitoring with LoRa </h1>");
            client.println("<h3>Last update: " + timeStamp + "</h3>");
            client.println("<table><tr><th>MEASUREMENT</th><th>VALUE</th></tr>");
            client.println("<tr><td>Temperature</td><td><span class=\"sensor\">");
            client.println(temperature);
            client.println(" *C</span></td></tr>");  
            // Uncomment the next line to change to the *F symbol
            //client.println(" *F</span></td></tr>");  
            client.println("<tr><td>Soil moisture (0-4095)</td><td><span class=\"sensor\">");
            client.println(soilMoisture);
            client.println("</span></td></tr>"); 
            client.println("<tr><td>Battery level</td><td><span class=\"sensor\">");
            client.println(batteryLevel);
            client.println(" %</span></td></tr>"); 
            client.println("<p>LoRa RSSI: " + String(rssi) + "</p>");
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  
}

// Read LoRa packet and get the sensor readings
void getLoRaData() {
  Serial.print("Lora packet received: ");
  // Read packet
  while (LoRa.available()) {
    String LoRaData = LoRa.readString();
    // LoRaData format: readingID/temperature&soilMoisture#batterylevel
    // String example: 1/27.43&654#95.34
    Serial.print(LoRaData); 
    
    // Get readingID, temperature and soil moisture
    int pos1 = LoRaData.indexOf('/');
    int pos2 = LoRaData.indexOf('&');
    int pos3 = LoRaData.indexOf('#');
    readingID = LoRaData.substring(0, pos1);
    temperature = LoRaData.substring(pos1 +1, pos2);
    soilMoisture = LoRaData.substring(pos2+1, pos3);
    batteryLevel = LoRaData.substring(pos3+1, LoRaData.length());    
  }
  // Get RSSI
  rssi = LoRa.packetRssi();
  Serial.print(" with RSSI ");    
  Serial.println(rssi);
}

// Function to get date and time from NTPClient
void getTimeStamp() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2022-06-25T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("2022-04-13");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  Serial.println(timeStamp);
}

// Write the sensor readings on the SD card
void logSDCard() {
  loRaMessage = String(readingID) + "," + String(dayStamp) + "," + String(timeStamp) + "," + 
                String(temperature) + "," + String(soilMoisture) + "," + String(rssi) + "," + String(batteryLevel) + "\r\n";
  appendFile(SD, "/data.txt", loRaMessage.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void sendInf ()
{
  liveSwitchValue1 = digitalRead(sw1);
//  Serial.println(liveSwitchValue1);
  if( (liveSwitchValue1 == 0) and (switchPressFlag1 == false) )
  {
    delay(50);
    data = 11;
//    Serial.println("11");
    switchPressFlag1 = true;
    priviousSwitchValue1 = !priviousSwitchValue1;

    LoRa.beginPacket();  
    LoRa.print(data);
    LoRa.endPacket();
  }
  if( (liveSwitchValue1 == 1) and (switchPressFlag1 == true) )
  {
    delay(50);
    data = 22;
//    Serial.println("22");
    switchPressFlag1 = false;
    LoRa.beginPacket();  
    LoRa.print(data);
    LoRa.endPacket();
  }
  
  liveSwitchValue2 = digitalRead(sw2);
  if( (liveSwitchValue2 == 0) and (switchPressFlag2 == false))
  {
    delay(50);
    data = 33;
//    Serial.println("33");
    switchPressFlag2 = true;
    priviousSwitchValue2 = !priviousSwitchValue2;
    LoRa.beginPacket();  
    LoRa.print(data);
    LoRa.endPacket();
  }
  if( (liveSwitchValue2 == 1) and (switchPressFlag2 == true) )
  {
    delay(50);
    data = 44;
//    Serial.println("44");
    switchPressFlag2 = false;
    LoRa.beginPacket();  
    LoRa.print(data);
    LoRa.endPacket();
  }
}
