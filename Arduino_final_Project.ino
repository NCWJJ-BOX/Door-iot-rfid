#include <WiFiManager.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ============ CONFIGURATION ============
#define I2CADDR 0x20
#define SS_PIN 5
#define RST_PIN 16
const int RELAY_PIN = 13;
const int BUTTON_PIN = 15;
const int HALL_SENSOR_PIN = 12;

// API Endpoints - UPDATE THESE BEFORE USE
const char* API_ENDPOINT = "https://your-api-domain/rfid";
const char* FAST_API_URL = "https://your-fastapi-domain";
const char* SERVER_URL = "https://your-server-domain/get_otp_api";

const long UTC_OFFSET_SECONDS = 25200;  // UTC+7 for Thailand

// ============ GLOBAL OBJECTS ============
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS);
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '4', '7', '*' },
  { '2', '5', '8', '0' },
  { '3', '6', '9', '#' },
  { 'A', 'B', 'C', 'D' }
};
byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6, 7 };
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2CADDR, PCF8574);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============ STATE VARIABLES ============
String inputCode;
bool otpMode = false;
int storedValue = -1;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  Wire.begin();
  
  keypad.begin(makeKeymap(keys));
  mfrc522.PCD_Init();
  
  WiFiManager wifiManager;
  wifiManager.autoConnect("Door_IoT");
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(HALL_SENSOR_PIN, INPUT);
  
  lcd.begin();
  lcd.backlight();
  
  Serial.println("WiFi Connected!");
  displayWelcomeMessage();
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    handleManualOpen();
    return;
  }
  
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    handleRFID();
    return;
  }
  
  char key = keypad.getKey();
  if (key) {
    handleKeypad(key);
  }
  
  checkDoorStatus();
}

void displayWelcomeMessage() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Door IoT RFID");
  lcd.setCursor(3, 1);
  lcd.print("System Ready");
  delay(2000);
  lcd.clear();
}

void handleManualOpen() {
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Manual Open Door");
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Open Door");
  delay(5000);
  lcd.clear();
}

void handleRFID() {
  String uid = getCardUID();
  Serial.println("Card UID: " + uid);
  
  sendToAPI(uid);
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(2000);
  lcd.clear();
}

String getCardUID() {
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  return uidString;
}

void handleKeypad(char key) {
  if (key == 'C') {
    otpMode = true;
    inputCode = "";
    Serial.println("OTP Mode Activated");
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("OTP Mode");
    delay(500);
    return;
  }
  
  if (key == 'D') {
    if (!inputCode.isEmpty()) {
      inputCode.remove(inputCode.length() - 1);
      displayOTPInput();
    }
    return;
  }
  
  if (otpMode) {
    if (key >= '0' && key <= '9') {
      inputCode += key;
      Serial.print(key);
      displayOTPInput();
      
      if (inputCode.length() == 6) {
        Serial.println();
        sendToFastAPI(inputCode.c_str());
        delay(2000);
        otpMode = false;
        inputCode = "";
        lcd.clear();
      }
    }
  }
}

void displayOTPInput() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("OTP Mode");
  lcd.setCursor(0, 1);
  lcd.print("Code: ");
  lcd.print(inputCode);
}

void checkDoorStatus() {
  if (digitalRead(HALL_SENSOR_PIN) == 0) {
    digitalWrite(RELAY_PIN, LOW);
    
    lcd.setCursor(1, 0);
    lcd.print("Put Your Card");
    
    timeClient.update();
    lcd.setCursor(1, 1);
    lcd.print("Time: ");
    lcd.print(timeClient.getFormattedTime());
    
    checkStoredValue();
  }
}

void sendToAPI(String uid) {
  HTTPClient http;
  http.begin(API_ENDPOINT);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String postData = "rfid_tag=" + uid;
  int httpCode = http.POST(postData);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.println("API Response: " + response);
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      Serial.println("JSON Parse Error: " + String(error.c_str()));
      http.end();
      return;
    }
    
    const char* status = doc["status"];
    
    if (strcmp(status, "unlock") == 0) {
      const char* user = doc["user"];
      const char* pin = doc["pin"];
      handleUnlock(user, pin);
    } else if (strcmp(status, "lock") == 0) {
      handleLock();
    }
  } else {
    Serial.println("HTTP Error: " + String(httpCode));
  }
  
  http.end();
}

void handleUnlock(const char* user, const char* pin) {
  digitalWrite(RELAY_PIN, HIGH);
  
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Unlock Door");
  
  // Display user info 3 times (original behavior)
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(0, 1);
    lcd.print("Name: ");
    lcd.print(user);
    delay(1000);
    
    lcd.setCursor(0, 1);
    lcd.print("              ");
    lcd.setCursor(0, 1);
    lcd.print("ID: ");
    lcd.print(pin);
    delay(1000);
  }
  
  // Clear LCD
  lcd.setCursor(0, 1);
  lcd.print("              ");
}

void handleLock() {
  digitalWrite(RELAY_PIN, LOW);
  
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Lock Door");
  
  for (int i = 5; i >= 0; i--) {
    lcd.setCursor(4, 1);
    lcd.print("Wait ");
    lcd.print(i);
    lcd.print(" s");
    delay(1000);
  }
}

void sendToFastAPI(const char* code) {
  HTTPClient http;
  String url = String(FAST_API_URL) + "/check_code";
  
  Serial.println("Sending to: " + url);
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String postData = "code=" + String(code);
  int httpCode = http.POST(postData);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.println("Response: " + response);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    
    const char* status = doc["status"];
    const char* timeStr = doc["time"];
    
    if (strcmp(status, "success") == 0) {
      digitalWrite(RELAY_PIN, HIGH);
      lcd.clear();
      lcd.setCursor(3, 0);
      lcd.print("Unlock Door");
      lcd.setCursor(4, 1);
      lcd.print(timeStr);
    } else {
      digitalWrite(RELAY_PIN, LOW);
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print("Lock Door");
      lcd.setCursor(4, 1);
      lcd.print(timeStr);
    }
    
    delay(2000);
  } else {
    Serial.println("HTTP Error: " + http.errorToString(httpCode));
  }
  
  http.end();
}

void checkStoredValue() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.begin(SERVER_URL);
  
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Door Status: " + payload);
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      int newValue = doc["stored_value"];
      
      if (newValue != storedValue) {
        storedValue = newValue;
        
        if (storedValue == 1) {
          digitalWrite(RELAY_PIN, HIGH);
          delay(5000);
        } else if (storedValue == 0) {
          digitalWrite(RELAY_PIN, LOW);
        }
      }
    }
  }
  
  http.end();
}
