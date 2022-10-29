#define USELCD            //comment this line out to disable LCD
#define PARAM_FILE "/elements.json"

#include <FS.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h> // https://github.com/Links2004/arduinoWebSockets
#include <ArduinoJson.h>  // https://arduinojson.org/?utm_source=meta&utm_medium=library.properties

#ifdef USELCD
  #include <SPI.h>
  #include <TFT_eSPI.h>     // https://github.com/Bodmer/TFT_eSPI
  #include "qrcode.h"       // https://github.com/ricmoo/qrcode/
#endif

/////////////////////////////////
///////////CHANGE////////////////
/////////////////////////////////

#ifdef USELCD
const uint32_t QR_X_OFFSET = 20;
const uint32_t QR_Y_OFFSET = 3;
const uint32_t QR_DOT_SIZE = 2;
const uint32_t QR_VERSION = 11;
const uint32_t QR_ECC = ECC_LOW;
#endif

const uint32_t WS_RECONNECT_INTERVAL = 5000;  // websocket reconnect interval (ms)
const uint32_t WS_HB_PING_TIME = 20000;          // ping server every WS_HB_PING_TIME ms (set to 0 to disable heartbeat)
const uint32_t WS_HB_PONG_WITHIN = 10000;        // expect pong from server within WS_HB_PONG_WITHIN ms
const uint32_t WS_HB_PONGS_MISSED = 2;             // consider connection disconnected if pong is not received WS_HB_PONGS_MISSED times

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

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
String payReq;

bool paid = false;
bool triggerUSB = false;

#ifdef USELCD 
  bool showqr = false;
  TFT_eSPI tft = TFT_eSPI();
#endif

WebSocketsClient webSocket;

struct KeyValue {
  String key;
  String value;
};

void setup()
{
  Serial.begin(115200);
  
  #ifdef USELCD
    tft.init();
    tft.setRotation(3);
    tft.invertDisplay(false);
    logoScreen();
  #endif

  //turn off onboard led (gpio 2)
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

  Serial.println(lnbitsServer + "/lnurldevice/ws/" + deviceId);
  webSocket.beginSSL(lnbitsServer.c_str(), 443, ("/lnurldevice/ws/" + deviceId).c_str());
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(WS_RECONNECT_INTERVAL);
  if (WS_HB_PING_TIME != 0) {
    Serial.print(F("Enabling WS heartbeat with ping time "));
    Serial.print(WS_HB_PING_TIME);
    Serial.print(F("ms, pong time "));
    Serial.print(WS_HB_PONG_WITHIN);
    Serial.print(F("ms, "));
    Serial.print(WS_HB_PONGS_MISSED);
    Serial.print(F(" missed pongs to reconnect."));
    webSocket.enableHeartbeat(WS_HB_PING_TIME, WS_HB_PONG_WITHIN, WS_HB_PONGS_MISSED);
  }
}

void loop() {
  while(WiFi.status() != WL_CONNECTED){
    #ifdef USELCD
      connectionError();
    #endif
    Serial.println("Failed to connect");
    delay(500);
  }

  #ifdef USELCD
    if(showqr == true){
      qrdisplayScreen();
      showqr = false;
    }
  #endif
  
  webSocket.loop();
    
  if(paid){
    paid = false;
    Serial.println("Paid");
    Serial.println(payloadStr);
    highPin = getValue(payloadStr, '-', 0);
    Serial.println(highPin);
    timePin = getValue(payloadStr, '-', 1);
    Serial.println(timePin);
    #ifdef USELCD
      paidScreen();
    #endif
    onOff();
    #ifdef USELCD
      completeScreen();
      delay(2000);
      showqr = true;
    #endif
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
    //delay(2000);
  }
  else{
    digitalWrite(highPin.toInt(), HIGH);
    delay(timePin.toInt());
    digitalWrite(highPin.toInt(), LOW); 
    //delay(2000);
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
#ifdef USELCD
  void logoScreen()
  { 
    tft.fillScreen(TFT_WHITE);
    tft.setCursor(3, 40);
    tft.setTextSize(2);
    tft.setTextColor(TFT_PURPLE);
    tft.println("bitcoinSwitch");
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(36, 64);
    tft.println("ESP8266 version");
    delay(1000);
    tft.setCursor(30, 80);
    tft.println("POWERED BY LNBITS");
  }
  
//  void serverError()
//  {
//    tft.fillScreen(TFT_WHITE);
//    tft.setCursor(0, 80);
//    tft.setTextSize(1);
//    tft.setTextColor(TFT_RED);
//    tft.println("Server connect fail");
//  }
//  
  void connectionError()
  {
    tft.fillScreen(TFT_WHITE);
    tft.setCursor(0, 80);
    tft.setTextSize(1);
    tft.setTextColor(TFT_RED);
    tft.println("Wifi connect fail");
  }
//  
//  void connection()
//  {
//    tft.fillScreen(TFT_WHITE);
//    tft.setCursor(0, 80);
//    tft.setTextSize(1);
//    tft.setTextColor(TFT_RED);
//    tft.println("Wifi connected");
//  }
  
  void paidScreen()
  { 
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(60, 60);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.println("PAID");
  }
  
  void completeScreen()
  { 
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(30, 60);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.println("COMPLETE");
  }
  
  void qrdisplayScreen() { 
    if(lnurl == "true") {
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(30, 60);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED);
      tft.println("NO LNURL");
      return;
    }

    lnurl.toUpperCase();
    tft.fillScreen(TFT_WHITE);
    QRCode qrcoded;
    uint8_t qrcodeData[qrcode_getBufferSize(QR_VERSION)];
    qrcode_initText(&qrcoded, qrcodeData, QR_VERSION, QR_ECC, lnurl.c_str());
    uint16_t module_color;
    for (uint8_t y = 0; y < qrcoded.size; y++) {
      // Each horizontal module
      for (uint8_t x = 0; x < qrcoded.size; x++) {
        if (qrcode_getModule(&qrcoded, x, y)) {
          module_color = TFT_BLACK;
        } else {
          module_color = TFT_WHITE;
        }
        tft.fillRect(QR_X_OFFSET + QR_DOT_SIZE * x, QR_Y_OFFSET + QR_DOT_SIZE * y, QR_DOT_SIZE, QR_DOT_SIZE, module_color);
      }
    }
  }
#endif  
//////////////////WEBSOCKET///////////////////
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println(F("[WSc] Disconnected!"));
            break;
        case WStype_CONNECTED:
            {
            Serial.printf("[WSc] Connected to url: %s\n",  payload);
			      webSocket.sendTXT("Connected");
            #ifdef USELCD
              showqr = true;
            #endif  
            }
            break;
        case WStype_TEXT:
            payloadStr = (char*)payload;
            paid = true;
            break;
        case WStype_PING:
            // pong will be send automatically
            Serial.println("[WSc] ping received");
            break;
        case WStype_PONG:
            // answer to a ping we send
            Serial.println("[WSc] pong received");
            break;    
    		case WStype_ERROR:			
    		case WStype_FRAGMENT_TEXT_START:
    		case WStype_FRAGMENT_BIN_START:
    		case WStype_FRAGMENT:
    		case WStype_FRAGMENT_FIN:
    			break;
    }
}
