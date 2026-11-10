#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include "../ESP32_ATTENDANCE_SYSTEM/WifiConfig.h"

// --- ĐỊNH NGHĨA CHÂN GPIO HỆ THỐNG ---
#define SS_PIN      5   // RC522 SDA / SS (GPIO 5)
#define RST_PIN     4   // RC522 RST (GPIO 4)
#define BUZZER_PIN  25  // Còi Buzzer (GPIO 25)
#define RED_LED     26  // Đèn LED đỏ (GPIO 26)

// Khởi tạo LCD 16x2 I2C (Địa chỉ 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khởi tạo RFID RC522
MFRC522 rfid(SS_PIN, RST_PIN);

// --- CẤU TRÚC VÀ DANH SÁCH MAPPING SINH VIÊN ---
struct Student {
  String uid;   // Mã UID không dấu cách, viết hoa
  String mssv;  // Mã số sinh viên
  String name;  // Tên hiển thị trên LCD
};

Student studentList[] = {
  {"7340D637", "B2303828", "N.P.Loc"},
  {"7BA52C54", "B2303829", "V.T.Nam"},
  {"E3A53038", "B2303844", "T.T.Phuc"}
};

const int TOTAL_STUDENTS = sizeof(studentList) / sizeof(studentList[0]);

// Khai báo prototype hàm
void connectWiFi();
void resetLcdIdle();
void playBeep(int frequency, int durationMs);

void setup() {
  Serial.begin(115200);

  // 1. Cấu hình chân Output
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);

  delay(500);

  // 2. Khởi tạo I2C 100kHz và LCD
  Wire.begin(21, 22);      // SDA = GPIO 21, SCL = GPIO 22
  Wire.setClock(100000);   // Hạ xung I2C chống nhiễu
  delay(100);

  lcd.init();
  delay(100);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // 3. Khởi tạo SPI và module RFID RC522
  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();

  // 4. Kết nối Wi-Fi
  connectWiFi();

  // 5. Chuyển sang màn hình chờ
  resetLcdIdle();
}

void loop() {
  // Kiểm tra nếu thẻ NFC được đưa lại gần
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // 1. ĐỌC VÀ CHUẨN HÓA MÃ UID (VIẾT HOA, KHÔNG KHOẢNG TRẮNG)
  String scannedUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) scannedUID += "0";
    scannedUID += String(rfid.uid.uidByte[i], HEX);
  }
  scannedUID.toUpperCase();

  Serial.print("\n[RFID Scan] Clean UID: ");
  Serial.println(scannedUID);

  // 2. TRA CỨU MAPPING TRONG DANH SÁCH
  int foundIndex = -1;
  for (int i = 0; i < TOTAL_STUDENTS; i++) {
    if (scannedUID.equals(studentList[i].uid)) {
      foundIndex = i;
      break;
    }
  }

  // 3. XỬ LÝ KẾT QUẢ ĐIỂM DANH
  if (foundIndex != -1) {
    // THẢM HỢP LỆ (Có trong danh sách)
    Student s = studentList[foundIndex];
    Serial.println("-> KET QUA: Diem danh thanh cong!");
    Serial.println("   MSSV: " + s.mssv + " | Ho Ten: " + s.name);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DIEM DANH OK!");
    lcd.setCursor(0, 1);
    lcd.print(s.mssv + " " + s.name); // Hiển thị: "B2303828 N.P.Loc"

    digitalWrite(RED_LED, HIGH);
    playBeep(2000, 150);
    delay(1500);
    digitalWrite(RED_LED, LOW);
  } else {
    // THẺ KHÔNG HỢP LỆ
    Serial.println("-> KET QUA: The chua duoc dang ky trong he thong!");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TU CHOI TRUY CAP");
    lcd.setCursor(0, 1);
    lcd.print("UID:" + scannedUID);

    for (int i = 0; i < 3; i++) {
      digitalWrite(RED_LED, HIGH);
      playBeep(1000, 100);
      digitalWrite(RED_LED, LOW);
      delay(100);
    }
    delay(1000);
  }

  resetLcdIdle();

  // Ngắt giao tiếp để sẵn sàng cho lần quẹt tiếp theo
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}

// --- HÀM KẾT NỐI WI-FI ---
void connectWiFi() {
  Serial.print("\n[Wi-Fi] Dang ket noi toi SSID: ");
  Serial.println(WIFI_SSID);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CONNECTING WIFI");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Wi-Fi] Ket noi thanh cong!");
    Serial.print("[Wi-Fi] IP Address: ");
    Serial.println(WiFi.localIP());

    lcd.setCursor(0, 0);
    lcd.print("WIFI CONNECTED!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());

    playBeep(2000, 100);
    delay(100);
    playBeep(2500, 150);
  } else {
    Serial.println("\n[Wi-Fi] Ket noi That bai! Chay che do Offline.");
    lcd.setCursor(0, 0);
    lcd.print("WIFI FAILED!");
    lcd.setCursor(0, 1);
    lcd.print("Mode: Offline");
    delay(2000);
  }
  delay(1200);
}

// Màn hình chờ mặc định
void resetLcdIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG IOT");
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 1);
    lcd.print("Moi quet the...");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("[Offline] Quet..");
  }
}

// Hàm phát âm thanh còi
void playBeep(int frequency, int durationMs) {
  tone(BUZZER_PIN, frequency, durationMs);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}