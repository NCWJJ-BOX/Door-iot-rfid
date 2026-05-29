# Door IoT RFID System

Smart door access control system using RFID cards and OTP codes. ESP32 with RFID reader, keypad, LCD, and relay controls a door lock, authenticated via FastAPI backend with MySQL.

## Architecture

```
RFID Card / Keypad (OTP)
        │
        ▼
    ESP32 + MFRC522 + LCD + Relay
        │ HTTP
        ▼
  FastAPI Backend + MySQL
        │
        ▼
    Access Logging
```

## Features

- RFID card authentication against MySQL database
- OTP code authentication (6-digit, regenerated every 2 minutes)
- Web-based door control (open/close via API)
- Access logging with timestamps
- LCD display showing time and user info
- WiFi auto-connect via WiFiManager
- Auto-close door after timeout

## Tech Stack

- **Backend:** Python FastAPI, MySQL
- **Firmware:** Arduino (ESP32), C++
- **Hardware:** MFRC522 RFID, 4x4 keypad (I2C), LCD I2C 16x2, relay module, Hall sensor

## Project Structure

```
Door-iot-rfid/
├── Arduino_final_Project.ino   # ESP32 firmware
├── fast_api.py                 # FastAPI backend API
└── README.md
```

## Setup

### Backend

```bash
pip install fastapi uvicorn mysql-connector-python
uvicorn fast_api:app --reload
```

### Firmware

1. Open `Arduino_final_Project.ino` in Arduino IDE
2. Configure WiFi SSID/password and API endpoint URL
3. Flash to ESP32

## Hardware Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| MFRC522 SDA | GPIO 5 |
| MFRC522 SCK | GPIO 18 |
| MFRC522 MOSI | GPIO 23 |
| MFRC522 MISO | GPIO 19 |
| Relay | GPIO 26 |
| LCD I2C | SDA/SCL |
| Keypad I2C | SDA/SCL |
