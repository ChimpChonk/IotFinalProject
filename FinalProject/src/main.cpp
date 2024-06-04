#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "FS.h"
#include <LittleFS.h>
#include "SD.h"
#include "SPI.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NTPClient.h>

//AsyncWebServer port
AsyncWebServer server(80);

//prototypes
String readDSTemperatureC();
String processor(const String& var);
bool getTimeStamp();
void saveData();
void initSDCard();
void clearWifiConfig();
bool initWiFi();
void writeFile(fs::FS &fs, const char * path, const char * message);
String readFile(fs::FS &fs, const char * path);
void initLittleFS();

//Temp here --------------------------------------------------
//Temp data wire define
#define ONE_WIRE_BUS 4

//One wire instance
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
String temperatureC = "";

unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;
//---------------------------------------------------------
//Wifi Config-------------------------------------------------
//Search parameter in HTTP post request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

//Var save values for HTML form
String ssid;
String pass;
String ip;
String gateway;

//File path save for wifi config values
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt"; 

//local IP
IPAddress localIP;

//local gaveway
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);
IPAddress dns(8,8,8,8);
unsigned long previousMillis = 0;
const long interval = 10000;
//------------------------------------------------------------

//datetime-------------------------------------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "87.104.58.9", 3600, 60000);
String formattedDate;
String dayStamp;
String timeStamp;
//------------------------------------------------------------

void setup(){
    Serial.begin(115200);
    initLittleFS();
    initSDCard();
    // ssid = readFile(LittleFS, ssidPath);
    // pass = readFile(LittleFS, passPath);
    // ip = readFile(LittleFS, ipPath);
    // gateway = readFile(LittleFS, gatewayPath);

    //only for development
    ssid = "SSID";
    pass = "PASS";
    ip = "192.168.1.169";
    gateway = "192.168.1.1";

    Serial.println(ssid);
    Serial.println(ip);
    Serial.println(gateway);


    if(initWiFi()){
        Serial.println("HTTP server started");
        timeClient.begin();
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/index.html", "text/html");
        });

        server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/style.css","text/css");
        });
        server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/index.js","text/javascript");
        });

        server.on("/clearconfig", HTTP_GET, [](AsyncWebServerRequest *request){
            clearWifiConfig();
        });

        server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
          File file = SD.open("/data.csv", FILE_READ);
          if(file){
            request->send(SD, "/data.csv", "text/csv");
            file.close();
          }
          else{
            request->send(404, "text/plain", "File not found");
          }
        });

        server.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request){
          File file = SD.open("/data.csv", FILE_READ);
          if (file){
            String data = file.readString();
            request->send(200, "application/json", data);
          }
          else{
            request->send(404, "text/plain", "File not found");
          }
        });
    server.begin();
  }
    else {
        // Connect to Wi-Fi network with SSID and password
        Serial.println("Setting AP (Access Point)");
        // NULL sets an open Access Point
        WiFi.softAP("Ape-ESP-AP", NULL);

        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP); 

        // Web Server Root URL
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send(LittleFS, "/wifimanager.html", "text/html");
        });

        server.serveStatic("/", LittleFS, "/");

        server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
          int params = request->params();
          for(int i=0;i<params;i++){
            AsyncWebParameter* p = request->getParam(i);
            if(p->isPost()){
              // HTTP POST ssid value
              if (p->name() == PARAM_INPUT_1) {
                ssid = p->value().c_str();
                Serial.print("SSID set to: ");
                Serial.println(ssid);
                // Write file to save value
                writeFile(LittleFS, ssidPath, ssid.c_str());
              }
              // HTTP POST pass value
              if (p->name() == PARAM_INPUT_2) {
                pass = p->value().c_str();
                Serial.print("Password set to: ");
                Serial.println(pass);
                // Write file to save value
                writeFile(LittleFS, passPath, pass.c_str());
              }
              // HTTP POST ip value
              if (p->name() == PARAM_INPUT_3) {
                ip = p->value().c_str();
                Serial.print("IP Address set to: ");
                Serial.println(ip);
                // Write file to save value
                writeFile(LittleFS, ipPath, ip.c_str());
              }
              // HTTP POST gateway value
              if (p->name() == PARAM_INPUT_4) {
                gateway = p->value().c_str();
                Serial.print("Gateway set to: ");
                Serial.println(gateway);
                // Write file to save value
                writeFile(LittleFS, gatewayPath, gateway.c_str());
              }
              //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }
        request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
        delay(3000);
        ESP.restart();
    });
    server.begin();
  }

}

void loop(){

    if((millis() - lastTime) > timerDelay){
        temperatureC = readDSTemperatureC();
        lastTime = millis();
        saveData();
    }
    getTimeStamp();

}


//init LittleFS-------------------------------------
void initLittleFS()
{
  
  if(!LittleFS.begin(true))
  {
    Serial.println("Error mounting file");
  }
  
  Serial.println("Mounting success");
}

//Reading file from littleFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
  }
  return fileContent;
}

//Writing file to littleFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}
//--------------------------------------------------

//WiFi init-----------------------------------------------------
//wifi init
bool initWiFi(){
  if(ssid == "" || ip == ""){
    Serial.println("Undefined SSID or IP");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if(!WiFi.config(localIP, localGateway, subnet)){
    Serial.println("Failed to configure");
    return false;
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  while(WiFi.status() != WL_CONNECTED){
    if(millis() - currentMillis >= interval){
      Serial.println("Failed to connect");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

void clearWifiConfig(){
  Serial.println("Clearning WiFi Config.....");
  LittleFS.remove(ssidPath);
  LittleFS.remove(passPath);
  LittleFS.remove(ipPath);
  LittleFS.remove(gatewayPath);

  ESP.restart();
}
//--------------------------------------------------------------


//Temp reading -----------------
String readDSTemperatureC() {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);

  if(tempC == -127.00) {
    Serial.println("Failed to read from DS18B20 sensor");
    return "--";
  } else {
    Serial.print("Temperature Celsius " + dayStamp +":"+ timeStamp + " : ");
    Serial.println(tempC); 
  }
  return String(tempC);
}
//-------------------------------------------

//process -----------------------------------
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATUREC"){
    return temperatureC;
  }
  return String();
}
//------------------------------------

//DateTime----------------------------------------
bool getTimeStamp() {
  // Update timeClient and wait for a response
  if (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(500); // Rest of the function handles time synchronization and parsing
  }

  // Tjek igen efter forceUpdate
  if (!timeClient.update()) {
    return false; // Returner false, hvis tiden stadig ikke er opdateret
  }

  // Uddrag dato og tid
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  if (splitT == -1) {
    return false; // Returner false, hvis formateringen er forkert
  }

  dayStamp = formattedDate.substring(0, splitT);
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);

  return true; // Gyldig tidspunkt modtaget
}
//-------------------------------------------------------

//Save date on sd card-------------------------------------
void initSDCard(){
  if(!SD.begin()){
    Serial.println("Card mount Failed");
    return;
  }
  File dataFile = SD.open("/data.csv", FILE_WRITE);
  dataFile.println("Time,Temperature");

  if(!dataFile){
    Serial.println("Error opening data.CSV file");
    return;
  }
  dataFile.close();
}

void saveData(){
  File dataFile = SD.open("/data.csv", FILE_APPEND);

 if(!dataFile){
    Serial.println("Error opening data.CSV file");
    return;
 }
 
  if (dataFile.available()) {
    dataFile.seek(0);
  }

  dataFile.print(dayStamp + " " + timeStamp);
  dataFile.print(",");
  dataFile.println(readDSTemperatureC());
  dataFile.close();
}
//------------------------------------------------------------