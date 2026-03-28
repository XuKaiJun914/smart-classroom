#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h> 

const int CLASSROOM_ID = 1;
const char* ssid = "kevin";
const char* password = "kh930914";
const char* ws_server = "192.168.100.142";
const int ws_port = 8080;

#define DHTPIN 4
#define DHTTYPE DHT11
#define LED_PIN 13
#define NUMPIXELS 8
#define BUTTON_PIN 15

Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebSocketsClient webSocket;

int currentCount = 0;
int capacity = 40;
bool isLocked = true;
String roomName = "Room";
float temp = 0;
float hum = 0;
unsigned long lastTime = 0;
unsigned long timerDelay = 3000; 

int brightness = 50;
int fadeAmount = 5;

const unsigned char PROGMEM lock_icon [] = {
0x00, 0x00, 0x0F, 0xF0, 0x18, 0x18, 0x30, 0x0C, 0x30, 0x0C, 0x30, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC,
0x3F, 0xFC, 0x33, 0xCC, 0x33, 0xCC, 0x33, 0xCC, 0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x00, 0x00, 0x00
};
const unsigned char PROGMEM unlock_icon [] = {
0x00, 0x00, 0x0F, 0xF0, 0x18, 0x18, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x3F, 0xFC, 0x3F, 0xFC,
0x3F, 0xFC, 0x33, 0xCC, 0x33, 0xCC, 0x33, 0xCC, 0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  dht.begin();
  pixels.begin();
  pixels.setBrightness(100); 

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(20,25);
  display.println("Connecting...");
  display.display();

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(500);

  webSocket.begin(ws_server, ws_port, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();

  if (digitalRead(BUTTON_PIN) == LOW) {
    String msg = "{\"action\":\"toggle_lock\", \"cid\":" + String(CLASSROOM_ID) + "}";
    webSocket.sendTXT(msg);
    delay(500); 
  }

  if ((millis() - lastTime) > timerDelay) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if(!isnan(t)) temp = t;
    if(!isnan(h)) hum = h;
    
    String msg = "{\"action\":\"env_update\", \"cid\":" + String(CLASSROOM_ID) + ", \"temp\":" + String(temp) + ", \"hum\":" + String(hum) + "}";
    webSocket.sendTXT(msg);
    lastTime = millis();
  }

  updateLEDs();
  drawUI();
  delay(30); 
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    if (doc.containsKey("type") && (doc["type"] == "dashboard_update" || doc["type"] == "dashboard_data")) {
      JsonObject data = doc["data"];
      if(doc["cid"] == CLASSROOM_ID || doc["type"] == "dashboard_data") {
        currentCount = data["current_count"];
        capacity = data["capacity"];
        isLocked = (data["is_locked"] == 1);
        roomName = data["name"].as<String>();
      }
    }
  } else if (type == WStype_CONNECTED) {
    String msg = "{\"action\":\"get_dashboard\", \"cid\":" + String(CLASSROOM_ID) + "}";
    webSocket.sendTXT(msg);
  }
}

void drawUI() {
  display.clearDisplay();
  
  if(isLocked) {
    display.drawBitmap(0, 0, lock_icon, 16, 16, 1);
    display.setCursor(20, 4); display.print("LOCKED");
  } else {
    display.drawBitmap(0, 0, unlock_icon, 16, 16, 1);
    display.setCursor(20, 4); display.print("OPEN");
  }

  display.setCursor(80, 4); display.print(roomName.substring(0, 6));
  display.drawLine(0, 18, 128, 18, WHITE);

  display.setTextSize(1);
  display.setCursor(0, 25); display.print("Students:");
  display.setTextSize(2);
  display.setCursor(60, 25); display.print(currentCount);
  
  display.setTextSize(1);
  display.setCursor(100, 32); display.print("/"); display.print(capacity);

  display.setTextSize(1);
  display.setCursor(0, 50); 
  display.print("T:"); display.print((int)temp); display.print("C");
  display.setCursor(64, 50); 
  display.print("H:"); display.print((int)hum); display.print("%");
  
  display.drawRect(0, 60, 128, 4, WHITE);
  int fill = map((int)temp, 20, 40, 0, 128); 
  display.fillRect(0, 60, fill, 4, WHITE);

  display.display();
}

void updateLEDs() {
  brightness = brightness + fadeAmount;
  if (brightness <= 20 || brightness >= 150) {
    fadeAmount = -fadeAmount;
  }
  
  uint32_t color;
  float usage = (float)currentCount / (float)capacity;
  
  if (usage < 0.5) {
     color = pixels.Color(0, 255, 0); 
  } else if (usage < 0.8) {
     color = pixels.Color(255, 200, 0); 
  } else {
     color = pixels.Color(255, 0, 0); 
  }

  pixels.setBrightness(brightness);
  
  int litPixels = map(currentCount, 0, capacity, 0, NUMPIXELS);
  if(litPixels > NUMPIXELS) litPixels = NUMPIXELS;
  
  for(int i=0; i<NUMPIXELS; i++) {
    if(i < litPixels) pixels.setPixelColor(i, color);
    else pixels.setPixelColor(i, 0);
  }
  pixels.show();
}