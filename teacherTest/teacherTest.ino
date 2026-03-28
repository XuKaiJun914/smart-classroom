/*
 * 教室物聯網 - 老師端硬體測試程式
 * 功能: 測試 OLED, DHT11, WS2812, 按鈕 是否接線正確
 * * 接線複習:
 * OLED: SDA -> 21, SCL -> 22
 * DHT11: Data -> 4
 * WS2812: Data -> 13
 * 按鈕: 一端接 15, 另一端接 GND
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

// --- 腳位設定 (與之前相同) ---
#define DHTPIN 4
#define DHTTYPE DHT11
#define BUTTON_PIN 15
#define LED_PIN 13
#define NUMPIXELS 8
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- 物件初始化 ---
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Hardware Test Start ---");

  // 1. 初始化 OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Error: SSD1306 OLED not found!"));
    // 如果 OLED 失敗，會卡在這裡，請檢查 VCC/GND/SDA/SCL
    for(;;); 
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("OLED: OK");
  display.display();
  Serial.println("OLED initialized");
  delay(500);

  // 2. 初始化 DHT11
  dht.begin();
  Serial.println("DHT11 initialized");

  // 3. 初始化 WS2812
  pixels.begin();
  pixels.setBrightness(50); // 亮度設低一點避免電流過大
  pixels.show(); // 初始化全暗
  Serial.println("WS2812 initialized");

  // 4. 初始化按鈕
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Button initialized");
}

void loop() {
  // --- 測試 1: 讀取按鈕 ---
  int btnState = digitalRead(BUTTON_PIN);
  String btnText = (btnState == LOW) ? "PRESSED" : "RELEASED";
  
  // 按下按鈕時，Serial 印出訊息
  if (btnState == LOW) {
    Serial.println("Button is PRESSED!");
  }

  // --- 測試 2: 讀取溫濕度 ---
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  // 檢查讀取是否失敗
  String tempText = "---";
  String humText = "---";
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    tempText = String(t, 1);
    humText = String(h, 0);
    // Serial.printf("Temp: %.1f C, Hum: %.1f %%\n", t, h);
  }

  // --- 測試 3: WS2812 跑馬燈效果 ---
  // 根據時間產生顏色流動
  static uint16_t j = 0;
  for(int i=0; i<NUMPIXELS; i++) {
    // 簡單的彩虹邏輯
    pixels.setPixelColor(i, Wheel(((i * 256 / NUMPIXELS) + j) & 255));
  }
  pixels.show();
  j+=5; // 增加顏色變換速度
  if (j >= 256 * 5) j = 0;

  // --- 測試 4: 更新 OLED 顯示 ---
  display.clearDisplay();
  
  // 標題
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("== HARDWARE TEST ==");
  
  // 溫濕度
  display.setCursor(0, 15);
  display.print("Temp: "); display.print(tempText); display.println(" C");
  display.print("Humi: "); display.print(humText); display.println(" %");

  // 按鈕狀態
  display.setCursor(0, 40);
  display.print("Button: "); 
  if (btnState == LOW) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // 反白顯示
    display.println(btnText);
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.println(btnText);
  }

  // 燈號狀態提示
  display.setCursor(0, 55);
  display.print("LEDs: Running...");

  display.display();
  
  // 稍微延遲避免刷新太快看不清楚 Serial
  delay(50);
}

// WS2812 彩虹顏色產生函式
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}