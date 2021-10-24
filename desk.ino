#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
 
#include "wifi-credentials.txt"

const uint16_t MAX_HEIGHT = 1300; //(1287)
const uint16_t MIN_HEIGHT = 750; // 640;

const int uppin = 2;
const int downpin = 0;

const int upBtnPin = 13;
const int downBtnPin = 12;
 
ESP8266WebServer server(80);
SoftwareSerial deskPositionSerial;


void move(int pin, int duration)
{
  digitalWrite(pin, LOW);
  delay(duration);
  digitalWrite(pin, HIGH);
}

void move2height( uint16_t targetHeight)
{
  const short hysteresis = 16;
  int diff = targetHeight - getDeskPosition();
  
  if (abs(diff) < hysteresis)
    return;

  if(diff > 0)
  {
    digitalWrite(uppin, LOW);
    while(getDeskPosition() < (targetHeight - hysteresis))
    {
      delay(10);
    }
    digitalWrite(uppin, HIGH);
    
  }else{
    digitalWrite(downpin, LOW);
    while(getDeskPosition() > (targetHeight + hysteresis))
    {
      delay(10);
    }
    digitalWrite(downpin, HIGH);
  }
  Serial.print("moved to ");
  Serial.println(getDeskPosition());
}

void handleMove2Height()
{
  const char* argName = "mm";
  if(!server.hasArg(argName))
  {
    server.send(400, "text/plain", F("missing required argument ms (duration in ms)\n"));
    return;
  }
  
  int targetHeight = server.arg(argName).toInt();
  if (targetHeight < MIN_HEIGHT)
   {
     server.send(400, "text/plain", String("height (mm) must be at least ") + MIN_HEIGHT + String ("\n"));
     return;
   }

  if (targetHeight > MAX_HEIGHT)
   {
     server.send(400, "text/plain", String("height (mm) must be at least ") + MAX_HEIGHT + String ("\n"));
     return;
   }
     
    move2height(targetHeight);
    String message = "Height set to ";
    message += getDeskPosition();
    server.send(200, "text/plain", message);
}

void handleMove(int pin)
{
  const char* argName = "ms";
  const int minDuration = 500;
  if(server.hasArg(argName))
  {
    int duration = server.arg(argName).toInt();
    if (duration > minDuration)
    {
      move(pin, duration);
      String message = "OK";
      message += duration;
      server.send(200, "text/plain", message);
    }
    else
    {
      server.send(400, "text/plain", String("duration (ms) must be at least ") + minDuration + String ("\n"));
    }
  }
  else
  {
    server.send(400, "text/plain", F("missing required argument ms (duration in ms)\n"));
  }  
}
void handleGetHeight(){
  String message = F("{\"height\":");
  message += getDeskPosition();
  message += F(",\"unit\"=\"mm\"}\n");
  server.send(200, "text/json", message);
}

void handleMoveUp() {
  handleMove(uppin);
}

void handleMoveDown() {
  handleMove(downpin);
}
 
// Define routing
void restServerRouting() {
    server.on("/", HTTP_GET, []() {
        server.send(200, F("text/json"),
            F("{\"operations\":{\"/up\":{\"arguments\":{\"ms\":\"duration in ms (int)\"}},\"/down\":{\"arguments\":{\"ms\":\"duration in ms (int)\"}}}}"));
    });
    server.on(F("/up"), HTTP_POST, handleMoveUp);
    server.on(F("/down"), HTTP_POST, handleMoveDown);
    server.on(F("/height"), HTTP_POST, handleMove2Height);
    server.on(F("/height"), HTTP_GET, handleGetHeight);
    
}
 
// Manage not found URL
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void ICACHE_RAM_ATTR onUpBtnPress()
{
  Serial.println("up button changed");
  Serial.println(digitalRead(upBtnPin));
  if(digitalRead(upBtnPin) == 0){
    //start moving
    digitalWrite(uppin, LOW);
  }
  else{
    //stop moving
    digitalWrite(uppin, HIGH);
  }
}

void ICACHE_RAM_ATTR onDownBtnPress()
{
  Serial.println("down button pressed");
  Serial.println(digitalRead(downBtnPin));
  if(digitalRead(downBtnPin) == 0){
    //start moving
   digitalWrite(downpin, LOW);
  }
  else{
    //stop moving
    digitalWrite(downpin, HIGH);
  }
  Serial.write(deskPositionSerial.read());
}
 
void setup(void) {
  pinMode(uppin, OUTPUT);
  digitalWrite(uppin, HIGH);
  
  pinMode(downpin, OUTPUT);
  digitalWrite(downpin, HIGH);

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
 
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
  // Activate mDNS this is used to be able to connect to the server
  // with local DNS hostmane esp8266.local
  if (MDNS.begin("libdesk")) {
    Serial.println("MDNS responder started");
  }
 
  // Set server routing
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  pinMode(upBtnPin, INPUT_PULLUP);
  pinMode(downBtnPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(upBtnPin), onUpBtnPress, CHANGE);
  attachInterrupt(digitalPinToInterrupt(downBtnPin), onDownBtnPress, CHANGE);

  const int deskPosSerialPin = 5;
  pinMode(deskPosSerialPin, INPUT);
  pinMode(14, OUTPUT);
  //5 / 4
  deskPositionSerial.begin(9600, SWSERIAL_8N1, deskPosSerialPin, 14);

 Serial.println(getDeskPosition());
 //move2height(800);
}

uint16_t getDeskPosition(){

   byte header_correct = 0;
   while(header_correct < 2)
   {
      if(deskPositionSerial.available() > 0)
        if(deskPositionSerial.read() == 1)
        {
          header_correct ++;
        }
        else{
          header_correct = 0;
        }
   }
   byte buf [2];
   while(deskPositionSerial.available()<2)
   {
    delay(10);
   }
   deskPositionSerial.readBytes(buf, 2);
   uint16_t pos = (buf[0] << 8) + buf[1];
   return pos;
}
  
void loop(void) {
  server.handleClient();
      //  Serial.println(getDeskPosition());

       // moveDown2(900);

    //     Serial.print(dataBuffer[0], DEC);
    //     Serial.print(" ");
    //     Serial.print(dataBuffer[1], DEC);
    //     Serial.print(" ");
    //     Serial.print(dataBuffer[2], DEC);
    //     Serial.print(" ");
    //     Serial.println(dataBuffer[3], DEC);
    //     Serial.println(pos);
    //     Serial.println("");
}
