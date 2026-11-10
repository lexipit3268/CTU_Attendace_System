#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "WifiConfig.h"

#define SS_PIN      5
#define RST_PIN     4
#define BUZZER_PIN  25
#define RED_LED     26

const char* SERVER_URL  = "http://192.168.137.1:3000/api/attendance/scan";

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

struct FallbackStudent {
  String uid;
  String mssv;
  String name;
};

const FallbackStudent fallbackList[] = {
  {"7340D637", "B2303828", "N.P.Loc"},
  {"7BA52C54", "B2303829", "V.T.Nam"},
  {"E3A53038", "B2303844", "T.T.Phuc"}
};
const int TOTAL_FALLBACK = sizeof(fallbackList) / sizeof(fallbackList[0]);

void connectWiFi();
void resetLcdIdle();
void playBeep(int frequency, int durationMs);
String getJsonValue(const String& json, const String& key);
void handleOfflineFallback(const String& scannedUID);

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);

  delay(500);

  Wire.begin(21, 22);
  Wire.setClock(100000);
  delay(100);

  lcd.init();
  delay(100);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();

  connectWiFi();
  resetLcdIdle();
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String scannedUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) scannedUID += "0";
    scannedUID += String(rfid.uid.uidByte[i], HEX);
  }
  scannedUID.toUpperCase();

  Serial.println("\n[Card Scanned] UID: " + scannedUID);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DANG XU LY...");
  lcd.setCursor(0, 1);
  lcd.print("UID: " + scannedUID);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_URL);
    http.setTimeout(3000);
    http.addHeader("Content-Type", "application/json");

    String requestBody = "{\"uid\":\"" + scannedUID + "\"}";
    int httpCode = http.POST(requestBody);

    if (httpCode == 200) {
      String response = http.getString();
      Serial.println("[Server Response] " + response);

      String status = getJsonValue(response, "status");

      if (status == "SUCCESS") {
        String mssv = getJsonValue(response, "mssv");
        String name = getJsonValue(response, "name");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DIEM DANH OK!");
        lcd.setCursor(0, 1);
        String info = mssv + " " + name;
        lcd.print(info.substring(0, 16));

        digitalWrite(RED_LED, HIGH);
        playBeep(2000, 150);
        delay(1500);
        digitalWrite(RED_LED, LOW);
      } else if (status == "ENROLL_MODE") {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DANG KY THE MOI");
        lcd.setCursor(0, 1);
        lcd.print("UID:" + scannedUID);

        for (int i = 0; i < 2; i++) {
          digitalWrite(RED_LED, HIGH);
          playBeep(2500, 100);
          digitalWrite(RED_LED, LOW);
          delay(100);
        }
        delay(1200);
      } else {
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
    } else {
      Serial.printf("[HTTP] POST failed, code: %d\n", httpCode);
      handleOfflineFallback(scannedUID);
    }
    http.end();
  } else {
    handleOfflineFallback(scannedUID);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  resetLcdIdle();
  delay(800);
}

void handleOfflineFallback(const String& scannedUID) {
  int found = -1;
  for (int i = 0; i < TOTAL_FALLBACK; i++) {
    if (scannedUID.equals(fallbackList[i].uid)) {
      found = i;
      break;
    }
  }

  if (found != -1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OFFLINE: OK!");
    lcd.setCursor(0, 1);
    String info = fallbackList[found].mssv + " " + fallbackList[found].name;
    lcd.print(info.substring(0, 16));

    digitalWrite(RED_LED, HIGH);
    playBeep(2000, 150);
    delay(1500);
    digitalWrite(RED_LED, LOW);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OFFLINE: TU CHOI");
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
}

void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CONNECTING WIFI");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected, IP: " + WiFi.localIP().toString());
    lcd.setCursor(0, 0);
    lcd.print("WIFI CONNECTED!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());

    playBeep(2000, 100);
    delay(100);
    playBeep(2500, 150);
  } else {
    Serial.println("\n[WiFi] Failed. Offline mode active.");
    lcd.setCursor(0, 0);
    lcd.print("WIFI FAILED!");
    lcd.setCursor(0, 1);
    lcd.print("Mode: Offline");
    delay(1500);
  }
  delay(1000);
}

void resetLcdIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG IOT");
  lcd.setCursor(0, 1);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("Moi quet the...");
  } else {
    lcd.print("[Offline] Quet..");
  }
}

void playBeep(int frequency, int durationMs) {
  tone(BUZZER_PIN, frequency, durationMs);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

String getJsonValue(const String& json, const String& key) {
  String search = "\"" + key + "\":\"";
  int start = json.indexOf(search);
  if (start == -1) {
    search = "\"" + key + "\":";
    start = json.indexOf(search);
    if (start == -1) return "";
    start += search.length();
    int end = json.indexOf(",", start);
    if (end == -1) end = json.indexOf("}", start);
    if (end == -1) return "";
    String val = json.substring(start, end);
    val.trim();
    return val;
  }
  start += search.length();
  int end = json.indexOf("\"", start);
  if (end == -1) return "";
  return json.substring(start, end);
}
