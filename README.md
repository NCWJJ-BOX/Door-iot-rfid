# Door IoT RFID System

ระบบควบคุมประตูด้วย RFID และ OTP สำหรับ IoT เป็นโปรเจคชิ้นงานจบปี 1 ที่พัฒนาขึ้นเพื่อศึกษาและพัฒนาระบบ Smart Door โดยใช้ RFID เชื่อมต่อกับ MySQL และระบบ OTP

## ภาพรวมระบบ

โปรเจคนี้เป็นระบบควบคุมการเปิด-ปิดประตูอัจฉริยะ โดยสามารถใช้งานได้ 2 รูปแบบ:

1. **RFID Authentication** - ใช้บัตร RFID ในการยืนยันตัวตน
2. **OTP Authentication** - ใช้รหัส OTP 6 หลักในการเปิดประตู

## การทำงานของระบบ

RFID อ่านค่าจากบัตรและส่งไปที่ API เพื่อตรวจสอบที่ Database ว่าตรงกันหรือไม่ เพื่อให้ประตูเปิดหรือปิด นอกจากนี้ยังมีระบบ OTP ที่สุ่มเลข 6 หลักเพื่อให้กรอกที่หน้าประตูได้ สามารถดูหลังบ้านได้ว่ามีใครเข้ามาในห้องบ้าง

## เทคโนโลยีที่ใช้

- **Backend**: Python FastAPI
- **Firmware**: Arduino (ESP32)
- **Database**: MySQL
- **Hardware**: RFID Reader (MFRC522), Keypad 4x4, LCD I2C, Relay, Hall Sensor

## ฟีเจอร์

- ระบบยืนยันตัวตนด้วย RFID Card
- ระบบ OTP สุ่มรหัด 6 หลัก อัปเดตทุก 2 นาที (สุ่มใหม่ทุกครั้งที่มีการใช้ไปแล้ว)
- ควบคุมการเปิด-ปิดประตูผ่านเว็บไซต์
- บันทึกประวัติการเข้าใช้งาน (Log)
- แสดงเวลาปัจจุบันบน LCD

## หน้าจอผู้ดูแลระบบ

![Admin Interface](./Screenshot%202024-08-02%20132434.png)

สามารถเพิ่ม ลบ แก้ไข ได้และเปิด/ปิด บัตรแต่ละใบได้

![Admin Interface](./Screenshot%202024-08-02%20132444.png)

มีปุ่มเปิดประตูจากหน้าเว็บได้เลยสำหรับผู้ดูแล

## ประวัติการเข้าใช้งาน

![Access Log](./Screenshot%202024-08-02%20132858.png)

สามารถตรวจสอบได้ว่าใครเข้ามาในห้องบ้างพร้อม timestamp

## การติดตั้ง

### 1. Backend

```bash
# ติดตั้ง dependencies
pip install fastapi uvicorn mysql-connector-python

# แก้ไขการตั้งค่า Database ใน fast_api.py
MYSQL_CONFIG = {
    "host": "localhost",
    "user": "your_username",
    "password": "your_password",
    "database": "your_database",
    "autocommit": True,
}

# รัน server
uvicorn fast_api:app --reload
```

### 2. Database

สร้างตารางที่จำเป็น:

```sql
-- ตารางผู้ใช้งาน RFID
CREATE TABLE esp32 (
    id INT AUTO_INCREMENT PRIMARY KEY,
    rfid VARCHAR(255) UNIQUE,
    pin VARCHAR(255),
    user VARCHAR(255)
);

-- ตาราง OTP
CREATE TABLE otp (
    id INT AUTO_INCREMENT PRIMARY KEY,
    otp VARCHAR(6),
    timestamp DATETIME
);

-- ตารางบันทึกการเข้าใช้
CREATE TABLE access_log (
    id INT AUTO_INCREMENT PRIMARY KEY,
    rfid VARCHAR(255),
    status VARCHAR(50),
    timestamp DATETIME
);
```

### 3. Arduino

เปิดไฟล์ `Arduino_final_Project.ino` ใน Arduino IDE และแก้ไขค่า:

```cpp
const char* API_ENDPOINT = "https://your-api-domain/rfid";
const char* FAST_API_URL = "https://your-fastapi-domain";
const char* SERVER_URL = "https://your-server-domain/get_otp_api";
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/rfid` | ตรวจสอบ RFID |
| POST | `/check_code` | ตรวจสอบ OTP |
| GET | `/update_data/{value}` | ควบคุมประตู (0=lock, 1=unlock) |
| GET | `/get_otp_api` | สถานะประตูปัจจุบัน |
| GET | `/health` | Health check |

## การใช้งาน

### การเปิดประตูด้วย RFID
1. แตะบัตร RFID ที่เครื่องอ่าน
2. ระบบจะส่ง RFID ไปตรวจสอบที่ API
3. หากถูกต้อง ประตูจะเปิดและแสดงข้อมูลผู้ใช้บน LCD

### การเปิดประตูด้วย OTP
1. กดปุ่ม 'C' บน keypad เพื่อเข้าโหมด OTP
2. กรอกรหัส OTP 6 หลัก
3. หากถูกต้อง ประตูจะเปิด

### การเปิดประตูผ่านเว็บ
- เรียก `GET /update_data/1` เพื่อเปิดประตู
- เรียก `GET /update_data/0` เพื่อปิดประตู

## ไฟล์ในโปรเจค

```
.
├── Arduino_final_Project.ino   # Firmware สำหรับ ESP32
├── fast_api.py                 # Backend API
├── README.md                   # เอกสารนี้
└── Screenshot*.png             # ภาพตัวอย่างหน้าจอ
```

## License

MIT License
