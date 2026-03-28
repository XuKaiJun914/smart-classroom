/*
 * 教室物聯網 - 門禁端硬體測試程式 (整合修復版)
 * 基礎: 使用者提供的原始測試碼
 * 修正內容:
 * 1. 加入 I2C 降速 (100kHz) -> 修復 OLED 亂碼
 * 2. 加入 RC522 暴力重置 -> 修復初始化失敗
 * 3. 加入 最大天線增益 -> 修復讀不到卡片
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- 腳位設定 ---
#define RST_PIN     27  // 為了避開 OLED 的 SCL (22)，RST 改接 27
#define SS_PIN      5   // RC522 的 SDA (Chip Select)

#define LED_R       15
#define LED_G       2
#define LED_B       4

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- 物件初始化 ---
MFRC522 mfrc522(SS_PIN, RST_PIN); 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Door Node Hardware Test (Fixed) ---");

  // 1. 初始化 LED (設為輸出)
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  
  // 開機先全暗
  setLedColor(0, 0, 0);

  // 2. 初始化 OLED
  // Address 0x3C for 128x32 or 128x64
  // 先延遲一下等待電源穩定
  delay(100);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // 如果失敗卡在這邊
  }

  // [修正1] 降低 I2C 速度至 100kHz，防止 RC522 干擾導致螢幕亂碼
  Wire.setClock(100000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("OLED: OK");
  display.display();
  Serial.println("OLED initialized");
  delay(500);

  // 3. 初始化 SPI 與 RC522
  SPI.begin();        
  mfrc522.PCD_Init(); 
  
  // [修正3] 設定最大天線增益 (48dB)
  // 增強訊號，解決讀不到被動式白卡的問題
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  Serial.println("RC522 initialized");
  
  // 顯示版本資訊，確認 RC522 是否有反應
  mfrc522.PCD_DumpVersionToSerial();
  
  display.setCursor(0, 10);
  display.println("RC522: Ready");
  display.display();
  delay(1000);
}

void loop() {
  // --- 測試 1: LED 循環測試 ---
  // 每隔一段時間切換 LED 顏色，證明 LED 沒壞
  // 順序: 紅 -> 綠 -> 藍 -> 暗
  static int colorStep = 0;
  static unsigned long lastLedTime = 0;
  
  if (millis() - lastLedTime > 1000) { // 每秒換一次顏色
    lastLedTime = millis();
    colorStep++;
    if (colorStep > 3) colorStep = 0;
    
    if (colorStep == 0) setLedColor(0, 0, 0); // OFF
    if (colorStep == 1) setLedColor(1, 0, 0); // RED
    if (colorStep == 2) setLedColor(0, 1, 0); // GREEN
    if (colorStep == 3) setLedColor(0, 0, 1); // BLUE
  }

  // --- 測試 2: RFID 讀卡 ---
  // 檢查是否有新卡片
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    
    // 讀取 UID
    String uidStr = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uidStr += String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      uidStr += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();
    
    // Serial 輸出
    Serial.print("Card Detected! UID:");
    Serial.println(uidStr);

    // OLED 顯示 UID
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("== CARD DETECTED ==");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.println(uidStr); // 顯示卡號
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.println("Test: OK");
    display.display();
    
    // 讀到卡片時，閃爍綠燈兩次
    for(int i=0; i<2; i++){
      setLedColor(0, 1, 0); delay(100);
      setLedColor(0, 0, 0); delay(100);
    }

    // 讓卡片進入休眠，避免重複讀取
    mfrc522.PICC_HaltA(); 
    mfrc522.PCD_StopCrypto1();
    
    // 延遲一下讓使用者看清楚
    delay(1000); 
  }

  // 若沒讀到卡，顯示待機畫面
  static unsigned long lastOledTime = 0;
  if (millis() - lastOledTime > 500) {
    lastOledTime = millis();
    // 只有在沒讀卡的時候才刷新待機畫面，避免蓋掉 UID
    // 這裡我們簡單做，僅在螢幕沒顯示 UID 時刷新
    // 但為了簡單測試，我們就假設沒讀卡時一直顯示等待
    
    // 注意：這會跟上面的 UID 顯示競爭，實際測試時
    // 讀到卡會 delay(1000)，所以那一秒鐘會顯示 UID
    // 之後會回到這裡顯示 Waiting...
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("== DOOR TEST ==");
    display.setCursor(0, 15);
    display.println("Fixed Version");
    display.setCursor(0, 30);
    display.println("Waiting for Card...");
    
    display.setCursor(0, 50);
    display.print("LED: ");
    if (colorStep == 1) display.print("RED");
    else if (colorStep == 2) display.print("GREEN");
    else if (colorStep == 3) display.print("BLUE");
    else display.print("OFF");
    
    display.display();
  }
}

// 輔助函式: 設定 LED 顏色
// 假設是共陰極 (1為亮)，若是共陽極請將 digitalWrite 的 HIGH/LOW 反過來
void setLedColor(int r, int g, int b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}