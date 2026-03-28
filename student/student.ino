#include <WiFi.h>
#include <WebSocketsClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <ArduinoJson.h>

const int CLASSROOM_ID = 1;
const char* ssid = "kevin";
const char* password = "kh930914";
const char* ws_server = "192.168.100.142";
const int ws_port = 8080;

#define SS_PIN  5
#define RST_PIN 27 
#define LED_R 15
#define LED_G 2
#define LED_B 4

Adafruit_SSD1306 display(128, 64, &Wire, -1);
MFRC522 rfid(SS_PIN, RST_PIN);
WebSocketsClient webSocket;

String roomName = "Syncing...";
String lastStatus = "Unknown"; 
unsigned long lastScreenUpdate = 0;

const unsigned char PROGMEM icon_check[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x0F, 0xC0, 0x00, 0x00, 0x1F, 0xE0, 0x00, 0x00, 0x3F, 0xF0,
0x00, 0x00, 0x7F, 0xF8, 0x00, 0x00, 0xFF, 0xFC, 0x00, 0x01, 0xFF, 0xFE, 0x00, 0x03, 0xFF, 0xFF,
0x00, 0x07, 0xFF, 0xFF, 0x80, 0x0F, 0xFF, 0xFF, 0xC0, 0x1F, 0xFF, 0xFF, 0xE0, 0x3F, 0xFF, 0xFF,
0xF0, 0x7F, 0xFF, 0xFE, 0xF8, 0xFF, 0xFF, 0xFC, 0xFC, 0xFF, 0xFF, 0xF8, 0xFE, 0xFF, 0xFF, 0xF0,
0xFF, 0xFF, 0xFF, 0xE0, 0x7F, 0xFF, 0xFF, 0xC0, 0x3F, 0xFF, 0xFF, 0x80, 0x1F, 0xFF, 0xFF, 0x00,
0x0F, 0xFF, 0xFE, 0x00, 0x07, 0xFF, 0xFC, 0x00, 0x03, 0xFF, 0xF8, 0x00, 0x01, 0xFF, 0xF0, 0x00,
0x00, 0x7F, 0xE0, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x0F, 0x00, 0x00
};
const unsigned char PROGMEM icon_cross[] = {
0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0xC0, 0x07, 0x80, 0x01, 0xE0, 0x0F, 0xC0, 0x03, 0xF0,
0x1F, 0xE0, 0x07, 0xF8, 0x3F, 0xF0, 0x0F, 0xFC, 0x7F, 0xF8, 0x1F, 0xFE, 0xFF, 0xFC, 0x3F, 0xFF,
0xFF, 0xFE, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFE, 0x3F, 0xFF, 0xFF, 0xFC,
0x1F, 0xFF, 0xFF, 0xF8, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF8,
0x3F, 0xFF, 0xFF, 0xFC, 0x7F, 0xFF, 0xFF, 0xFE, 0xFF, 0xFE, 0x7F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF,
0x7F, 0xF8, 0x1F, 0xFE, 0x3F, 0xF0, 0x0F, 0xFC, 0x1F, 0xE0, 0x07, 0xF8, 0x0F, 0xC0, 0x03, 0xF0,
0x07, 0x80, 0x01, 0xE0, 0x03, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  delay(2000); 
  
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(20, 25);
  display.print("System Boot...");
  display.display();
  
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(500);
  
  configTime(28800, 0, "pool.ntp.org");
  
  webSocket.begin(ws_server, ws_port, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();
  
  if (millis() - lastScreenUpdate > 1000) {
    drawStandby();
    lastScreenUpdate = millis();
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  checkAccess(uid);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    if (doc.containsKey("type") && (doc["type"] == "dashboard_update" || doc["type"] == "dashboard_data")) {
      JsonObject data = doc["data"];
      if(doc["cid"] == CLASSROOM_ID || doc["type"] == "dashboard_data") {
        roomName = data["name"].as<String>();
        if(data["is_locked"] == 1) lastStatus = "LOCKED";
        else lastStatus = "OPEN";
      }
    }
    
    if (doc.containsKey("type") && doc["type"] == "access_result") {
      String status = doc["status"].as<String>();
      String msg = doc["message"].as<String>();
      roomName = doc["room_name"].as<String>();
      
      showResult(status == "GRANTED", msg);
    }
  } else if (type == WStype_CONNECTED) {
    String msg = "{\"action\":\"check_access\", \"uid\":\"STATUS_CHECK\", \"cid\":" + String(CLASSROOM_ID) + "}";
    webSocket.sendTXT(msg);
  }
}

void drawStandby() {
  struct tm timeinfo;
  bool timeSynced = getLocalTime(&timeinfo);

  display.clearDisplay();
  
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(16, 10);
  
  if(timeSynced) {
    display.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    display.setTextSize(1);
    display.setCursor(30, 15);
    display.print("Syncing...");
  }
  
  display.drawLine(0, 30, 128, 30, WHITE);
  
  display.setTextSize(1);
  int nameX = (128 - roomName.length()*6)/2;
  if(nameX < 0) nameX = 0;
  display.setCursor(nameX, 36);
  display.print(roomName);

  if(lastStatus == "LOCKED") {
     display.setCursor(40, 52); display.print("[LOCKED]");
  } else {
     if ((millis() / 1000) % 2 == 0) { 
       display.setCursor(30, 52); display.print("Please Scan");
     } else {
       display.setCursor(50, 52); display.print("Card");
     }
  }
  
  display.display();
}

void checkAccess(String uid) {
  display.clearDisplay();
  display.setCursor(30, 25); display.print("Checking...");
  display.display();

  String msg = "{\"action\":\"check_access\", \"uid\":\"" + uid + "\", \"cid\":" + String(CLASSROOM_ID) + "}";
  webSocket.sendTXT(msg);
}

void showResult(bool success, String msg) {
  display.clearDisplay();
  if(success) {
    digitalWrite(LED_G, HIGH); 
    display.drawBitmap(48, 10, icon_check, 32, 32, 1);
  } else {
    digitalWrite(LED_R, HIGH); 
    display.drawBitmap(48, 10, icon_cross, 32, 32, 1);
  }
  
  display.setTextSize(1);
  int x = (128 - msg.length() * 6) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, 50); 
  display.print(msg);
  display.display();
  
  delay(2000);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_R, LOW);
}