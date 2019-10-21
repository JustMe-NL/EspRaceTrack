#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <EEPROM.h>

// char[1] = '/0'; null termination

ESP8266WiFiMulti wifiMulti;                // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

ESP8266WebServer server(80);               // create a web server on port 80
WebSocketsServer webSocket(81);            // create a websocket server on port 81

File fsUploadFile;                         // a File variable to temporarily store the received file

const char *ssid = "MyOwnSSID"; // The name of the Wi-Fi network that will be created
const char *password = "SuperSecretPassword";   // The password required to connect to it, leave blank for an open network

const char *OTAName = "OTAName";           // A name and a password for the OTA service
const char *OTAPassword = "OTASSPassword";

#define E2END     255

#define TRIGGER1  12
#define TRIGGER2  13
#define TRIGGER3  14
#define TRIGGER4  04

#define S_IDLE        0
#define S_STOPRCVD    1
#define S_STARTRCVD   2
#define S_START       3
#define S_RUNNING     4

const char* mdnsName = "CarreraSpeedway"; // Domain name for the mDNS responder

// Prototypes
void startWiFi();
void startOTA();
void startSPIFFS();
void startWebSocket();
void startMDNS();
void startServer();
String formatBytes(size_t bytes);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
void handleFileUpload();
void handleNotFound();
bool handleFileRead(String path);
String getContentType(String filename);
char *ultostr(unsigned long value, char *ptr, int base);
void readEE();
void writeEE();
void printASCII(char * buffer);
void e2reader();

// Global vars
unsigned long prevMillis = millis();
boolean dotrig1;
boolean dotrig2;
boolean dotrig3;
boolean dotrig4;
uint8_t state;
boolean ping;
uint8_t sendstatus;

long unsigned int track1;
long unsigned int track2;
long unsigned int track3;
long unsigned int track4;
long unsigned int timeout;
long unsigned int starttime;

struct EEdataset {
  char name1[32];   // 32
  char name2[32];   // 64
  char name3[32];   // 96
  char name4[32];   // 128
  char ssid[32];    // 160
  char pwd[32];     // 192
};

EEdataset eeData;

// Interrupt routines
void triggerTrack1() {
  track1 = millis();
  dotrig1 = true;
}

void triggerTrack2() {
  track2 = millis();
  dotrig2 = true;
}

void triggerTrack3() {
  track3 = millis();
  dotrig3 = true;
}

void triggerTrack4() {
  track4 = millis();
  dotrig4 = true;
}
/*__________________________________________________________SETUP__________________________________________________________*/

void setup() {
  EEPROM.begin(256);

  pinMode(TRIGGER1, INPUT);
  pinMode(TRIGGER2, INPUT);
  pinMode(TRIGGER3, INPUT);
  pinMode(TRIGGER4, INPUT);

  dotrig1 = false;
  dotrig2 = false;
  dotrig3 = false;
  dotrig4 = false;
  state = 0;
  ping = false;
  sendstatus = 0;

  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  readEE();                    // Read data in EEPROM to set WiFi

  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startOTA();                  // Start the OTA service

  startSPIFFS();               // Start the SPIFFS and list all contents

  startWebSocket();            // Start a WebSocket server

  startMDNS();                 // Start the mDNS responder

  startServer();               // Start a HTTP server with a file read handler and an upload handler

  attachInterrupt(digitalPinToInterrupt(TRIGGER1), triggerTrack1, RISING);
  attachInterrupt(digitalPinToInterrupt(TRIGGER2), triggerTrack2, RISING);
  attachInterrupt(digitalPinToInterrupt(TRIGGER3), triggerTrack3, RISING);
  attachInterrupt(digitalPinToInterrupt(TRIGGER4), triggerTrack4, RISING);
}

/*__________________________________________________________LOOP__________________________________________________________*/

void loop() {
char message[36];
uint8_t meslen;

  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events

  if (dotrig1) {
    sprintf(message,"1:%lu", track1);
    webSocket.broadcastTXT(message);
    dotrig1 = false;
  }
  if (dotrig2) {
    sprintf(message,"2:%lu", track2);
    webSocket.broadcastTXT(message);
    dotrig2 = false;
  }
  if (dotrig3) {
    sprintf(message,"3:%lu", track3);
    webSocket.broadcastTXT(message);
    dotrig3 = false;
  }
  if (dotrig4) {
    sprintf(message,"4:%lu", track4);
    webSocket.broadcastTXT(message);
    dotrig4 = false;
  }
  if (state == S_STARTRCVD) {
    starttime = millis();
    timeout = random(500, 3000);
    state++;
  }
  if (state == S_START) {
    unsigned long now = millis();         // now: timestamp
    unsigned long elapsed = now - starttime;  // elapsed: duration
    if (elapsed >= timeout) {
      sprintf(message,"S:%lu", millis());
      webSocket.broadcastTXT(message);
      state++;
    }
  }
  if (ping) {
    sprintf(message,"P");
    webSocket.broadcastTXT(message);
    ping = false;
  }
  if (sendstatus > 0) {
    if (sendstatus < 7) {
      message[0] = 'R';
      message[1] = sendstatus + 0x30;
      message[2] = ':';
      for (uint8_t i = 1; i < 32; i++) {
        if (sendstatus == 1) { message[i + 2] = eeData.name1[i]; }
        if (sendstatus == 2) { message[i + 2] = eeData.name2[i]; }
        if (sendstatus == 3) { message[i + 2] = eeData.name3[i]; }
        if (sendstatus == 4) { message[i + 2] = eeData.name4[i]; }
        if (sendstatus == 5) { message[i + 2] = eeData.ssid[i]; }
        if (sendstatus == 6) { message[i + 2] = eeData.pwd[i]; }
      }
      if (sendstatus == 1) { meslen = eeData.name1[0]; }
      if (sendstatus == 2) { meslen = eeData.name2[0]; }
      if (sendstatus == 3) { meslen = eeData.name3[0]; }
      if (sendstatus == 4) { meslen = eeData.name4[0]; }
      if (sendstatus == 5) { meslen = eeData.ssid[0]; }
      if (sendstatus == 6) { meslen = eeData.pwd[0]; }
      meslen = meslen + 3;
      webSocket.broadcastTXT(message, meslen);
    }
    if (sendstatus == 7) {
      if (wifiMulti.run() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        sprintf(message,"X:Connected - %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      } else {
        sprintf(message,"X:NOT connected");
      }
      webSocket.broadcastTXT(message);
    }
    if (sendstatus == 8) {
      readEE();
    }
    sendstatus = 0;
  }
}

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi() { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
char ssidwifi[32], pwdwifi[32];

  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  if ((eeData.ssid[0] > 0) && (eeData.ssid[0] < 31)) {
    for (uint8_t i = 0; i < eeData.ssid[0]; i++) {
      ssidwifi[i] = eeData.ssid[i + 1];
    };
    ssidwifi[eeData.ssid[0]] = NULL;
    if ((eeData.pwd[0] > 0) && (eeData.pwd[0] < 31)) {
      for (uint8_t i = 0; i < eeData.pwd[0]; i++) {
        pwdwifi[i] = eeData.pwd[i + 1];
      };
      pwdwifi[eeData.pwd[0]] = NULL;
      Serial.print("Trying to connect to ");
      Serial.println(ssidwifi);
      wifiMulti.addAP(ssidwifi, pwdwifi);
    }
  }

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED && WiFi.softAPgetStationNum() < 1) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  if(WiFi.softAPgetStationNum() == 0) {      // If the ESP is connected to an AP
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());             // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  } else {                                   // If a station is connected to the ESP SoftAP
    Serial.print("Station connected to ESP8266 AP");
  }
  Serial.println("\r\n");
}

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdnsName);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  }, handleFileUpload);                       // go to 'handleFileUpload'

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
                                              // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound(){ // if the requested file or page doesn't exist, return a 404 not found error
  if(!handleFileRead(server.uri())){          // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  String path;
  if(upload.status == UPLOAD_FILE_START){
    path = upload.filename;
    if(!path.startsWith("/")) path = "/"+path;
    if(!path.endsWith(".gz")) {                          // The file server always prefers a compressed version of a file
      String pathWithGz = path+".gz";                    // So if an uploaded file is not compressed, the existing compressed
      if(SPIFFS.exists(pathWithGz))                      // version of that file must be deleted (if it exists)
         SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
uint8_t i, vallen;

  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);                // Turn rainbow off when a new connection is established
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] get Text: %s\n", num, payload);
      if (payload[0] == 'S') {            // start
        state = S_STARTRCVD;
      }
      if (payload[0] =='s') {
        state = S_STOPRCVD;
      }
      if (payload[0] == 'p') {
        ping = true;
      }
      if (payload[0] == 'R') {

      }
      if (payload[0] == 'W') {
        // 0123 start@3, end @33
        // Wx:text
        if ((payload[1] - 0x30) < 7) {
          vallen = length - 3;
          if (vallen > 30) { vallen = 30; }
          Serial.print("start copy recv data: ");
          Serial.println(vallen);
          for (i = 1; i < vallen + 1; i++) {
              if (payload[1] == '1') { eeData.name1[i] = payload[i + 2]; }
              if (payload[1] == '2') { eeData.name2[i] = payload[i + 2]; }
              if (payload[1] == '3') { eeData.name3[i] = payload[i + 2]; }
              if (payload[1] == '4') { eeData.name4[i] = payload[i + 2]; }
              if (payload[1] == '5') { eeData.ssid[i] = payload[i + 2]; }
              if (payload[1] == '6') { eeData.pwd[i] = payload[i + 2]; }
          } // for
          if (payload[1] == '1') { eeData.name1[0] = vallen; }
          if (payload[1] == '2') { eeData.name2[0] = vallen; }
          if (payload[1] == '3') { eeData.name3[0] = vallen; }
          if (payload[1] == '4') { eeData.name4[0] = vallen; }
          if (payload[1] == '5') { eeData.ssid[0] = vallen; }
          if (payload[1] == '6') { eeData.pwd[0] = vallen; }
        } // if
        if (payload[1] == '8') {
          writeEE();
          Serial.println("Data written to EEPROM");
        }
      }
      if (payload[0] == 'R') {
        if (length > 1) {
          sendstatus = payload[1] - 0x30;
        }
      }
      break;
    default:
      break;
  }
}

void readEE() { // read data in EEPROM to eeData
int eeAddress = 0;
  EEPROM.get(eeAddress, eeData);
  //e2reader();
}

void writeEE() { // Write data from eeData to EEPROM
int eeAddress = 0;
  EEPROM.put(eeAddress, eeData);
  EEPROM.commit();
  //e2reader();
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void printASCII(char * buffer){
  for(int i = 0; i < 16; i++){
    if(i == 8)
      Serial.print(" ");

    if(buffer[i] > 31 and buffer[i] < 127){
      Serial.print(buffer[i]);
    }else{
      Serial.print(".");
    }
  }
}

void e2reader(){
  char buffer[16];
  char valuePrint[4];
  byte value;
  unsigned int address;
  uint8_t trailingSpace = 2;

  Serial.print("Dumping "); Serial.print(E2END + 1);
  Serial.println(" bytes from EEPROM.");
  Serial.print("baseAddr ");
  for(int x = 0; x < 2; x++){
    Serial.print(" ");
    for(int y = 0; y < 25; y++)
      Serial.print("=");
  }

  // E2END is a macro defined as the last EEPROM address
  // (1023 for ATMEGA328P)
  for(address = 0; address <= E2END; address++){
    // read a byte from the current address of the EEPROM
    value = EEPROM.read(address);

    // add space between two sets of 8 bytes
    if(address % 8 == 0)
      Serial.print("  ");

    // newline and address for every 16 bytes
    if(address % 16 == 0){
      //print the buffer
      if(address > 0 && address % 16 == 0)
        printASCII(buffer);

      sprintf(buffer, "\n 0x%05X: ", address);
      Serial.print(buffer);

      //clear the buffer for the next data block
      memset (buffer, 32, 16);
    }

    // save the value in temporary storage
    buffer[address%16] = value;

    // print the formatted value
    sprintf(valuePrint, " %02X", value);
    Serial.print(valuePrint);
  }

  if(address % 16 > 0){
    if(address % 16 < 9)
      trailingSpace += 2;

    trailingSpace += (16 - address % 16) * 3;
  }

  for(int i = trailingSpace; i > 0; i--)
    Serial.print(" ");

  //last line of data and a new line
  printASCII(buffer);
  Serial.println();
}
