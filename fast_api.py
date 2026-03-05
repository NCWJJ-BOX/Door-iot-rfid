"""
Door IoT RFID Backend API
FastAPI application for RFID and OTP authentication system
"""

from contextlib import asynccontextmanager
from datetime import datetime, timezone, timedelta
from typing import Optional

import asyncio
import random

from fastapi import BackgroundTasks, FastAPI, Form, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel

# ============= CONFIGURATION =============
# Update these values with your MySQL credentials
MYSQL_CONFIG = {
    "host": "localhost",
    "user": "your_username",
    "password": "your_password",
    "database": "your_database",
    "autocommit": True,
}

# CORS settings - update for production
CORS_ORIGINS = ["http://localhost:3000", "http://localhost:8080"]

# ============= GLOBAL STATE =============
class DoorState:
    """Manages door state with thread-safe operations"""
    def __init__(self):
        self.value: Optional[int] = None
        self.last_update: Optional[float] = None
        self.auto_close_seconds = 10

    def update(self, value: int) -> None:
        self.value = value
        import time
        self.last_update = time.time()

    def get_value(self) -> Optional[int]:
        import time
        if self.value is not None and self.last_update is not None:
            if time.time() - self.last_update >= self.auto_close_seconds:
                self.value = 0
        return self.value

door_state = DoorState()


# ============= DATABASE FUNCTIONS =============
def get_db_connection():
    """Create a new database connection"""
    import mysql.connector
    return mysql.connector.connect(**MYSQL_CONFIG)


def check_rfid_in_mysql(rfid_tag: str) -> tuple[bool, Optional[str], Optional[str]]:
    """
    Check if RFID tag exists in database
    Returns: (is_valid, username, pin)
    """
    conn = get_db_connection()
    cursor = conn.cursor()
    
    try:
        query = "SELECT rfid, pin, user FROM esp32 WHERE rfid = %s"
        cursor.execute(query, (rfid_tag,))
        result = cursor.fetchone()
        
        if result:
            rfid, pin, user = result
            return True, user, pin
        return False, None, None
    finally:
        cursor.close()
        conn.close()


def get_latest_otp() -> Optional[str]:
    """Get the most recent OTP from database"""
    conn = get_db_connection()
    cursor = conn.cursor()
    
    try:
        query = "SELECT otp FROM otp ORDER BY timestamp DESC LIMIT 1"
        cursor.execute(query)
        result = cursor.fetchone()
        return result[0] if result else None
    finally:
        cursor.close()
        conn.close()


def store_otp_in_mysql(otp: str) -> None:
    """Store OTP in database, replacing any existing OTP"""
    conn = get_db_connection()
    cursor = conn.cursor()
    
    try:
        # Delete old OTP
        cursor.execute("DELETE FROM otp")
        
        # Insert new OTP
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        cursor.execute("INSERT INTO otp (otp, timestamp) VALUES (%s, %s)", (otp, timestamp))
        conn.commit()
    finally:
        cursor.close()
        conn.close()


def log_access(rfid_tag: str, status: str) -> None:
    """Log door access attempt to database"""
    conn = get_db_connection()
    cursor = conn.cursor()
    
    try:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        cursor.execute(
            "INSERT INTO access_log (rfid, status, timestamp) VALUES (%s, %s, %s)",
            (rfid_tag, status, timestamp)
        )
        conn.commit()
    except Exception as e:
        print(f"Failed to log access: {e}")
    finally:
        cursor.close()
        conn.close()


# ============= HELPER FUNCTIONS =============
def generate_otp(length: int = 6) -> str:
    """Generate random OTP of specified length"""
    return ''.join(str(random.randint(0, 9)) for _ in range(length))


def get_formatted_time() -> str:
    """Get current time formatted as HH:MM:SS"""
    local_time = datetime.now(timezone(timedelta(hours=7)))
    return local_time.strftime("%H:%M:%S")


def create_unlock_response(user: str, pin: str) -> JSONResponse:
    """Create unlock response JSON"""
    return JSONResponse(content={
        "status": "unlock",
        "user": user,
        "pin": pin,
        "timestamp": get_formatted_time()
    })


def create_lock_response() -> JSONResponse:
    """Create lock response JSON"""
    return JSONResponse(content={
        "status": "lock",
        "timestamp": get_formatted_time()
    })


# ============= OTP GENERATION =============
async def otp_generator():
    """Background task that generates OTP every 2 minutes"""
    while True:
        otp = generate_otp()
        print(f"Generated OTP: {otp}")
        
        try:
            store_otp_in_mysql(otp)
        except Exception as e:
            print(f"Failed to store OTP: {e}")
        
        await asyncio.sleep(120)


# ============= LIFESPAN HANDLER =============
@asynccontextmanager
async def lifespan(app: FastAPI):
    """Manage application lifespan"""
    # Startup
    asyncio.create_task(otp_generator())
    yield
    # Shutdown - cleanup if needed


# ============= API APPLICATION =============
app = FastAPI(
    title="Door IoT RFID API",
    description="Backend API for RFID door access control system",
    version="1.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# ============= API ROUTES =============
@app.post("/rfid")
async def process_rfid(rfid_tag: str = Form(...)):
    """
    Process RFID tag authentication
    
    - **rfid_tag**: RFID card UID
    """
    print(f"RFID tag: {rfid_tag}")
    
    is_valid, user, pin = check_rfid_in_mysql(rfid_tag)
    
    if is_valid:
        # Type narrowing - is_valid guarantees user and pin are not None
        assert user is not None and pin is not None
        print(f"Access granted - User: {user}, PIN: {pin}")
        log_access(rfid_tag, "unlock")
        return create_unlock_response(user, pin)
    else:
        print("Access denied")
        log_access(rfid_tag, "lock")
        return create_lock_response()


@app.post("/otp")
async def trigger_otp(background_tasks: BackgroundTasks):
    """Manually trigger OTP generation (optional endpoint)"""
    background_tasks.add_task(otp_generator)
    return {"message": "OTP generation task started"}


@app.post("/check_code")
async def check_code(code: str = Form(...)):
    """
    Verify OTP code
    
    - **code**: 6-digit OTP code
    """
    print(f"Received code: {code}")
    
    latest_otp = get_latest_otp()
    
    if latest_otp is None:
        print("No OTP in database")
        return {
            "status": "failure",
            "message": "lock",
            "last_otp": None,
            "time": get_formatted_time()
        }
    
    print(f"Latest OTP: {latest_otp}")
    
    if code == latest_otp:
        print("OTP verified - Unlocking")
        return {
            "status": "success",
            "message": "unlock",
            "last_otp": latest_otp,
            "time": get_formatted_time()
        }
    else:
        print("Invalid OTP - Locking")
        return {
            "status": "failure",
            "message": "lock",
            "last_otp": latest_otp,
            "time": get_formatted_time()
        }


@app.get("/update_data/{value}")
async def update_door_state(value: int):
    """
    Update door state from web interface
    
    - **value**: 1 = unlock, 0 = lock
    """
    if value not in (0, 1):
        raise HTTPException(status_code=400, detail="Value must be 0 or 1")
    
    print(f"Web command: {'Unlock' if value == 1 else 'Lock'}")
    door_state.update(value)
    
    return {"message": "Door state updated successfully"}


@app.get("/get_otp_api")
async def get_door_status():
    """Get current door status"""
    stored_value = door_state.get_value()
    return {"stored_value": stored_value}


# ============= HEALTH CHECK =============
@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {"status": "healthy"}


# ============= MODELS (for potential future use) =============
class RFIDRequest(BaseModel):
    rfid_tag: str


class OTPRequest(BaseModel):
    code: str


class DoorStatusResponse(BaseModel):
    stored_value: Optional[int]
