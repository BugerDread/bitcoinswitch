#include <FS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ESP8266WiFi.h>
#include "qrcode.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#define PARAM_FILE "/elements.json"

/////////////////////////////////
///////////CHANGE////////////////
/////////////////////////////////

bool usingLCD = true; // false if not using LCD
// int portalPin = 4;

const int QR_X_OFFSET = 20;
const int QR_Y_OFFSET = 3;
const int QR_DOT_SIZE = 2;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

// Access point variables
String payloadStr;
String password;
String serverFull;
String lnbitsServer;
String ssid;
String wifiPassword;
String deviceId;
String highPin;
String timePin;
String pinFlip = "true";
String lnurl;
String dataId;
String payReq;

int balance;
int oldBalance;

static const uint16_t WS_RECONNECT_INTERVAL = 5000;  // websocket reconnec interval

bool paid;
bool triggerUSB = false; 
bool showqr = false; 

TFT_eSPI tft = TFT_eSPI();
WebSocketsClient webSocket;

struct KeyValue {
  String key;
  String value;
};

void logoScreen()
{ 
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_PURPLE);
  tft.println("bitcoinSwitch");
}

void setup()
{
  Serial.begin(115200);
  if(usingLCD == true){
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(false);
    logoScreen();
  }

  pinMode (2, OUTPUT);
  digitalWrite(2, HIGH);

  SPIFFSConfig cfg;
  cfg.setAutoFormat(true);
  SPIFFS.setConfig(cfg);
  SPIFFS.begin();

  // get the saved details and store in global variables
  readFiles();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin((char*)ssid.c_str(), (char*)wifiPassword.c_str());

  int timer = 0;  
  while (WiFi.status() != WL_CONNECTED && timer < 8000) {
    delay(1000);
    Serial.print(".");
    timer = timer + 1000;
    if(timer > 7000){
      triggerUSB = true;
    }
  }
  
  if (triggerUSB == true)
  {
    Serial.println("USB triggered");
    configOverSerialPort();
  }

  lnbitsScreen();
  delay(1000);

  paid = false;

  Serial.println(lnbitsServer + "/lnurldevice/ws/" + deviceId);
  webSocket.beginSSL(lnbitsServer.c_str(), 443, ("/lnurldevice/ws/" + deviceId).c_str());
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(WS_RECONNECT_INTERVAL);
}

void loop() {
  while(WiFi.status() != WL_CONNECTED){
    if(usingLCD == true){
      connectionError();
    }
    Serial.println("Failed to connect");
    delay(500);
  }

  if((lnurl != "true") and (usingLCD == true) and (showqr == true)){
      qrdisplayScreen();
      showqr = false;
  }
  
    webSocket.loop();
    
    if(paid){
      paid = false;
      Serial.println("Paid");
      Serial.println(payloadStr);
      highPin = getValue(payloadStr, '-', 0);
      Serial.println(highPin);
      timePin = getValue(payloadStr, '-', 1);
      Serial.println(timePin);
      if(usingLCD == true){
        paidScreen();
      }
      onOff();
      if(usingLCD == true){
        completeScreen();
        delay(2000);
        showqr = true;
      }
    }
    
}

//////////////////HELPERS///////////////////

void onOff()
{ 
  pinMode (highPin.toInt(), OUTPUT);
  if(pinFlip == "true"){
    digitalWrite(highPin.toInt(), LOW);
    delay(timePin.toInt());
    digitalWrite(highPin.toInt(), HIGH); 
    delay(2000);
  }
  else{
    digitalWrite(highPin.toInt(), HIGH);
    delay(timePin.toInt());
    digitalWrite(highPin.toInt(), LOW); 
    delay(2000);
  }
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void readFiles()
{
  File paramFile = SPIFFS.open(PARAM_FILE, "r");
  if (paramFile)
  {
    StaticJsonDocument<1500> doc;
    DeserializationError error = deserializeJson(doc, paramFile.readString());

    const JsonObject maRoot0 = doc[0];
    const char *maRoot0Char = maRoot0["value"];
    password = maRoot0Char;
    Serial.println(password);

    const JsonObject maRoot1 = doc[1];
    const char *maRoot1Char = maRoot1["value"];
    ssid = maRoot1Char;
    Serial.println(ssid);

    const JsonObject maRoot2 = doc[2];
    const char *maRoot2Char = maRoot2["value"];
    wifiPassword = maRoot2Char;
    Serial.println(wifiPassword);

    const JsonObject maRoot3 = doc[3];
    const char *maRoot3Char = maRoot3["value"];
    serverFull = maRoot3Char;
    lnbitsServer = serverFull.substring(5, serverFull.length() - 38);
    deviceId = serverFull.substring(serverFull.length() - 22);

    const JsonObject maRoot4 = doc[4];
    const char *maRoot4Char = maRoot4["value"];
    lnurl = maRoot4Char;
    Serial.println(lnurl);
  }
  paramFile.close();
}

//////////////////DISPLAY///////////////////

void serverError()
{
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED);
  tft.println("Server connect fail");
}

void connectionError()
{
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED);
  tft.println("Wifi connect fail");
}

void connection()
{
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED);
  tft.println("Wifi connected");
}



void portalLaunched()
{ 
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_PURPLE);
  tft.println("PORTAL LAUNCH");
}

void processingScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(40, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.println("PROCESSING");
}

void lnbitsScreen()
{ 
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(10, 90);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  tft.println("POWERED BY LNBITS");
}

void portalScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(30, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.println("PORTAL LAUNCHED");
}

void paidScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(110, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.println("PAID");
}

void completeScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(60, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.println("COMPLETE");
}

void errorScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(70, 80);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.println("ERROR");
}

void qrdisplayScreen()
{
  String qrCodeData;
  if(lnurl != "true"){
    qrCodeData = lnurl;
  }
  else{
    qrCodeData = payReq;
  }
  tft.fillScreen(TFT_WHITE);
  qrCodeData.toUpperCase();
  const char *qrDataChar = qrCodeData.c_str();
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(10)];
  qrcode_initText(&qrcoded, qrcodeData, 10, 0, qrDataChar);
  for (uint8_t y = 0; y < qrcoded.size; y++)
  {
    // Each horizontal module
    for (uint8_t x = 0; x < qrcoded.size; x++)
    {
      if (qrcode_getModule(&qrcoded, x, y))
      {
        tft.fillRect(QR_X_OFFSET + QR_DOT_SIZE * x, QR_Y_OFFSET + QR_DOT_SIZE * y, QR_DOT_SIZE, QR_DOT_SIZE, ST7735_BLACK);
      }
      else
      {
        tft.fillRect(QR_X_OFFSET + QR_DOT_SIZE * x, QR_Y_OFFSET + QR_DOT_SIZE * y, QR_DOT_SIZE, QR_DOT_SIZE, ST7735_WHITE);
      }
    }
  }
}

//////////////////WEBSOCKET///////////////////
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WSc] Disconnected!\n");
            break;
        case WStype_CONNECTED:
            {
            Serial.printf("[WSc] Connected to url: %s\n",  payload);
			      webSocket.sendTXT("Connected");
            showqr = true;
            }
            break;
        case WStype_TEXT:
            payloadStr = (char*)payload;
            paid = true;
    		case WStype_ERROR:			
    		case WStype_FRAGMENT_TEXT_START:
    		case WStype_FRAGMENT_BIN_START:
    		case WStype_FRAGMENT:
    		case WStype_FRAGMENT_FIN:
    			break;
    }
}
