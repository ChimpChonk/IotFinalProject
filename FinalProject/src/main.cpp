/**
 * @file main.cpp
 * @brief Main file for the ESP32 temperature monitoring and web server project.
 */

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
#include <ESPmDNS.h>
#include <Arduino_Json.h>

//AsyncWebServer port
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//Function Prototypes
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
void notifyClients(String csvData);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void initWebSocket();

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

/**
 * @brief Setup function for initializing the system.
 */
void setup(){
    Serial.begin(115200);
    initLittleFS();
    initSDCard();
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    ip = readFile(LittleFS, ipPath);
    gateway = readFile(LittleFS, gatewayPath);

    Serial.println(ssid);
    Serial.println(ip);
    Serial.println(gateway);

    if(initWiFi()){
        Serial.println("HTTP server started");
        if(!MDNS.begin("apeesp")){
            Serial.println("Error setting up MDNS");
            while (1)
            {
                delay(1000);
            }
        }
        initWebSocket();
        Serial.println("mDNS responder started");
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
            request->send(200, "text/plain", "Config cleared");
        });

        server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(SD, "/data.csv", "text/csv", true);
        });

        server.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request){
          File file = SD.open("/data.csv", FILE_READ);
          if (file){
            String data = file.readString();
            request->send(200, "text/csv", data);
          }
          else{
            request->send(404, "text/plain", "File not found");
          }
        });

        server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
          SD.remove("/data.csv");
          request->send(200, "text/plain", "File deleted");
        });
    server.begin();
  }
    else {
        Serial.println("Setting AP (Access Point)");
        WiFi.softAP("Ape-ESP-AP", NULL);

        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP); 

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send(LittleFS, "/wifimanager.html", "text/html");
        });

        server.serveStatic("/", LittleFS, "/");

        server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
          int params = request->params();
          for(int i=0;i<params;i++){
            AsyncWebParameter* p = request->getParam(i);
            if(p->isPost()){
              if (p->name() == PARAM_INPUT_1) {
                ssid = p->value().c_str();
                Serial.print("SSID set to: ");
                Serial.println(ssid);
                writeFile(LittleFS, ssidPath, ssid.c_str());
              }
              if (p->name() == PARAM_INPUT_2) {
                pass = p->value().c_str();
                Serial.print("Password set to: ");
                Serial.println(pass);
                writeFile(LittleFS, passPath, pass.c_str());
              }
              if (p->name() == PARAM_INPUT_3) {
                ip = p->value().c_str();
                Serial.print("IP Address set to: ");
                Serial.println(ip);
                writeFile(LittleFS, ipPath, ip.c_str());
              }
              if (p->name() == PARAM_INPUT_4) {
                gateway = p->value().c_str();
                Serial.print("Gateway set to: ");
                Serial.println(gateway);
                writeFile(LittleFS, gatewayPath, gateway.c_str());
              }
            }
        }
        request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
        delay(3000);
        ESP.restart();
    });
    server.begin();
  }

}

/**
 * @brief Main loop function for periodic tasks.
 */
void loop(){
    if((millis() - lastTime) > timerDelay){
        temperatureC = readDSTemperatureC();
        lastTime = millis();
        saveData();
    }
    getTimeStamp();
    ws.cleanupClients();
}

/**
 * @brief Initialize LittleFS.
 */
void initLittleFS()
{
  if(!LittleFS.begin(true))
  {
    Serial.println("Error mounting file");
  }
  Serial.println("Mounting success");
}

/**
 * @brief Read a file from LittleFS.
 * 
 * @param fs File system instance.
 * @param path Path to the file.
 * @return Content of the file as a String.
 */
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

/**
 * @brief Write a file to LittleFS.
 * 
 * @param fs File system instance.
 * @param path Path to the file.
 * @param message Content to write.
 */
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

/**
 * @brief Initialize WiFi connection.
 * 
 * @return True if WiFi is initialized successfully, otherwise false.
 */
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

/**
 * @brief Clear WiFi configuration.
 */
void clearWifiConfig(){
  Serial.println("Clearning WiFi Config.....");
  LittleFS.remove(ssidPath);
  LittleFS.remove(passPath);
  LittleFS.remove(ipPath);
  LittleFS.remove(gatewayPath);

  ESP.restart();
}

/**
 * @brief Read temperature from DS18B20 sensor in Celsius.
 * 
 * @return Temperature value as a String.
 */
String readDSTemperatureC() {
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

/**
 * @brief Process function for web server response.
 * 
 * @param var Variable name to process.
 * @return Processed value as a String.
 */
String processor(const String& var){
  if(var == "TEMPERATUREC"){
    return temperatureC;
  }
  return String();
}

/**
 * @brief Get current date and time stamp.
 * 
 * @return True if timestamp is obtained successfully, otherwise false.
 */
bool getTimeStamp() {
  if (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(500);
  }

  if (!timeClient.update()) {
    return false;
  }

  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  if (splitT == -1) {
    return false;
  }

  dayStamp = formattedDate.substring(0, splitT);
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);

  return true;
}

/**
 * @brief Initialize the SD card.
 */
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

/**
 * @brief Save temperature data to the SD card.
 */
void saveData(){
  File dataFile = SD.open("/data.csv", FILE_APPEND);

 if(!dataFile){
    Serial.println("Error opening data.CSV file");
    return;
 }
 
  if (dataFile.available()) {
    dataFile.seek(0);
  }
  String data = dayStamp + " " + timeStamp + "," + readDSTemperatureC();
  dataFile.print(dayStamp + " " + timeStamp);
  dataFile.print(",");
  dataFile.println(readDSTemperatureC());
  notifyClients(data);
  dataFile.close();
}

/**
 * @brief Notify all WebSocket clients with temperature data.
 * 
 * @param csvData Temperature data in CSV format.
 */
void notifyClients(String csvData) {
  ws.textAll(csvData);
}

/**
 * @brief Handle WebSocket message.
 * 
 * @param arg Pointer to the WebSocket frame info.
 * @param data Pointer to the message data.
 * @param len Length of the message.
 */
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
  }
}

/**
 * @brief Handle WebSocket events.
 * 
 * @param server Pointer to the WebSocket server.
 * @param client Pointer to the WebSocket client.
 * @param type Type of WebSocket event.
 * @param arg Pointer to additional event data.
 * @param data Pointer to event data.
 * @param len Length of the event data.
 */
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

/**
 * @brief Initialize WebSocket server.
 */
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}
