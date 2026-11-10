#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- ĐỊNH NGHĨA CHÂN GPIO THEO SƠ ĐỒ CHUẨN ---
#define SS_PIN      5   // RC522 SDA / SS (GPIO 5)
#define RST_PIN     4   // RC522 RST (GPIO 4)
#define BUZZER_PIN  25  // Còi Buzzer (GPIO 25)
#define RED_LED     26  // Đèn LED đỏ (GPIO 26)

// Khởi tạo LCD 16x2 I2C (Địa chỉ 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khởi tạo RFID RC522
MFRC522 rfid(SS_PIN, RST_PIN);

// --- NẠP MÃ UID THỰC TẾ THẺ NFC CỦA BẠN ---
const String MY_CARD_UID   = "7B A5 2C 54"; // Mã UID phần cứng thẻ của bạn
const String MY_STUDENT_ID = "B2303828";    // Token/MSSV nạp qua app NFC Tools

// Khai báo prototype các hàm
void systemStartupTest();
void resetLcdIdle();
void handleAccessGranted(String info);
void handleAccessDenied(String info);
void playBeep(int frequency, int durationMs);
String readTokenFromBlock(byte blockAddr);

void setup() {
  Serial.begin(115200);

  // 1. Cấu hình Output
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);

  // 2. Chờ 500ms chờ điện áp chip HD44780 trên LCD ổn định
  delay(500);

  // 3. Khởi tạo I2C chuẩn 100kHz
  Wire.begin(21, 22);        // SDA = GPIO 21, SCL = GPIO 22
  Wire.setClock(100000);     // Hạ xung I2C chống nhiễu
  delay(100);

  lcd.init();                // Khởi tạo LCD lần 1
  delay(100);
  lcd.init();                // Khởi tạo lần 2 để xóa cặn lệnh cũ
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
  // Kiểm tra xem có thẻ RFID/NFC chạm vào mặt đọc không
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // 1. ĐỌC MÃ UID PHẦN CỨNG THẺ
  String scannedUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) scannedUID += "0";
    scannedUID += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) scannedUID += " ";
  }
  scannedUID.toUpperCase();

  Serial.print("\n[RFID/NFC Scan] Da quet UID: ");
  Serial.println(scannedUID);

  // 2. ĐỌC DỮ LIỆU TOKEN / NDEF (BLOCK 4 - SECTOR 1) GHI BẰNG ĐIỆN THOẠI
  String scannedToken = readTokenFromBlock(4);
  if (scannedToken.length() > 0) {
    Serial.print("[RFID/NFC Scan] Token/MSSV tren the: ");
    Serial.println(scannedToken);
  }

  // 3. KIỂM TRA ĐIỀU KIỆN TRUY CẬP (Khớp UID 7B A5 2C 54 HOẶC Khớp Token MSSV)
  if (scannedUID.equals(MY_CARD_UID) || scannedToken.indexOf(MY_STUDENT_ID) >= 0) {
    handleAccessGranted("SV: B2303828");
  } else {
    handleAccessDenied(scannedUID);
  }

  // Ngắt giao tiếp thẻ để sẵn sàng lượt quét tiếp theo
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}

// --- HÀM ĐỌC & LỌC DỮ LIỆU TOKEN TỪ THẺ NFC ---
String readTokenFromBlock(byte blockAddr) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF; // Key A mặc định

  byte buffer[18];
  byte size = sizeof(buffer);

  MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) return "";

  status = rfid.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) return "";

  // Chỉ lọc lấy ký tự Chữ và Số (chống rác NDEF Header trên LCD)
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

void handleAccessGranted(String info) {
  Serial.println("-> KẾT QUẢ: Quét đúng thẻ chính chủ!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DIEM DANH OK!");
  lcd.setCursor(0, 1);
  lcd.print(info);

  digitalWrite(RED_LED, HIGH);
  playBeep(2000, 150);
  delay(1500);

  digitalWrite(RED_LED, LOW);
  resetLcdIdle();
}

void handleAccessDenied(String info) {
  Serial.println("-> KẾT QUẢ: Thẻ không hợp lệ!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TU CHOI TRUY CAP");
  lcd.setCursor(0, 1);
  lcd.print("UID:" + info.substring(0, 12));

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