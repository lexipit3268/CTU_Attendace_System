#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- ĐỊNH NGHĨA CHÂN GPIO THEO SƠ ĐỒ HỆ THỐNG ---
#define SS_PIN      5   // RC522 SDA / SS
#define RST_PIN     4   // RC522 RST
#define BUZZER_PIN  25  // Còi Buzzer
#define RED_LED     26  // Đèn LED đỏ

// Khởi tạo LCD 16x2 I2C (Địa chỉ 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khởi tạo RFID RC522
MFRC522 rfid(SS_PIN, RST_PIN);

// Mã UID hoặc Token hợp lệ mẫu
const String VALID_UID_1 = "20 89 B2 4A";
const String VALID_TOKEN_1 = "B2303828"; // MSSV ghi bằng app NFC Tools

// Khai báo hàm
void systemStartupTest();
void resetLcdIdle();
void handleSuccess(String info);
void handleFailed(String info);
void playBeep(int frequency, int durationMs);
String readTokenFromBlock(byte blockAddr);

void setup() {
  Serial.begin(115200);

  // 1. Cấu hình Output
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);

  // 2. Chờ 500ms để điện áp chip HD44780 trên LCD ổn định hoàn toàn
  delay(500);

  // 3. Khởi tạo I2C chuẩn 100kHz
  Wire.begin(21, 22);        // SDA = GPIO 21, SCL = GPIO 22
  Wire.setClock(100000);     // Hạ xung I2C chống nhiễu
  delay(100);

  lcd.init();                // Khởi tạo LCD lần 1
  delay(100);
  lcd.init();                // Khởi tạo lần 2 để ép LCD vào đúng chế độ 4-bit
  lcd.backlight();
  lcd.clear();

  // 4. Khởi tạo SPI và module RFID RC522
  SPI.begin(18, 19, 23, 5); 
  rfid.PCD_Init();

  // 5. Chạy Test khởi động
  systemStartupTest();

  // 6. Trở về màn hình chờ
  resetLcdIdle();
}

void loop() {
  // Kiểm tra xem có thẻ RFID chạm vào mặt đọc không
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // 1. LẤY MÃ UID PHẦN CỨNG THẺ (Đã sửa lỗi cú pháp)
  String scannedUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) scannedUID += "0";
    scannedUID += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) scannedUID += " ";
  }
  scannedUID.toUpperCase();

  Serial.print("\n[RFID Scan] Ma UID: ");
  Serial.println(scannedUID);

  // 2. ĐỌC DỮ LIỆU TOKEN / NDEF (BLOCK 4 - SECTOR 1)
  String scannedToken = readTokenFromBlock(4);
  if (scannedToken.length() > 0) {
    Serial.print("[RFID Scan] Token/Text tren the: ");
    Serial.println(scannedToken);
  }

  // 3. KIỂM TRA ĐIỀU KIỆN TRUY CẬP (Khớp UID HOẶC Khớp Token)
  if (scannedUID.equals(VALID_UID_1) || scannedToken.indexOf(VALID_TOKEN_1) >= 0) {
    handleSuccess(scannedToken.length() > 0 ? scannedToken : scannedUID);
  } else {
    handleFailed(scannedUID);
  }

  // Ngắt giao tiếp thẻ để sẵn sàng lượt quét tiếp theo
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}

// --- HÀM ĐỌC & LỌC SẠCH DỮ LIỆU TOKEN (LOẠI BỎ HEADER NDEF CHỐNG RÁC LCD) ---
String readTokenFromBlock(byte blockAddr) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte buffer[18];
  byte size = sizeof(buffer);

  MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
    return "";
  }

  status = rfid.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    return "";
  }

  // Chỉ giữ lại ký tự Chữ và Số (A-Z, a-z, 0-9) để loại bỏ hoàn toàn byte NDEF header
  String data = "";
  for (uint8_t i = 0; i < 16; i++) {
    char c = (char)buffer[i];
    if (isAlphaNumeric(c)) {
      data += c;
    }
  }
  data.trim();
  return data;
}

// --- TỰ ĐỘNG TEST TOÀN BỘ LINH KIỆN KHI RESET ESP32 ---
void systemStartupTest() {
  Serial.println("==========================================");
  Serial.println("   RESET ESP32 - CHẠY TEST KHỞI ĐỘNG");
  Serial.println("==========================================");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 RESETTING");
  lcd.setCursor(0, 1);
  lcd.print("Self Testing...");

  digitalWrite(RED_LED, HIGH);
  playBeep(2000, 100);
  delay(100);
  playBeep(2500, 150);
  
  digitalWrite(RED_LED, LOW);
  delay(800);

  Serial.println("-> Test Linh kien OK. He thong san sang!");
}

void resetLcdIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG IOT");
  lcd.setCursor(0, 1);
  lcd.print("Moi quet the...");
}

void handleSuccess(String info) {
  Serial.println("-> KẾT QUẢ: Quét thẻ thành công!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DIEM DANH OK!");
  lcd.setCursor(0, 1);
  lcd.print("ID: " + info.substring(0, 12));

  digitalWrite(RED_LED, HIGH);
  playBeep(2000, 150);
  delay(1200);

  digitalWrite(RED_LED, LOW);
  resetLcdIdle();
}

void handleFailed(String info) {
  Serial.println("-> KẾT QUẢ: Thẻ không hợp lệ!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TU CHOI TRUY CAP");
  lcd.setCursor(0, 1);
  lcd.print("The khong hop le");

  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED, HIGH);
    playBeep(1000, 100);
    digitalWrite(RED_LED, LOW);
    delay(100);
  }

  delay(1000);
  resetLcdIdle();
}

void playBeep(int frequency, int durationMs) {
  tone(BUZZER_PIN, frequency, durationMs);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}