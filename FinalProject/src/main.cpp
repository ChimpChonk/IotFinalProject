#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>
//AsyncWebServer port
AsyncWebServer server(80);

//Search parameter in HTTP post request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAm_INPUT_4 = "gateway";

//Var save values for HTML form
String ssid;
String pass;
String ip;
String gateway;


//File path save for wifi config values
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPaht = "/ip.txt";
const char* gatewayPath = "gateway.txt"; 

//local IP
IPAddress localIP;

//local gaveway
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);

//init littleFS
void initLittleFS()
{
  
  if(!LittleFS.begin(true))
  {
    Serial.println("Error mounting file");
  }
  
  Serial.println("Mounting success");
}

void setup() {
  Serial.begin(115200);
  initLittleFS();
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}
