#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <Preferences.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <esp_task_wdt.h>
#include <esp_arduino_version.h>
const char* WIFI_DEFAULT_SSID = "NGUYEN HIEP";
const char* WIFI_DEFAULT_PASSWORD = "07012012";
String WIFI_SSID = WIFI_DEFAULT_SSID;
String WIFI_PASSWORD = WIFI_DEFAULT_PASSWORD;
const char* WIFI_SETUP_AP = "SmartDoor_Direct";
const char* WIFI_SETUP_PASSWORD = "12345678";
const char* MDNS_HOSTNAME = "smartdoor";
bool wifiConfigPortalActive = false;
uint8_t previousAPClientCount = 0;
const unsigned long APP_CONNECTION_TIMEOUT = 5000;
unsigned long lastAppContactAt = 0;
bool appConnected = false;
bool mdnsOK = false;
#define LCD_SDA 21
#define LCD_SCL 22
#define ESP32_FINGER_RX 16
#define ESP32_FINGER_TX 17
#define AS608_BAUD 57600
#define RFID_SS_PIN    5
#define RFID_RST_PIN   4
#define RFID_SCK_PIN   18
#define RFID_MISO_PIN  19
#define RFID_MOSI_PIN  23
#define KEYPAD_R1 13
#define KEYPAD_R2 12
#define KEYPAD_R3 14
#define KEYPAD_R4 25
#define KEYPAD_C1 33
#define KEYPAD_C2 32
#define KEYPAD_C3 35
#define RELAY_LOCK_PIN 27
#define BUZZER_PIN 26
#define DOOR_LOCK_LEVEL HIGH
#define DOOR_UNLOCK_LEVEL LOW
#define BUZZER_ON_LEVEL HIGH
#define BUZZER_OFF_LEVEL LOW
const String DEFAULT_USER_PASSWORD = "123456";
String userPassword = DEFAULT_USER_PASSWORD;
const String DEFAULT_ADMIN_PASSWORD = "654321";
String adminPassword = DEFAULT_ADMIN_PASSWORD;
const byte MIN_PASSWORD_LENGTH = 4;
const byte MAX_PASSWORD_LENGTH = 12;
const unsigned long DOOR_OPEN_TIME = 5000;
const unsigned long DOOR_WARNING_TIME = 3000;
const unsigned long LOCKOUT_TIME = 10000;
const unsigned long BUZZER_TIME = 10000;
const unsigned long RESULT_TIME = 2000;
const unsigned long HOLD_FINGER_TIME = 1000;
const unsigned long FINGER_HOLD_LOST_TIMEOUT = 300;
const unsigned long FINGER_REMOVE_WAIT_TIMEOUT = 5000;
const unsigned long FINGER_SCAN_INTERVAL = 40;
const unsigned long KEY_DEBOUNCE_TIME = 60;
const unsigned long PASSWORD_INPUT_TIMEOUT = 10000;
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;
const unsigned long SENSOR_RETRY_INTERVAL = 10000;
const uint32_t WATCHDOG_TIMEOUT_SECONDS = 15;
const byte REQUIRED_EMPTY_SENSOR_COUNT = 5;
const byte REQUIRED_FINGER_DETECTED_COUNT = 1;
const byte REQUIRED_NO_FINGER_COUNT = 2;
const uint16_t MIN_FINGER_ID = 1;
const uint16_t MAX_FINGER_ID = 127;
const unsigned long ENROLL_FINGER_TIMEOUT = 60000;
const unsigned long REMOVE_FINGER_TIMEOUT = 20000;
const unsigned long WIFI_LOST_DISPLAY_TIME = 2000;
const unsigned long RECONNECT_IP_DISPLAY_TIME = 2000;
const byte MAX_FAILED_ATTEMPTS = 5;
byte failedAttempts = 0;
unsigned long lockoutStartedAt = 0;
const String DEFAULT_RFID_UID = "26301D06";
const byte MAX_RFID_CARDS = 10;
String allowedRfidCards[MAX_RFID_CARDS];
byte allowedRfidCount = 0;
bool rfidCardLatched = false;
byte rfidMissingCount = 0;
unsigned long rfidLastPresenceProbe = 0;
const unsigned long RFID_PRESENCE_PROBE_INTERVAL = 200;
const byte RFID_MISSING_REQUIRED = 3;
hd44780_I2Cexp lcd;
HardwareSerial fingerSerial(2);
Adafruit_Fingerprint finger(&fingerSerial);
MFRC522 rfid( RFID_SS_PIN, RFID_RST_PIN );
WebServer server(80);
Preferences prefs;
enum SystemState {
  STATE_BOOT, STATE_WAITING, STATE_USER_PASSWORD, STATE_FINGER_HOLDING, STATE_FINGER_WAIT_REMOVE, STATE_FINGER_VERIFY, STATE_RFID_VERIFY, STATE_SHOW_RESULT, STATE_DOOR_OPEN, STATE_APP_ENROLLING, STATE_APP_DELETING, STATE_LOCAL_ADMIN, STATE_LOCKOUT
}
;
SystemState systemState = STATE_BOOT;
enum NetworkDisplayState {
  NET_DISPLAY_NONE, NET_DISPLAY_APP_LOST, NET_DISPLAY_RECONNECTED_IP
}
;
NetworkDisplayState networkDisplayState = NET_DISPLAY_NONE;
bool lcdOK = false;
bool fingerOK = false;
bool rfidOK = false;
bool buzzerRunning = false;
bool fingerReady = false;
bool fingerTemplateReady = false;
bool webServerStarted = false;
bool watchdogEnabled = false;
bool wasWiFiConnected = false;
String inputBuffer = "";
String currentDoorMethod = "";
unsigned long stateStartedAt = 0;
unsigned long doorOpenedAt = 0;
bool doorWarningShown = false;
unsigned long buzzerStartedAt = 0;
unsigned long lastFingerScanAt = 0;
unsigned long fingerHoldLostAt = 0;
const unsigned long FINGER_STABILIZE_TIME = 2000;
unsigned long fingerIgnoreUntil = 0;
unsigned long lastWiFiReconnectAt = 0;
unsigned long lastSensorRetryAt = 0;
unsigned long networkDisplayStartedAt = 0;
byte emptySensorCount = 0;
byte fingerDetectedCount = 0;
byte noFingerCount = 0;
unsigned long successfulOpenCount = 0;
unsigned long apiRequestCount = 0;
int cachedFingerprintCount = 0;
#define ENROLL_RESULT_NONE          0
#define ENROLL_RESULT_SUCCESS       1
#define ENROLL_RESULT_ID_EXISTS     2
#define ENROLL_RESULT_DUPLICATE     3
#define ENROLL_RESULT_SENSOR_ERROR  4
#define ENROLL_RESULT_TIMEOUT       5
#define ENROLL_RESULT_MISMATCH      6
#define ENROLL_RESULT_STORE_ERROR   7
byte lastEnrollResult = ENROLL_RESULT_NONE;
uint16_t duplicateFingerprintId = 0;
const byte ROWS = 4;
const byte COLS = 3;
const int rowPins[ROWS] = {
  KEYPAD_R1, KEYPAD_R2, KEYPAD_R3, KEYPAD_R4
}
;
const int colPins[COLS] = {
  KEYPAD_C1, KEYPAD_C2, KEYPAD_C3
}
;
const char keyMap[ROWS][COLS] = {
  {
    '1', '2', '3'
  }
  , {
    '4', '5', '6'
  }
  , {
    '7', '8', '9'
  }
  , {
    '*', '0', '#'
  }
}
;
char previousRawKey = 0;
bool keyReported = false;
unsigned long keyChangedAt = 0;
unsigned long lastKeyInputAt = 0;
struct DoorHistoryItem {
  String time;
  String method;
  String detail;
}
;
const byte MAX_HISTORY = 5;
DoorHistoryItem doorHistory[MAX_HISTORY];
byte doorHistoryCount = 0;
int findAllowedRfidIndex( const String& uid ) {
  for ( byte i = 0; i < allowedRfidCount; i++ ) {
    if ( allowedRfidCards[i] == uid ) {
      return i;
    }
  }
  return -1;
}
bool isRFIDAllowed( const String& uid ) {
  return findAllowedRfidIndex( uid ) >= 0;
}
void saveRfidCardsToFlash() {
  prefs.putUChar( "rfid_count", allowedRfidCount );
  for ( byte i = 0; i < MAX_RFID_CARDS; i++ ) {
    String key = "rfid" + String(i);
    if ( i < allowedRfidCount ) {
      prefs.putString( key.c_str(), allowedRfidCards[i] );
    } else {
      prefs.remove( key.c_str() );
    }
  }
}
bool addAllowedRfid( const String& uid ) {
  if ( uid.length() == 0 ) {
    return false;
  }
  if ( isRFIDAllowed(uid) ) {
    return false;
  }
  if ( allowedRfidCount >= MAX_RFID_CARDS ) {
    return false;
  }
  allowedRfidCards[allowedRfidCount] = uid;
  allowedRfidCount++;
  saveRfidCardsToFlash();
  return true;
}
bool deleteAllowedRfid( const String& uid ) {
  int index = findAllowedRfidIndex( uid );
  if ( index < 0 ) {
    return false;
  }
  for ( byte i = index; i + 1 < allowedRfidCount; i++ ) {
    allowedRfidCards[i] = allowedRfidCards[i + 1];
  }
  if ( allowedRfidCount > 0 ) {
    allowedRfidCount--;
  }
  if ( allowedRfidCount < MAX_RFID_CARDS ) {
    allowedRfidCards[allowedRfidCount] = "";
  }
  saveRfidCardsToFlash();
  return true;
}
void saveUserPasswordToFlash() {
  prefs.putString( "user_pass", userPassword );
}
void saveAdminPasswordToFlash() {
  prefs.putString( "admin_pass", adminPassword );
}
void loadSecurityConfig() {
  bool initialized =
    prefs.getBool( "sec_init", false );
  if ( !initialized ) {
    userPassword = DEFAULT_USER_PASSWORD;
    adminPassword = DEFAULT_ADMIN_PASSWORD;
    allowedRfidCount = 1;
    allowedRfidCards[0] = DEFAULT_RFID_UID;
    saveUserPasswordToFlash();
    saveAdminPasswordToFlash();
    saveRfidCardsToFlash();
    prefs.putBool( "sec_init", true );
    Serial.println( "KHOI TAO CAU HINH BAO MAT MAC DINH" );
    return;
  }
  userPassword =
    prefs.getString(
      "user_pass",
      DEFAULT_USER_PASSWORD
    );
  adminPassword =
    prefs.getString(
      "admin_pass",
      DEFAULT_ADMIN_PASSWORD
    );
  if (
    userPassword.length() < MIN_PASSWORD_LENGTH
    ||
    userPassword.length() > MAX_PASSWORD_LENGTH
  ) {
    userPassword = DEFAULT_USER_PASSWORD;
    saveUserPasswordToFlash();
  }
  if (
    adminPassword.length() < MIN_PASSWORD_LENGTH
    ||
    adminPassword.length() > MAX_PASSWORD_LENGTH
  ) {
    adminPassword = DEFAULT_ADMIN_PASSWORD;
    saveAdminPasswordToFlash();
  }
  allowedRfidCount =
    prefs.getUChar(
      "rfid_count",
      0
    );
  if ( allowedRfidCount > MAX_RFID_CARDS ) {
    allowedRfidCount = MAX_RFID_CARDS;
  }
  for ( byte i = 0; i < allowedRfidCount; i++ ) {
    String key = "rfid" + String(i);
    allowedRfidCards[i] =
      prefs.getString(
        key.c_str(),
        ""
      );
  }
  Serial.print( "MAT KHAU DA NAP: " );
  Serial.println( "******" );
  Serial.print( "SO THE RFID DUOC PHEP: " );
  Serial.println( allowedRfidCount );
}
void feedWatchdog() { if (watchdogEnabled) esp_task_wdt_reset(); }
void setupWatchdog() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t config = {
  }
  ;
  config.timeout_ms = WATCHDOG_TIMEOUT_SECONDS * 1000;
  config.idle_core_mask = (1 << portNUM_PROCESSORS) - 1;
  config.trigger_panic = true;
  esp_err_t result = esp_task_wdt_init(&config);
  if ( result != ESP_OK && result != ESP_ERR_INVALID_STATE ) {
    Serial.print( "WATCHDOG INIT LOI: " );
    Serial.println(result);
    watchdogEnabled = false;
    return;
  }
#else
  esp_err_t result = esp_task_wdt_init( WATCHDOG_TIMEOUT_SECONDS, true );
  if ( result != ESP_OK && result != ESP_ERR_INVALID_STATE ) {
    watchdogEnabled = false;
    return;
  }
#endif
  esp_err_t addResult = esp_task_wdt_add(NULL);
  if ( addResult == ESP_OK || addResult == ESP_ERR_INVALID_STATE ) {
    watchdogEnabled = true;
    Serial.println( "WATCHDOG DA BAT" );
  }
}
void showLCD( const String& line1, const String& line2 = "" ) {
  if (!lcdOK) {
    return;
  }
  lcd.clear();
  lcd.setCursor( 0, 0 );
  lcd.print( line1.substring( 0, 16 ) );
  lcd.setCursor( 0, 1 );
  lcd.print( line2.substring( 0, 16 ) );
}
void resetFingerprintWaiting() {
  fingerReady = false;
  fingerTemplateReady = false;
  emptySensorCount = 0;
  fingerDetectedCount = 0;
  noFingerCount = 0;
  fingerHoldLostAt = 0;
  lastFingerScanAt = 0;
}
void stabilizeFingerprint() {
  resetFingerprintWaiting();
  fingerIgnoreUntil = millis() + FINGER_STABILIZE_TIME;
  Serial.println( "AS608 DANG ON DINH 2 GIAY" );
}
String getSystemStateName() {
  switch ( systemState ) {
    case STATE_BOOT:
    return "BOOT";
    case STATE_WAITING:
    return "READY";
    case STATE_USER_PASSWORD:
    return "PASSWORD";
    case STATE_FINGER_HOLDING:
    return "FINGER_HOLDING";
    case STATE_FINGER_WAIT_REMOVE:
    return "FINGER_REMOVE";
    case STATE_FINGER_VERIFY:
    return "FINGER_VERIFY";
    case STATE_RFID_VERIFY:
    return "RFID_VERIFY";
    case STATE_SHOW_RESULT:
    return "SHOW_RESULT";
    case STATE_DOOR_OPEN:
    return "DOOR_OPEN";
    case STATE_APP_ENROLLING:
    return "ENROLLING";
    case STATE_APP_DELETING:
    return "DELETING";
    case STATE_LOCAL_ADMIN:
    return "LOCAL_ADMIN";
    case STATE_LOCKOUT:
    return "LOCKOUT";
  }
  return "UNKNOWN";
}
bool isSystemBusy() { return systemState != STATE_WAITING; }
void updateLCDByState() {
  switch ( systemState ) {
    case STATE_BOOT:
    showLCD( "DANG KHOI DONG", "SMART DOOR..." );
    break;
    case STATE_WAITING: {
    if ( appConnected ) {
      if ( fingerOK && rfidOK ) {
        showLCD( "CUA DANG KHOA", "VT/THE/MK + APP" );
      } else if ( fingerOK ) {
        showLCD( "CUA DANG KHOA", "VT/MK/APP RFID!" );
      } else if ( rfidOK ) {
        showLCD( "CUA DANG KHOA", "THE/MK/APP VT!" );
      } else {
        showLCD( "CUA DANG KHOA", "MK + APP" );
      }
    } else {
      if ( fingerOK && rfidOK ) {
        showLCD( "CUA DANG KHOA", "VT/THE/MK" );
      } else if ( fingerOK ) {
        showLCD( "CUA DANG KHOA", "VT/MK" );
      } else if ( rfidOK ) {
        showLCD( "CUA DANG KHOA", "THE/MK" );
      } else {
        showLCD( "CUA DANG KHOA", "CHI MAT KHAU" );
      }
    }
    break;
    }
    case STATE_FINGER_HOLDING:
    showLCD( "DANG XAC MINH", "GIU TAY 1 GIAY" );
    break;
    case STATE_FINGER_WAIT_REMOVE:
    showLCD( "DA DOC VAN TAY", "NHAC NGON TAY" );
    break;
    case STATE_FINGER_VERIFY:
    break;
    case STATE_RFID_VERIFY:
    break;
    case STATE_DOOR_OPEN:
    if ( currentDoorMethod == "VAN TAY" ) {
      showLCD( "VAN TAY HOP LE", "XIN MOI VAO!" );
    } else if ( currentDoorMethod == "THE RFID" ) {
      showLCD( "THE HOP LE", "XIN MOI VAO!" );
    } else if ( currentDoorMethod == "MAT KHAU" ) {
      showLCD( "MAT KHAU DUNG", "XIN MOI VAO!" );
    } else if ( currentDoorMethod == "APP" ) {
      showLCD( "MO BANG APP", "XIN MOI VAO!" );
    } else {
      showLCD( "CUA DANG MO", "XIN MOI VAO!" );
    }
    break;
    case STATE_LOCAL_ADMIN:
    showLCD( "CHE DO QUAN TRI", "TREN PHAN CUNG" );
    break;
    case STATE_LOCKOUT:
    showLCD( "CANH BAO!", "KHOA HE THONG!!!" );
    break;
    default:
    break;
  }
}
void showWaitingScreen() {
  systemState = STATE_WAITING;
  inputBuffer = "";
  lastKeyInputAt = 0;
  currentDoorMethod = "";
  resetFingerprintWaiting();
  updateLCDByState();
}
void showResult( const String& line1, const String& line2 = "" ) {
  showLCD( line1, line2 );
  systemState = STATE_SHOW_RESULT;
  stateStartedAt = millis();
}
void updateResultScreen() { if (systemState == STATE_SHOW_RESULT && millis()-stateStartedAt >= RESULT_TIME) showWaitingScreen(); }
void showHiddenInput( const String& title ) {
  if (!lcdOK) {
    return;
  }
  lcd.clear();
  lcd.setCursor( 0, 0 );
  lcd.print( title.substring( 0, 16 ) );
  lcd.setCursor( 0, 1 );
  for ( unsigned int i = 0; i < inputBuffer.length(); i++ ) {
    lcd.print('*');
  }
}
void startBuzzer() {
  digitalWrite( BUZZER_PIN, BUZZER_ON_LEVEL );
  buzzerRunning = true;
  buzzerStartedAt = millis();
  Serial.println( "COI CANH BAO BAT" );
}
void stopBuzzer() {
  digitalWrite( BUZZER_PIN, BUZZER_OFF_LEVEL );
  buzzerRunning = false;
  Serial.println( "COI TAT" );
}
void updateBuzzer() { if (buzzerRunning && millis()-buzzerStartedAt >= BUZZER_TIME) stopBuzzer(); }
void beepSuccess() {
  digitalWrite( BUZZER_PIN, BUZZER_ON_LEVEL );
  delay(120);
  digitalWrite( BUZZER_PIN, BUZZER_OFF_LEVEL );
}
void beepErrorShort() {
  for ( byte i = 0; i < 2; i++ ) {
    digitalWrite( BUZZER_PIN, BUZZER_ON_LEVEL );
    delay(120);
    digitalWrite( BUZZER_PIN, BUZZER_OFF_LEVEL );
    delay(100);
  }
}
void startLockout() {
  systemState = STATE_LOCKOUT;
  lockoutStartedAt = millis();
  inputBuffer = "";
  resetFingerprintWaiting();
  networkDisplayState = NET_DISPLAY_NONE;
  updateLCDByState();
  startBuzzer();
  Serial.println();
  Serial.println( "==============================" );
  Serial.println( "SAI DU 5 LAN" );
  Serial.println( "KHOA HE THONG 5 GIAY" );
  Serial.println( "==============================" );
}
void updateLockout() { if (systemState==STATE_LOCKOUT && millis()-lockoutStartedAt>=LOCKOUT_TIME) { stopBuzzer(); failedAttempts=0; Serial.println("HET LOCKOUT"); showWaitingScreen(); } }
void addDoorHistory( const String& method, const String& detail );
void authenticationFailed( const String& reason ) {
  failedAttempts++;
  addDoorHistory("XAC THUC SAI", reason);
  Serial.print( "XAC THUC SAI: " );
  Serial.print( failedAttempts );
  Serial.print("/");
  Serial.println( MAX_FAILED_ATTEMPTS );
  if ( failedAttempts >= MAX_FAILED_ATTEMPTS ) {
    startLockout();
    return;
  }
  beepErrorShort();
  String line1 = "SAI LAN " + String(failedAttempts) + "/" + String(MAX_FAILED_ATTEMPTS);
  showResult( line1, reason );
}
void hardwareError( const String& line1, const String& line2 ) {
  beepErrorShort();
  showResult( line1, line2 );
}
String getCurrentTimeString() {
  time_t now = time(nullptr);
  if ( now < 100000 ) {
    return "Chua dong bo";
  }
  struct tm timeInfo;
  localtime_r( &now, &timeInfo );
  char buffer[25];
  strftime( buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeInfo );
  return String(buffer);
}
const char HISTORY_FIELD_SEPARATOR = 31;
const char HISTORY_RECORD_SEPARATOR = 30;
String oldHistoryKey( byte index, const String& suffix ) {
  return "h" + String(index) + "_" + suffix;
}
void saveHistoryToFlash() {
  String blob = "";
  for ( byte i = 0; i < doorHistoryCount; i++ ) {
    if ( i > 0 ) {
      blob += HISTORY_RECORD_SEPARATOR;
    }
    blob += doorHistory[i].time;
    blob += HISTORY_FIELD_SEPARATOR;
    blob += doorHistory[i].method;
    blob += HISTORY_FIELD_SEPARATOR;
    blob += doorHistory[i].detail;
  }
  prefs.putString( "history_blob", blob );
  Serial.println( "HISTORY DA LUU FLASH" );
}
void migrateOldHistory() {
  byte oldCount = prefs.getUChar( "hist_count", 0 );
  if ( oldCount == 0 ) {
    return;
  }
  if ( oldCount > MAX_HISTORY ) {
    oldCount = MAX_HISTORY;
  }
  doorHistoryCount = oldCount;
  for ( byte i = 0; i < oldCount; i++ ) {
    doorHistory[i].time = prefs.getString( oldHistoryKey( i, "time" ).c_str(), "" );
    doorHistory[i].method = prefs.getString( oldHistoryKey( i, "method" ).c_str(), "" );
    doorHistory[i].detail = prefs.getString( oldHistoryKey( i, "detail" ).c_str(), "" );
  }
  saveHistoryToFlash();
  Serial.println( "DA MIGRATE HISTORY CU" );
}
void loadHistoryFromFlash() {
  doorHistoryCount = 0;
  String blob = prefs.getString( "history_blob", "" );
  if ( blob.length() == 0 ) {
    migrateOldHistory();
    blob = prefs.getString( "history_blob", "" );
  }
  if ( blob.length() == 0 ) {
    Serial.println( "CHUA CO HISTORY" );
    return;
  }
  int startRecord = 0;
  while ( startRecord <= blob.length() && doorHistoryCount < MAX_HISTORY ) {
    int endRecord = blob.indexOf( HISTORY_RECORD_SEPARATOR, startRecord );
    if ( endRecord < 0 ) {
      endRecord = blob.length();
    }
    String record = blob.substring( startRecord, endRecord );
    int sep1 = record.indexOf( HISTORY_FIELD_SEPARATOR );
    int sep2 = -1;
    if ( sep1 >= 0 ) {
      sep2 = record.indexOf( HISTORY_FIELD_SEPARATOR, sep1 + 1 );
    }
    if ( sep1 >= 0 && sep2 >= 0 ) {
      doorHistory[ doorHistoryCount ].time = record.substring( 0, sep1 );
      doorHistory[ doorHistoryCount ].method = record.substring( sep1 + 1, sep2 );
      doorHistory[ doorHistoryCount ].detail = record.substring( sep2 + 1 );
      doorHistoryCount++;
    }
    if ( endRecord >= blob.length() ) {
      break;
    }
    startRecord = endRecord + 1;
  }
  Serial.print( "HISTORY LOAD: " );
  Serial.println( doorHistoryCount );
}
void addDoorHistory( const String& method, const String& detail ) {
  int lastIndex;
  if ( doorHistoryCount < MAX_HISTORY ) {
    lastIndex = doorHistoryCount;
    doorHistoryCount++;
  } else {
    lastIndex = MAX_HISTORY - 1;
  }
  for ( int i = lastIndex; i > 0; i-- ) {
    doorHistory[i] = doorHistory[i - 1];
  }
  doorHistory[0].time = getCurrentTimeString();
  doorHistory[0].method = method;
  doorHistory[0].detail = detail;
  Serial.print( "LICH SU: " );
  Serial.print( method );
  Serial.print( " - " );
  Serial.println( detail );
  saveHistoryToFlash();
}
void resetRFIDForNextScan() {
  if (!rfidOK) {
    return;
  }
  rfid.PCD_StopCrypto1();
  rfid.PCD_AntennaOff();
  delay(60);
  rfid.PCD_Reset();
  delay(60);
  rfid.PCD_Init();
  delay(100);
  byte version = rfid.PCD_ReadRegister( MFRC522::VersionReg );
  if ( version == 0x00 || version == 0xFF ) {
    rfidOK = false;
    Serial.println( "RC522 MAT KET NOI" );
    return;
  }
  rfid.PCD_AntennaOn();
}
void lockDoor() {
  digitalWrite( RELAY_LOCK_PIN, DOOR_LOCK_LEVEL );
  Serial.println( "CUA DA KHOA" );
  resetFingerprintWaiting();
  resetRFIDForNextScan();
  if ( systemState == STATE_LOCKOUT ) {
    return;
  }
  showWaitingScreen();
}
void unlockDoor( const String& method, const String& detail = "" ) {
  failedAttempts = 0;
  networkDisplayState = NET_DISPLAY_NONE;
  digitalWrite( RELAY_LOCK_PIN, DOOR_UNLOCK_LEVEL );
  stopBuzzer();
  beepSuccess();
  currentDoorMethod = method;
  systemState = STATE_DOOR_OPEN;
  doorOpenedAt = millis();
  doorWarningShown = false;
  inputBuffer = "";
  successfulOpenCount++;
  updateLCDByState();
  addDoorHistory( method, detail );
  Serial.print( "MO CUA BANG: " );
  Serial.println( method );
}
void updateDoor() {
  if ( systemState != STATE_DOOR_OPEN ) {
    return;
  }
  unsigned long elapsed = millis() - doorOpenedAt;
  if ( !doorWarningShown && elapsed >= DOOR_WARNING_TIME ) {
    doorWarningShown = true;
    showLCD( "SAP KHOA CUA", "CON 2 GIAY..." );
    for ( byte i = 0; i < 2; i++ ) {
      digitalWrite( BUZZER_PIN, BUZZER_ON_LEVEL );
      delay(100);
      digitalWrite( BUZZER_PIN, BUZZER_OFF_LEVEL );
      delay(100);
    }
  }
  if ( elapsed >= DOOR_OPEN_TIME ) {
    lockDoor();
  }
}
void setupKeypad() {
  for ( byte i = 0; i < ROWS; i++ ) {
    pinMode( rowPins[i], OUTPUT );
    digitalWrite( rowPins[i], HIGH );
  }
  pinMode( KEYPAD_C1, INPUT_PULLUP );
  pinMode( KEYPAD_C2, INPUT_PULLUP );
  pinMode( KEYPAD_C3, INPUT );
  previousRawKey = 0;
  keyReported = false;
}
char scanKeypadRaw() {
  for ( byte row = 0; row < ROWS; row++ ) {
    for ( byte i = 0; i < ROWS; i++ ) {
      digitalWrite( rowPins[i], HIGH );
    }
    digitalWrite( rowPins[row], LOW );
    delayMicroseconds(30);
    for ( byte col = 0; col < COLS; col++ ) {
      if ( digitalRead( colPins[col] ) == LOW ) {
        digitalWrite( rowPins[row], HIGH );
        return keyMap[row][col];
      }
    }
    digitalWrite( rowPins[row], HIGH );
  }
  return 0;
}
char readKeypad() {
  char rawKey = scanKeypadRaw();
  if ( rawKey != previousRawKey ) {
    previousRawKey = rawKey;
    keyChangedAt = millis();
    keyReported = false;
  }
  if ( rawKey != 0 && !keyReported && millis() - keyChangedAt >= KEY_DEBOUNCE_TIME ) {
    keyReported = true;
    return rawKey;
  }
  if ( rawKey == 0 ) {
    keyReported = false;
  }
  return 0;
}
void runLocalAdminMenu();
void processKeypad() {
  if ( systemState == STATE_LOCKOUT || systemState == STATE_FINGER_HOLDING || systemState == STATE_FINGER_WAIT_REMOVE || systemState == STATE_FINGER_VERIFY || systemState == STATE_RFID_VERIFY || systemState == STATE_SHOW_RESULT || systemState == STATE_DOOR_OPEN || systemState == STATE_APP_ENROLLING || systemState == STATE_APP_DELETING ) {
    return;
  }
  if ( systemState == STATE_USER_PASSWORD && lastKeyInputAt != 0 && millis() - lastKeyInputAt >= PASSWORD_INPUT_TIMEOUT ) {
    Serial.println( "TIMEOUT NHAP MAT KHAU - VE READY" );
    inputBuffer = "";
    lastKeyInputAt = 0;
    showWaitingScreen();
    return;
  }
  char key = readKeypad();
  if ( key == 0 ) {
    return;
  }
  networkDisplayState = NET_DISPLAY_NONE;
  Serial.print( "KEYPAD: " );
  Serial.println( key );
  lastKeyInputAt = millis();
  if ( key == '*' ) {
    if (
      systemState == STATE_WAITING
      &&
      inputBuffer.length() == 0
    ) {
      runLocalAdminMenu();
      return;
    }
    if ( inputBuffer.length() == 0 ) {
      showWaitingScreen();
      return;
    }
    if ( inputBuffer.length() == 1 ) {
      inputBuffer = "";
      showWaitingScreen();
      return;
    }
    inputBuffer.remove( inputBuffer.length() - 1 );
    systemState = STATE_USER_PASSWORD;
    showHiddenInput( "NHAP MAT KHAU:" );
    return;
  }
  if ( key >= '0' && key <= '9' ) {
    if ( inputBuffer.length() < MAX_PASSWORD_LENGTH ) {
      inputBuffer += key;
    }
    systemState = STATE_USER_PASSWORD;
    showHiddenInput( "NHAP MAT KHAU:" );
    return;
  }
  if ( key == 'A' || key == 'B' || key == 'C' || key == 'D' ) {
    return;
  }
  if ( key != '#' ) {
    return;
  }
  if ( inputBuffer.length() == 0 ) {
    showResult( "CHUA NHAP MA", "VUI LONG NHAP" );
    return;
  }
  if ( inputBuffer == userPassword ) {
    inputBuffer = "";
    lastKeyInputAt = 0;
    unlockDoor( "MAT KHAU", "Mo bang keypad" );
  } else {
    inputBuffer = "";
    lastKeyInputAt = 0;
    authenticationFailed( "SAI MAT KHAU" );
  }
}
bool waitForFingerImage( const String& title, unsigned long timeoutMs ) {
  showLCD( title, "DAT NGON TAY" );
  unsigned long startedAt = millis();
  while ( millis() - startedAt < timeoutMs ) {
    feedWatchdog();
    uint8_t result = finger.getImage();
    if ( result == FINGERPRINT_OK ) {
      return true;
    }
    if ( result == FINGERPRINT_NOFINGER ) {
      delay(40);
      continue;
    }
    if ( result == FINGERPRINT_PACKETRECIEVEERR ) {
      delay(100);
      continue;
    }
    return false;
  }
  return false;
}
bool waitForFingerRemoval( unsigned long timeoutMs ) {
  showLCD( "NHAC NGON TAY", "CHO CAM BIEN" );
  unsigned long startedAt = millis();
  byte removedCount = 0;
  while ( millis() - startedAt < timeoutMs ) {
    feedWatchdog();
    uint8_t result = finger.getImage();
    if ( result == FINGERPRINT_NOFINGER ) {
      removedCount++;
      if ( removedCount >= 2 ) {
        delay(200);
        return true;
      }
    } else if ( result == FINGERPRINT_OK ) {
      removedCount = 0;
    } else if ( result == FINGERPRINT_PACKETRECIEVEERR ) {
      removedCount = 0;
    } else {
      return false;
    }
    delay(40);
  }
  return false;
}
bool fingerprintIdExists( uint16_t id ) {
  if (!fingerOK) {
    return false;
  }
  uint8_t result = finger.loadModel( id );
  return result == FINGERPRINT_OK;
}
bool currentTemplateAlreadyExists( uint8_t slot, uint16_t &existingId ) {
  uint8_t result = finger.fingerSearch( slot );
  Serial.print( "KIEM TRA TRUNG SLOT " );
  Serial.print( slot );
  Serial.print( " - MA: " );
  Serial.println( result );
  if ( result == FINGERPRINT_OK ) {
    existingId = finger.fingerID;
    Serial.print( "VAN TAY DA CO ID: " );
    Serial.println( existingId );
    Serial.print( "DO TIN CAY: " );
    Serial.println( finger.confidence );
    return true;
  }
  return false;
}
bool enrollFingerprint( uint16_t id ) {
  lastEnrollResult = ENROLL_RESULT_NONE;
  duplicateFingerprintId = 0;
  if (!fingerOK) {
    lastEnrollResult = ENROLL_RESULT_SENSOR_ERROR;
    return false;
  }
  if ( id < MIN_FINGER_ID || id > MAX_FINGER_ID ) {
    lastEnrollResult = ENROLL_RESULT_SENSOR_ERROR;
    showResult( "ID KHONG HOP LE", "CHI TU 1 DEN 127" );
    return false;
  }
  if ( fingerprintIdExists( id ) ) {
    lastEnrollResult = ENROLL_RESULT_ID_EXISTS;
    Serial.print( "ID DA TON TAI: " );
    Serial.println( id );
    showResult( "ID DA TON TAI", "CHON ID KHAC" );
    return false;
  }
  networkDisplayState = NET_DISPLAY_NONE;
  systemState = STATE_APP_ENROLLING;
  Serial.print( "BAT DAU DANG KY ID: " );
  Serial.println( id );
  if ( !waitForFingerImage( "DAT TAY LAN 1", ENROLL_FINGER_TIMEOUT ) ) {
    lastEnrollResult = ENROLL_RESULT_TIMEOUT;
    showResult( "LOI DOC LAN 1", "THU LAI" );
    return false;
  }
  uint8_t result = finger.image2Tz( 1 );
  if ( result != FINGERPRINT_OK ) {
    lastEnrollResult = ENROLL_RESULT_SENSOR_ERROR;
    showResult( "ANH LAN 1 LOI", "THU LAI" );
    return false;
  }
  uint16_t existingId = 0;
  if ( currentTemplateAlreadyExists( 1, existingId ) ) {
    duplicateFingerprintId = existingId;
    lastEnrollResult = ENROLL_RESULT_DUPLICATE;
    showResult( "VAN TAY DA CO", "ID:" + String(existingId) );
    return false;
  }
  if ( !waitForFingerRemoval( REMOVE_FINGER_TIMEOUT ) ) {
    lastEnrollResult = ENROLL_RESULT_TIMEOUT;
    showResult( "LOI NHAC TAY", "THU LAI" );
    return false;
  }
  if ( !waitForFingerImage( "DAT TAY LAN 2", ENROLL_FINGER_TIMEOUT ) ) {
    lastEnrollResult = ENROLL_RESULT_TIMEOUT;
    showResult( "LOI DOC LAN 2", "THU LAI" );
    return false;
  }
  result = finger.image2Tz( 2 );
  if ( result != FINGERPRINT_OK ) {
    lastEnrollResult = ENROLL_RESULT_SENSOR_ERROR;
    showResult( "ANH LAN 2 LOI", "THU LAI" );
    return false;
  }
  existingId = 0;
  if ( currentTemplateAlreadyExists( 2, existingId ) ) {
    duplicateFingerprintId = existingId;
    lastEnrollResult = ENROLL_RESULT_DUPLICATE;
    showResult( "VAN TAY DA CO", "ID:" + String(existingId) );
    return false;
  }
  result = finger.createModel();
  if ( result == FINGERPRINT_ENROLLMISMATCH ) {
    lastEnrollResult = ENROLL_RESULT_MISMATCH;
    showResult( "2 LAN KO GIONG", "THU LAI" );
    return false;
  }
  if ( result != FINGERPRINT_OK ) {
    lastEnrollResult = ENROLL_RESULT_SENSOR_ERROR;
    showResult( "TAO MAU THAT BAI", "THU LAI" );
    return false;
  }
  showLCD( "DANG LUU", "ID:" + String(id) );
  result = finger.storeModel( id );
  if ( result != FINGERPRINT_OK ) {
    lastEnrollResult = ENROLL_RESULT_STORE_ERROR;
    showResult( "LUU VT THAT BAI", "MA:" + String(result) );
    return false;
  }
  finger.getTemplateCount();
  cachedFingerprintCount = finger.templateCount;
  lastEnrollResult = ENROLL_RESULT_SUCCESS;
  Serial.print( "DANG KY THANH CONG ID: " );
  Serial.println( id );
  Serial.print( "TONG SO VAN TAY: " );
  Serial.println( cachedFingerprintCount );
  showResult( "DKY THANH CONG", "ID:" + String(id) );
  return true;
}
bool deleteFingerprint( uint16_t id ) {
  if (!fingerOK) {
    return false;
  }
  if ( id < MIN_FINGER_ID || id > MAX_FINGER_ID ) {
    showResult( "ID KHONG HOP LE", "CHI TU 1 DEN 127" );
    return false;
  }
  if ( !fingerprintIdExists( id ) ) {
    showResult( "ID KHONG TON TAI", "KHONG THE XOA" );
    return false;
  }
  networkDisplayState = NET_DISPLAY_NONE;
  systemState = STATE_APP_DELETING;
  showLCD( "DANG XOA VAN TAY", "ID:" + String(id) );
  uint8_t result = finger.deleteModel( id );
  if ( result == FINGERPRINT_OK ) {
    finger.getTemplateCount();
    cachedFingerprintCount = finger.templateCount;
    showResult( "DA XOA VAN TAY", "ID:" + String(id) );
    return true;
  }
  showResult( "XOA VAN TAY LOI", "MA:" + String(result) );
  return false;
}
int refreshFingerprintCount() {
  if (!fingerOK) {
    cachedFingerprintCount = 0;
    return -1;
  }
  uint8_t result = finger.getTemplateCount();
  if ( result != FINGERPRINT_OK ) {
    return -1;
  }
  cachedFingerprintCount = finger.templateCount;
  return cachedFingerprintCount;
}
void searchFingerprintAfterRemoval() {
  if ( !fingerTemplateReady ) {
    hardwareError( "KHONG CO MAU VT", "THU LAI" );
    return;
  }
  networkDisplayState = NET_DISPLAY_NONE;
  systemState = STATE_FINGER_VERIFY;
  updateLCDByState();
  uint8_t result = finger.fingerFastSearch();
  fingerTemplateReady = false;
  if ( result == FINGERPRINT_OK ) {
    uint16_t foundID = finger.fingerID;
    Serial.print( "VAN TAY DUNG ID: " );
    Serial.println( foundID );
    Serial.print( "DO TIN CAY: " );
    Serial.println( finger.confidence );
    unlockDoor( "VAN TAY", "ID " + String(foundID) );
  } else if ( result == FINGERPRINT_NOTFOUND ) {
    authenticationFailed( "VAN TAY SAI" );
  } else {
    hardwareError( "LOI CAM BIEN", "THU LAI" );
  }
}
void processFingerprint() {
  if (!fingerOK) {
    return;
  }
  if ( fingerIgnoreUntil != 0 ) {
    if ( (long)( millis() - fingerIgnoreUntil ) < 0 ) {
      return;
    }
    fingerIgnoreUntil = 0;
    resetFingerprintWaiting();
    Serial.println( "AS608 DA ON DINH - CHO QUET" );
    return;
  }
  if ( systemState != STATE_WAITING && systemState != STATE_FINGER_HOLDING && systemState != STATE_FINGER_WAIT_REMOVE ) {
    return;
  }
  if ( millis() - lastFingerScanAt < FINGER_SCAN_INTERVAL ) {
    return;
  }
  lastFingerScanAt = millis();
  uint8_t result = finger.getImage();
  if ( systemState == STATE_WAITING ) {
    if ( !fingerReady ) {
      if ( result == FINGERPRINT_NOFINGER ) {
        emptySensorCount++;
        if ( emptySensorCount >= REQUIRED_EMPTY_SENSOR_COUNT ) {
          fingerReady = true;
          emptySensorCount = 0;
          Serial.println( "AS608 READY - CHO DAT NGON TAY" );
        }
      } else {
        emptySensorCount = 0;
      }
      return;
    }
    if ( result == FINGERPRINT_NOFINGER ) {
      return;
    }
    if ( result != FINGERPRINT_OK ) {
      return;
    }
    fingerReady = false;
    fingerTemplateReady = false;
    fingerHoldLostAt = 0;
    noFingerCount = 0;
    networkDisplayState = NET_DISPLAY_NONE;
    systemState = STATE_FINGER_HOLDING;
    stateStartedAt = millis();
    updateLCDByState();
    Serial.println( "AS608 PHAT HIEN TAY - GIU 1 GIAY" );
    return;
  }
  if ( systemState == STATE_FINGER_HOLDING ) {
    if ( result == FINGERPRINT_OK ) {
      fingerHoldLostAt = 0;
    } else if ( result == FINGERPRINT_NOFINGER ) {
      if ( fingerHoldLostAt == 0 ) {
        fingerHoldLostAt = millis();
      }
      if ( millis() - fingerHoldLostAt >= FINGER_HOLD_LOST_TIMEOUT ) {
        Serial.println( "AS608 MAT TAY TRUOC 1S - HUY PHIEN QUET" );
        fingerTemplateReady = false;
        fingerHoldLostAt = 0;
        resetFingerprintWaiting();
        showWaitingScreen();
        return;
      }
    }
    if ( millis() - stateStartedAt < HOLD_FINGER_TIME ) {
      return;
    }
    if ( result != FINGERPRINT_OK ) {
      if ( fingerHoldLostAt != 0 && millis() - fingerHoldLostAt < FINGER_HOLD_LOST_TIMEOUT ) {
        return;
      }
      resetFingerprintWaiting();
      showWaitingScreen();
      return;
    }
    uint8_t templateResult = finger.image2Tz( 1 );
    if ( templateResult != FINGERPRINT_OK ) {
      Serial.print( "AS608 TAO TEMPLATE LOI - MA: " );
      Serial.println( templateResult );
      fingerTemplateReady = false;
      resetFingerprintWaiting();
      showResult( "VAN TAY CHUA RO", "THU LAI" );
      return;
    }
    fingerTemplateReady = true;
    fingerHoldLostAt = 0;
    noFingerCount = 0;
    systemState = STATE_FINGER_WAIT_REMOVE;
    stateStartedAt = millis();
    updateLCDByState();
    Serial.println( "AS608 DA QUET DU 1S - NHAC NGON TAY" );
    return;
  }
  if ( systemState == STATE_FINGER_WAIT_REMOVE ) {
    if ( millis() - stateStartedAt >= FINGER_REMOVE_WAIT_TIMEOUT ) {
      Serial.println( "AS608 TIMEOUT CHO NHAC TAY - CHO SENSOR TRONG" );
      fingerTemplateReady = false;
      noFingerCount = 0;
      resetFingerprintWaiting();
      showWaitingScreen();
      return;
    }
    if ( result == FINGERPRINT_NOFINGER ) {
      noFingerCount++;
      if ( noFingerCount >= REQUIRED_NO_FINGER_COUNT ) {
        noFingerCount = 0;
        searchFingerprintAfterRemoval();
      }
      return;
    }
    if ( result == FINGERPRINT_OK ) {
      noFingerCount = 0;
      return;
    }
    if ( result == FINGERPRINT_PACKETRECIEVEERR ) {
      noFingerCount = 0;
      return;
    }
    noFingerCount = 0;
  }
}
void setupFingerprint() {
  fingerOK = false;
  Serial.println( "KHOI TAO AS608..." );
  fingerSerial.end();
  delay(300);
  fingerSerial.begin( AS608_BAUD, SERIAL_8N1, ESP32_FINGER_RX, ESP32_FINGER_TX );
  finger.begin( AS608_BAUD );
  delay(1500);
  for ( byte i = 1; i <= 5; i++ ) {
    feedWatchdog();
    if ( finger.verifyPassword() ) {
      fingerOK = true;
      break;
    }
    delay(400);
  }
  if (!fingerOK) {
    Serial.println( "AS608 LOI" );
    return;
  }
  refreshFingerprintCount();
  Serial.println( "AS608 OK" );
  Serial.print( "SO MAU VAN TAY: " );
  Serial.println( cachedFingerprintCount );
  resetFingerprintWaiting();
}
String getRFIDUID() {
  String uid = "";
  for ( byte i = 0; i < rfid.uid.size; i++ ) {
    if ( rfid.uid.uidByte[i] < 0x10 ) {
      uid += "0";
    }
    uid += String( rfid.uid.uidByte[i], HEX );
  }
  uid.toUpperCase();
  return uid;
}
void setupRFID() {
  rfidOK = false;
  Serial.println( "KHOI TAO RC522..." );
  pinMode( RFID_SS_PIN, OUTPUT );
  digitalWrite( RFID_SS_PIN, HIGH );
  pinMode( RFID_RST_PIN, OUTPUT );
  digitalWrite( RFID_RST_PIN, LOW );
  delay(50);
  digitalWrite( RFID_RST_PIN, HIGH );
  delay(100);
  SPI.begin( RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN );
  delay(200);
  rfid.PCD_Init();
  delay(200);
  byte version = rfid.PCD_ReadRegister( MFRC522::VersionReg );
  Serial.print( "RC522 VERSION: 0x" );
  Serial.println( version, HEX );
  if ( version == 0x00 || version == 0xFF ) {
    Serial.println( "RC522 LOI / KHONG PHAN HOI" );
    rfidOK = false;
    return;
  }
  rfid.PCD_AntennaOn();
  rfidOK = true;
  Serial.println( "RC522 OK" );
}
bool probeRFIDCardPresence() {
  if (!rfidOK) return false;
  resetRFIDForNextScan();
  if (!rfidOK) return false;
  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial()) return false;
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}
void processRFID() {
  if (!rfidOK) return;
  if (rfidCardLatched) {
    if (millis() - rfidLastPresenceProbe < RFID_PRESENCE_PROBE_INTERVAL) return;
    rfidLastPresenceProbe = millis();
    if (probeRFIDCardPresence()) {
      rfidMissingCount = 0;
      return;
    }
    rfidMissingCount++;
    if (rfidMissingCount >= RFID_MISSING_REQUIRED) {
      rfidCardLatched = false;
      rfidMissingCount = 0;
      resetRFIDForNextScan();
    }
    return;
  }
  if (systemState != STATE_WAITING) return;
  if (!rfid.PICC_IsNewCardPresent()) return;
  networkDisplayState = NET_DISPLAY_NONE;
  systemState = STATE_RFID_VERIFY;
  updateLCDByState();
  delay(30);
  bool readOK = false;
  for (byte i = 0; i < 3; i++) {
    if (rfid.PICC_ReadCardSerial()) {
      readOK = true;
      break;
    }
    delay(40);
  }
  if (!readOK) {
    showWaitingScreen();
    return;
  }
  String uid = getRFIDUID();
  Serial.print("UID RFID: ");
  Serial.println(uid);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  rfidCardLatched = true;
  rfidMissingCount = 0;
  rfidLastPresenceProbe = millis();
  if ( isRFIDAllowed(uid) ) {
    unlockDoor("THE RFID", "UID " + uid);
  } else {
    authenticationFailed("THE RFID SAI");
  }
}
void serviceAdminBackground() {
  feedWatchdog();
  if (
    webServerStarted
    &&
    (
      WiFi.status() == WL_CONNECTED
      ||
      wifiConfigPortalActive
    )
  ) {
    server.handleClient();
  }
  delay(5);
}
char waitAdminKey( unsigned long timeoutMs ) {
  unsigned long startedAt = millis();
  while ( millis() - startedAt < timeoutMs ) {
    serviceAdminBackground();
    char key = readKeypad();
    if ( key != 0 ) {
      return key;
    }
  }
  return 0;
}
bool readAdminDigits(
  const String& title,
  String& value,
  byte minLen,
  byte maxLen,
  bool hidden,
  unsigned long timeoutMs
) {
  value = "";
  unsigned long startedAt = millis();
  while ( millis() - startedAt < timeoutMs ) {
    if ( hidden ) {
      if ( lcdOK ) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print( title.substring(0, 16) );
        lcd.setCursor(0, 1);
        for ( unsigned int i = 0; i < value.length(); i++ ) {
          lcd.print('*');
        }
      }
    } else {
      showLCD(
        title,
        value.length() > 0
          ? value + "  #=OK"
          : "#=OK *=HUY"
      );
    }
    char key = waitAdminKey( 250 );
    if ( key == 0 ) {
      continue;
    }
    startedAt = millis();
    if ( key >= '0' && key <= '9' ) {
      if ( value.length() < maxLen ) {
        value += key;
      }
      continue;
    }
    if ( key == '*' ) {
      if ( value.length() == 0 ) {
        return false;
      }
      value.remove(
        value.length() - 1
      );
      continue;
    }
    if ( key == '#' ) {
      if (
        value.length() >= minLen
        &&
        value.length() <= maxLen
      ) {
        return true;
      }
      beepErrorShort();
      showLCD(
        "DO DAI KHONG DUNG",
        "THU LAI"
      );
      delay(1200);
      continue;
    }
  }
  return false;
}
void waitAdminResult() {
  unsigned long startedAt = millis();
  while ( millis() - startedAt < 2200 ) {
    serviceAdminBackground();
  }
  systemState = STATE_LOCAL_ADMIN;
}
char getAdminMenuChoice() {
  byte page = 0;
  byte row = 0;
  auto drawMenu = [&]() {
    if ( !lcdOK ) {
      return;
    }
    lcd.clear();
    if ( page == 0 ) {
      lcd.setCursor(0, 0);
      lcd.print(
        row == 0
          ? ">1 THEM VAN TAY"
          : " 1 THEM VAN TAY"
      );
      lcd.setCursor(0, 1);
      lcd.print(
        row == 1
          ? ">2 XOA VAN TAY"
          : " 2 XOA VAN TAY"
      );
    }
    else if ( page == 1 ) {
      lcd.setCursor(0, 0);
      lcd.print(
        row == 0
          ? ">3 THEM THE RFID"
          : " 3 THEM THE RFID"
      );
      lcd.setCursor(0, 1);
      lcd.print(
        row == 1
          ? ">4 XOA THE RFID"
          : " 4 XOA THE RFID"
      );
    }
    else {
      lcd.setCursor(0, 0);
      lcd.print(
        row == 0
          ? ">5 DOI MK CUA"
          : " 5 DOI MK CUA"
      );
      lcd.setCursor(0, 1);
      lcd.print(
        row == 1
          ? ">6 DOI MK ADMIN"
          : " 6 DOI MK ADMIN"
      );
    }
  };
  drawMenu();
  while ( true ) {
    char key = waitAdminKey( 100 );
    if ( key == 0 ) {
      continue;
    }
    if ( key == '*' ) {
      return '0';
    }
    if ( key == '2' ) {
      if ( row == 1 ) {
        row = 0;
      } else {
        row = 1;
        if ( page == 0 ) {
          page = 2;
        } else {
          page--;
        }
      }
      drawMenu();
      continue;
    }
    if ( key == '8' ) {
      if ( row == 0 ) {
        row = 1;
      } else {
        row = 0;
        page++;
        if ( page > 2 ) {
          page = 0;
        }
      }
      drawMenu();
      continue;
    }
    if ( key == '#' ) {
      if ( page == 0 && row == 0 ) {
        return '1';
      }
      if ( page == 0 && row == 1 ) {
        return '2';
      }
      if ( page == 1 && row == 0 ) {
        return '3';
      }
      if ( page == 1 && row == 1 ) {
        return '4';
      }
      if ( page == 2 && row == 0 ) {
        return '5';
      }
      if ( page == 2 && row == 1 ) {
        return '6';
      }
    }
  }
}
bool scanRfidForAdmin(
  String& uid,
  unsigned long timeoutMs
) {
  uid = "";
  if ( !rfidOK ) {
    showLCD(
      "RC522 DANG LOI",
      "KHONG THE QUET"
    );
    delay(1800);
    return false;
  }
  rfidCardLatched = false;
  rfidMissingCount = 0;
  resetRFIDForNextScan();
  showLCD(
    "QUET THE RFID",
    "* = HUY"
  );
  unsigned long startedAt = millis();
  while ( millis() - startedAt < timeoutMs ) {
    serviceAdminBackground();
    char key = readKeypad();
    if ( key == '*' ) {
      return false;
    }
    if ( !rfid.PICC_IsNewCardPresent() ) {
      delay(20);
      continue;
    }
    bool readOK = false;
    for ( byte i = 0; i < 3; i++ ) {
      if ( rfid.PICC_ReadCardSerial() ) {
        readOK = true;
        break;
      }
      delay(40);
    }
    if ( !readOK ) {
      continue;
    }
    uid = getRFIDUID();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    Serial.print( "ADMIN RFID UID: " );
    Serial.println( uid );
    resetRFIDForNextScan();
    return uid.length() > 0;
  }
  showLCD(
    "HET TG QUET THE",
    "THU LAI"
  );
  delay(1500);
  return false;
}
void localAddFingerprint() {
  if ( !fingerOK ) {
    showLCD(
      "AS608 DANG LOI",
      "KHONG THE THEM"
    );
    delay(1800);
    return;
  }
  String idText;
  if (
    !readAdminDigits(
      "ID VT 1-127:",
      idText,
      1,
      3,
      false,
      20000
    )
  ) {
    return;
  }
  int id = idText.toInt();
  if (
    id < MIN_FINGER_ID
    ||
    id > MAX_FINGER_ID
  ) {
    showLCD(
      "ID KHONG HOP LE",
      "CHI 1 DEN 127"
    );
    delay(1800);
    return;
  }
  enrollFingerprint(
    (uint16_t)id
  );
  waitAdminResult();
}
void localDeleteFingerprint() {
  if ( !fingerOK ) {
    showLCD(
      "AS608 DANG LOI",
      "KHONG THE XOA"
    );
    delay(1800);
    return;
  }
  String idText;
  if (
    !readAdminDigits(
      "XOA ID VT:",
      idText,
      1,
      3,
      false,
      20000
    )
  ) {
    return;
  }
  int id = idText.toInt();
  if (
    id < MIN_FINGER_ID
    ||
    id > MAX_FINGER_ID
  ) {
    showLCD(
      "ID KHONG HOP LE",
      "CHI 1 DEN 127"
    );
    delay(1800);
    return;
  }
  deleteFingerprint(
    (uint16_t)id
  );
  waitAdminResult();
}
void localAddRfid() {
  if ( allowedRfidCount >= MAX_RFID_CARDS ) {
    showLCD(
      "DANH SACH DAY",
      "TOI DA 10 THE"
    );
    delay(1800);
    return;
  }
  String uid;
  if (
    !scanRfidForAdmin(
      uid,
      20000
    )
  ) {
    return;
  }
  if ( isRFIDAllowed(uid) ) {
    showLCD(
      "THE DA TON TAI",
      uid
    );
    beepErrorShort();
    delay(1800);
    return;
  }
  if ( addAllowedRfid(uid) ) {
    beepSuccess();
    showLCD(
      "DA THEM THE",
      "TONG:" + String(allowedRfidCount)
    );
    delay(1800);
  } else {
    showLCD(
      "THEM THE LOI",
      "THU LAI"
    );
    beepErrorShort();
    delay(1800);
  }
}
void localDeleteRfid() {
  if ( allowedRfidCount == 0 ) {
    showLCD(
      "KHONG CO THE",
      "DE XOA"
    );
    delay(1800);
    return;
  }
  String uid;
  if (
    !scanRfidForAdmin(
      uid,
      20000
    )
  ) {
    return;
  }
  if ( !isRFIDAllowed(uid) ) {
    showLCD(
      "THE KHONG CO",
      "TRONG DANH SACH"
    );
    beepErrorShort();
    delay(1800);
    return;
  }
  if ( deleteAllowedRfid(uid) ) {
    beepSuccess();
    showLCD(
      "DA XOA THE",
      "CON:" + String(allowedRfidCount)
    );
    delay(1800);
  } else {
    showLCD(
      "XOA THE LOI",
      "THU LAI"
    );
    beepErrorShort();
    delay(1800);
  }
}
void localChangePassword() {
  String newPass1;
  String newPass2;
  if (
    !readAdminDigits(
      "MK MOI 4-12 SO:",
      newPass1,
      MIN_PASSWORD_LENGTH,
      MAX_PASSWORD_LENGTH,
      true,
      20000
    )
  ) {
    return;
  }
  showLCD(
    "NHAP LAI MK",
    "DE XAC NHAN"
  );
  delay(700);
  if (
    !readAdminDigits(
      "NHAP LAI MK:",
      newPass2,
      MIN_PASSWORD_LENGTH,
      MAX_PASSWORD_LENGTH,
      true,
      20000
    )
  ) {
    return;
  }
  if ( newPass1 != newPass2 ) {
    beepErrorShort();
    showLCD(
      "2 MK KHONG KHOP",
      "KHONG DOI MK"
    );
    delay(1800);
    return;
  }
  userPassword = newPass1;
  saveUserPasswordToFlash();
  beepSuccess();
  showLCD(
    "DA DOI MAT KHAU",
    "THANH CONG"
  );
  delay(1800);
}
void localChangeAdminPassword() {
  String newAdmin1;
  String newAdmin2;
  if (
    !readAdminDigits(
      "ADMIN 4-12 SO:",
      newAdmin1,
      MIN_PASSWORD_LENGTH,
      MAX_PASSWORD_LENGTH,
      true,
      20000
    )
  ) {
    return;
  }
  showLCD(
    "NHAP LAI ADMIN",
    "DE XAC NHAN"
  );
  delay(700);
  if (
    !readAdminDigits(
      "NHAP LAI ADMIN",
      newAdmin2,
      MIN_PASSWORD_LENGTH,
      MAX_PASSWORD_LENGTH,
      true,
      20000
    )
  ) {
    return;
  }
  if ( newAdmin1 != newAdmin2 ) {
    beepErrorShort();
    showLCD(
      "2 MK KHONG KHOP",
      "KHONG DOI ADMIN"
    );
    delay(1800);
    return;
  }
  adminPassword = newAdmin1;
  saveAdminPasswordToFlash();
  beepSuccess();
  showLCD(
    "DA DOI MK ADMIN",
    "THANH CONG"
  );
  delay(1800);
}
void runLocalAdminMenu() {
  networkDisplayState =
    NET_DISPLAY_NONE;
  systemState =
    STATE_LOCAL_ADMIN;
  inputBuffer = "";
  lastKeyInputAt = 0;
  resetFingerprintWaiting();
  rfidCardLatched = false;
  rfidMissingCount = 0;
  showLCD(
    "CHE DO QUAN TRI",
    "NHAP MK + #"
  );
  unsigned long adminNoticeStartedAt = millis();
  while (
    millis() - adminNoticeStartedAt < 2000
  ) {
    serviceAdminBackground();
  }
  String adminPasswordInput;
  if (
    !readAdminDigits(
      "NHAP MK QUAN TRI",
      adminPasswordInput,
      MIN_PASSWORD_LENGTH,
      MAX_PASSWORD_LENGTH,
      true,
      20000
    )
  ) {
    showLCD(
      "HUY QUAN TRI",
      "VE MAN HINH CHO"
    );
    delay(1000);
    showWaitingScreen();
    return;
  }
  if (
    adminPasswordInput != adminPassword
  ) {
    beepErrorShort();
    showLCD(
      "SAI MAT KHAU",
      "TU CHOI QUAN TRI"
    );
    delay(1800);
    showWaitingScreen();
    return;
  }
  beepSuccess();
  showLCD(
    "DANG NHAP DUNG",
    "MENU QUAN TRI"
  );
  delay(1000);
  while ( true ) {
    systemState =
      STATE_LOCAL_ADMIN;
    char choice =
      getAdminMenuChoice();
    if ( choice == '0' ) {
      showLCD(
        "THOAT QUAN TRI",
        "VE MAN HINH CHO"
      );
      delay(900);
      break;
    }
    if ( choice == '1' ) {
      localAddFingerprint();
    }
    else if ( choice == '2' ) {
      localDeleteFingerprint();
    }
    else if ( choice == '3' ) {
      localAddRfid();
    }
    else if ( choice == '4' ) {
      localDeleteRfid();
    }
    else if ( choice == '5' ) {
      localChangePassword();
    }
    else if ( choice == '6' ) {
      localChangeAdminPassword();
    }
    systemState =
      STATE_LOCAL_ADMIN;
    inputBuffer = "";
    lastKeyInputAt = 0;
    resetFingerprintWaiting();
    rfidCardLatched = false;
    rfidMissingCount = 0;
  }
  stabilizeFingerprint();
  showWaitingScreen();
}
void maintainSensors() {
  if ( systemState != STATE_WAITING ) {
    return;
  }
  if ( millis() - lastSensorRetryAt < SENSOR_RETRY_INTERVAL ) {
    return;
  }
  lastSensorRetryAt = millis();
  if (!fingerOK) {
    Serial.println( "THU KET NOI LAI AS608" );
    setupFingerprint();
    updateLCDByState();
  }
  if (!rfidOK) {
    Serial.println( "THU KET NOI LAI RC522" );
    setupRFID();
    updateLCDByState();
  }
}
void startMDNS() {
  if ( WiFi.status() != WL_CONNECTED ) {
    mdnsOK = false;
    return;
  }
  Serial.println( "KHOI DONG MDNS..." );
  if ( mdnsOK ) {
    MDNS.end();
    mdnsOK = false;
    delay(50);
  }
  if ( MDNS.begin( MDNS_HOSTNAME ) ) {
    mdnsOK = true;
    MDNS.addService( "http", "tcp", 80 );
    Serial.println( "MDNS SAN SANG" );
    Serial.println( "DIA CHI: http://smartdoor.local" );
  } else {
    mdnsOK = false;
    Serial.println( "MDNS KHOI DONG THAT BAI" );
  }
}
void loadWiFiCredentials() {
  WIFI_SSID = WIFI_DEFAULT_SSID;
  WIFI_PASSWORD = WIFI_DEFAULT_PASSWORD;
  Serial.println();
  Serial.print( "WIFI NGOAI MAC DINH: " );
  Serial.println( WIFI_SSID );
}
void startWiFiConfigPortal() {
  if ( wifiConfigPortalActive ) {
    return;
  }
  Serial.println();
  Serial.println( "==============================" );
  Serial.println( "BAT WIFI TRUC TIEP CUA ESP32" );
  Serial.print( "TEN WIFI: " );
  Serial.println( WIFI_SETUP_AP );
  Serial.println( "MAT KHAU AP: 12345678" );
  Serial.println( "IP ESP32 TRUC TIEP: http://192.168.4.1" );
  Serial.println( "==============================" );
  WiFi.mode( WIFI_AP_STA );
  bool apOK = WiFi.softAP( WIFI_SETUP_AP, WIFI_SETUP_PASSWORD );
  wifiConfigPortalActive = apOK;
  Serial.print( "AP ESP32: " );
  Serial.println( apOK ? "OK" : "LOI" );
  Serial.print( "IP AP: " );
  Serial.println( WiFi.softAPIP() );
}
void stopWiFiConfigPortal() {
  return;
}
String wifiConfigHtml() {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>SmartDoor WiFi</title>"
    "<style>"
    "body{font-family:Arial;background:#f4f6f8;margin:0;padding:24px;}"
    ".box{max-width:420px;margin:30px auto;background:white;padding:24px;"
    "border-radius:16px;box-shadow:0 4px 18px #0002;}"
    "h2{margin-top:0;}label{display:block;margin-top:14px;font-weight:bold;}"
    "input{width:100%;box-sizing:border-box;padding:12px;margin-top:6px;"
    "border:1px solid #bbb;border-radius:9px;font-size:16px;}"
    "button{width:100%;margin-top:20px;padding:13px;border:0;border-radius:9px;"
    "font-size:16px;font-weight:bold;background:#1f6feb;color:white;}"
    ".note{font-size:13px;color:#555;line-height:1.45;}"
    "</style></head><body><div class='box'>"
    "<h2>SMART DOOR - WIFI</h2>"
    "<p class='note'>Dien thoai dang ket noi truc tiep voi ESP32. Neu muon, nhap them WiFi/hotspot ma ESP32 se ket noi. "
    "Thong tin se duoc luu trong bo nho ESP32.</p>"
    "<form method='POST' action='/save-wifi'>"
    "<label>Ten WiFi (SSID)</label>"
    "<input name='ssid' maxlength='32' required placeholder='Vi du: DATN-Hoan'>"
    "<label>Mat khau WiFi</label>"
    "<input name='pass' type='password' maxlength='64' placeholder='De trong neu WiFi khong co mat khau'>"
    "<button type='submit'>LUU VA KET NOI</button>"
    "</form></div></body></html>";
  return html;
}
void handleWiFiConfigPage() {
  if ( !wifiConfigPortalActive ) {
    server.send( 200, "text/plain; charset=utf-8", "SmartDoor API dang hoat dong" );
    return;
  }
  server.send( 200, "text/html; charset=utf-8", wifiConfigHtml() );
}
void handleWiFiConfigSave() {
  if ( !wifiConfigPortalActive ) {
    server.send( 403, "text/plain; charset=utf-8", "Che do cau hinh WiFi dang tat" );
    return;
  }
  String newSsid = server.arg( "ssid" );
  String newPass = server.arg( "pass" );
  newSsid.trim();
  if ( newSsid.length() == 0 ) {
    server.send(
      400,
      "text/html; charset=utf-8",
      "<h3>SSID khong duoc de trong.</h3><a href='/'>Quay lai</a>"
    );
    return;
  }
  prefs.putString( "wifi_ssid", newSsid );
  prefs.putString( "wifi_pass", newPass );
  Serial.println();
  Serial.println( "DA LUU WIFI MOI" );
  Serial.print( "SSID: " );
  Serial.println( newSsid );
  server.send(
    200,
    "text/html; charset=utf-8",
    "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "</head><body style='font-family:Arial;padding:30px'>"
    "<h2>Da luu WiFi.</h2>"
    "<p>ESP32 se khoi dong lai. Hay bat hotspot/WiFi vua nhap neu chua bat.</p>"
    "</body></html>"
  );
  delay( 1200 );
  ESP.restart();
}
void showAPCredentialsAtBoot() {
  if ( !lcdOK || !wifiConfigPortalActive ) {
    return;
  }
  showLCD( WIFI_SETUP_AP, String("PASS:") + WIFI_SETUP_PASSWORD );
  Serial.println();
  Serial.println( "HIEN THI THONG TIN WIFI ESP32 TRONG 3 GIAY" );
  Serial.print( "SSID: " );
  Serial.println( WIFI_SETUP_AP );
  Serial.print( "PASSWORD: " );
  Serial.println( WIFI_SETUP_PASSWORD );
  unsigned long started = millis();
  while ( millis() - started < 3000 ) {
    feedWatchdog();
    if ( webServerStarted ) {
      server.handleClient();
    }
    delay(10);
  }
}
void setupWiFi() {
  loadWiFiCredentials();
  startWiFiConfigPortal();
  WiFi.setAutoReconnect( true );
  WiFi.persistent( false );
  if ( WIFI_SSID.length() == 0 ) {
    wasWiFiConnected = false;
    mdnsOK = false;
    Serial.println( "CHUA CO WIFI STA DA LUU - CHI CHAY AP TRUC TIEP" );
    startMDNS();
    return;
  }
  WiFi.mode( WIFI_AP_STA );
  WiFi.begin( WIFI_SSID.c_str(), WIFI_PASSWORD.c_str() );
  unsigned long started = millis();
  while ( WiFi.status() != WL_CONNECTED &&
          millis() - started < 12000 ) {
    feedWatchdog();
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  if ( WiFi.status() == WL_CONNECTED ) {
    wasWiFiConnected = true;
    Serial.println( "WIFI STA KET NOI OK" );
    Serial.print( "IP STA ESP32: " );
    Serial.println( WiFi.localIP() );
    Serial.print( "IP AP ESP32: " );
    Serial.println( WiFi.softAPIP() );
    startMDNS();
    configTime(
      7 * 3600,
      0,
      "pool.ntp.org",
      "time.google.com"
    );
  }
  else {
    wasWiFiConnected = false;
    mdnsOK = false;
    Serial.println( "KHONG KET NOI DUOC WIFI STA DA LUU" );
    Serial.println( "AP SmartDoor_Direct VAN HOAT DONG" );
    Serial.println( "DIEN THOAI CO THE KET NOI SMARTDOOR_DIRECT VA MO APP" );
    startMDNS();
  }
}
void maintainWiFi() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if ( connected && !wasWiFiConnected ) {
    wasWiFiConnected = true;
    stabilizeFingerprint();
    Serial.println();
    Serial.println( "==============================" );
    Serial.println( "WIFI STA DA KET NOI LAI" );
    Serial.print( "IP STA: " );
    Serial.println( WiFi.localIP() );
    Serial.print( "IP AP TRUC TIEP: " );
    Serial.println( WiFi.softAPIP() );
    mdnsOK = false;
    startMDNS();
    Serial.print( "MDNS: " );
    Serial.println( mdnsOK ? "smartdoor.local" : "LOI" );
    Serial.println( "==============================" );
    return;
  }
  if ( connected ) {
    wasWiFiConnected = true;
    return;
  }
  if ( wasWiFiConnected ) {
    wasWiFiConnected = false;
    stabilizeFingerprint();
    Serial.println();
    Serial.println( "==============================" );
    Serial.println( "WIFI STA DA MAT" );
    Serial.println( "AP SmartDoor_Direct VAN HOAT DONG" );
    Serial.println( "VT / RFID / MAT KHAU VAN HOAT DONG" );
    Serial.println( "APP CO THE KET NOI AP 192.168.4.1" );
    Serial.println( "==============================" );
  }
  if ( !wifiConfigPortalActive ) {
    startWiFiConfigPortal();
  }
  if ( WIFI_SSID.length() == 0 ) {
    return;
  }
  if ( millis() - lastWiFiReconnectAt < WIFI_RECONNECT_INTERVAL ) {
    return;
  }
  lastWiFiReconnectAt = millis();
  Serial.println( "DANG THU KET NOI LAI WIFI STA DA LUU..." );
  WiFi.disconnect( false, false );
  delay(100);
  WiFi.mode( WIFI_AP_STA );
  WiFi.begin( WIFI_SSID.c_str(), WIFI_PASSWORD.c_str() );
}
void maintainAPClients() {
  if ( !wifiConfigPortalActive ) {
    previousAPClientCount = 0;
    return;
  }
  uint8_t currentAPClientCount = WiFi.softAPgetStationNum();
  if ( currentAPClientCount > previousAPClientCount ) {
    Serial.println();
    Serial.println( "==============================" );
    Serial.println( "THIET BI DA KET NOI SMARTDOOR_DIRECT" );
    Serial.print( "SO THIET BI AP: " );
    Serial.println( currentAPClientCount );
    Serial.println( "CHO APP GUI API HEARTBEAT..." );
    Serial.println( "==============================" );
  }
  if ( currentAPClientCount < previousAPClientCount ) {
    Serial.print( "THIET BI NGAT SMARTDOOR_DIRECT - CON LAI: " );
    Serial.println( currentAPClientCount );
  }
  previousAPClientCount = currentAPClientCount;
}
void noteAppContact() {
  lastAppContactAt = millis();
}
void updateAppConnectionState() {
  bool nowConnected =
    ( lastAppContactAt != 0 ) &&
    ( millis() - lastAppContactAt <= APP_CONNECTION_TIMEOUT );
  if ( nowConnected && !appConnected ) {
    appConnected = true;
    Serial.println();
    Serial.println( "==============================" );
    Serial.println( "APP DA KET NOI LAI" );
    Serial.println( "NHAN DUOC HEARTBEAT/API REQUEST" );
    Serial.println( "==============================" );
    if ( systemState == STATE_WAITING ) {
      showLCD( "DA KET NOI LAI", "APP SAN SANG" );
      networkDisplayState = NET_DISPLAY_RECONNECTED_IP;
      networkDisplayStartedAt = millis();
    }
    return;
  }
  if ( !nowConnected && appConnected ) {
    appConnected = false;
    Serial.println();
    Serial.println( "==============================" );
    Serial.println( "APP DA MAT KET NOI" );
    Serial.println( "KHONG CON HEARTBEAT/API REQUEST" );
    Serial.println( "==============================" );
    if ( systemState == STATE_WAITING ) {
      showLCD( "APP MAT KET NOI", "CHE DO OFFLINE" );
      networkDisplayState = NET_DISPLAY_APP_LOST;
      networkDisplayStartedAt = millis();
    }
    return;
  }
  appConnected = nowConnected;
}
void updateNetworkDisplay() {
  if ( networkDisplayState == NET_DISPLAY_NONE ) {
    return;
  }
  if ( systemState != STATE_WAITING ) {
    networkDisplayState = NET_DISPLAY_NONE;
    updateLCDByState();
    return;
  }
  unsigned long elapsed = millis() - networkDisplayStartedAt;
  if ( networkDisplayState == NET_DISPLAY_APP_LOST ) {
    if ( elapsed >= WIFI_LOST_DISPLAY_TIME ) {
      networkDisplayState = NET_DISPLAY_NONE;
      updateLCDByState();
    }
    return;
  }
  if ( networkDisplayState == NET_DISPLAY_RECONNECTED_IP ) {
    if ( elapsed >= RECONNECT_IP_DISPLAY_TIME ) {
      networkDisplayState = NET_DISPLAY_NONE;
      updateLCDByState();
    }
    return;
  }
}
void sendJson( int code, const String& json ) {
  server.send( code, "application/json; charset=utf-8", json );
}
String escapeJsonString( const String& value ) {
  String result = value;
  result.replace( "\\", "\\\\" );
  result.replace( "\"", "\\\"" );
  result.replace( "\n", "\\n" );
  return result;
}
void sendBusyResponse() {
  String json = "{";
  json += "\"success\":false,";
  json += "\"message\":\"He thong dang ban\",";
  json += "\"state\":\"";
  json += getSystemStateName();
  json += "\"";
  json += "}";
  sendJson( 409, json );
}
void handleApiStatus() {
  noteAppContact();
  apiRequestCount++;
  String json = "{";
  json += "\"success\":true,";
  json += "\"doorLocked\":";
  json += ( systemState == STATE_DOOR_OPEN ? "false" : "true" );
  json += ",";
  json += "\"fingerprintConnected\":";
  json += fingerOK ? "true" : "false";
  json += ",";
  json += "\"rfidConnected\":";
  json += rfidOK ? "true" : "false";
  json += ",";
  json += "\"wifiConnected\":";
  json += ( WiFi.status() == WL_CONNECTED ? "true" : "false" );
  json += ",";
  json += "\"mdnsConnected\":";
  json += mdnsOK ? "true" : "false";
  json += ",";
  json += "\"hostname\":\"smartdoor.local\",";
  json += "\"ip\":\"";
  if ( WiFi.status() == WL_CONNECTED ) {
    json += WiFi.localIP().toString();
  } else {
    json += "0.0.0.0";
  }
  json += "\",";
  json += "\"state\":\"";
  json += getSystemStateName();
  json += "\",";
  json += "\"busy\":";
  json += isSystemBusy() ? "true" : "false";
  json += ",";
  json += "\"failedAttempts\":";
  json += String( failedAttempts );
  json += ",";
  json += "\"fingerprintCount\":";
  json += String( cachedFingerprintCount );
  json += ",";
  json += "\"historyCount\":";
  json += String( doorHistoryCount );
  json += ",";
  json += "\"uptimeSeconds\":";
  json += String( millis() / 1000UL );
  json += ",";
  json += "\"successfulOpenCount\":";
  json += String( successfulOpenCount );
  json += ",";
  json += "\"apiRequestCount\":";
  json += String( apiRequestCount );
  json += ",";
  json += "\"wifiRssi\":";
  if ( WiFi.status() == WL_CONNECTED ) {
    json += String( WiFi.RSSI() );
  } else {
    json += "0";
  }
  json += ",";
  json += "\"freeHeap\":";
  json += String( ESP.getFreeHeap() );
  json += "}";
  sendJson( 200, json );
}
void handleApiOpenDoor() {
  noteAppContact();
  apiRequestCount++;
  if ( systemState == STATE_LOCKOUT ) {
    sendJson( 423, "{\"success\":false," "\"message\":\"He thong dang khoa tam 5 giay\"}" );
    return;
  }
  if ( systemState != STATE_WAITING ) {
    sendBusyResponse();
    return;
  }
  networkDisplayState = NET_DISPLAY_NONE;
  unlockDoor( "APP", "Mo cua tu app" );
  sendJson( 200, "{\"success\":true," "\"message\":\"Da mo cua\"}" );
}
void handleApiLockDoor() {
  noteAppContact();
  apiRequestCount++;
  networkDisplayState = NET_DISPLAY_NONE;
  if (systemState != STATE_DOOR_OPEN) {
    sendJson(409,
             "{\"success\":false,"
             "\"message\":\"Cua khong o trang thai mo\"}");
    return;
  }
  lockDoor();
  sendJson(200,
           "{\"success\":true,"
           "\"message\":\"Da khoa cua\"}");
}
void handleApiFingerprintCount() {
  noteAppContact();
  apiRequestCount++;
  if (!fingerOK) {
    sendJson( 503, "{\"success\":false," "\"message\":\"AS608 khong ket noi\"," "\"count\":0}" );
    return;
  }
  String json = "{";
  json += "\"success\":true,";
  json += "\"count\":";
  json += String( cachedFingerprintCount );
  json += "}";
  sendJson( 200, json );
}
void handleApiEnrollFingerprint() {
  noteAppContact();
  apiRequestCount++;
  if ( systemState == STATE_LOCKOUT ) {
    sendJson( 423, "{\"success\":false," "\"message\":\"He thong dang khoa tam\"}" );
    return;
  }
  if ( systemState != STATE_WAITING ) {
    sendBusyResponse();
    return;
  }
  if (!fingerOK) {
    sendJson( 503, "{\"success\":false," "\"message\":\"AS608 khong ket noi\"}" );
    return;
  }
  if ( !server.hasArg( "id" ) ) {
    sendJson( 400, "{\"success\":false," "\"message\":\"Thieu tham so id\"}" );
    return;
  }
  int id = server.arg( "id" ).toInt();
  if ( id < MIN_FINGER_ID || id > MAX_FINGER_ID ) {
    showResult( "ID KHONG HOP LE", "CHI TU 1 DEN 127" );
    String json = "{";
    json += "\"success\":false,";
    json += "\"message\":\"ID khong hop le. Chi duoc nhap tu 1 den 127\",";
    json += "\"id\":";
    json += String(id);
    json += "}";
    sendJson( 400, json );
    return;
  }
  bool success = enrollFingerprint( (uint16_t)id );
  if (success) {
    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"Dang ky van tay thanh cong\",";
    json += "\"id\":";
    json += String(id);
    json += ",";
    json += "\"count\":";
    json += String( cachedFingerprintCount );
    json += "}";
    sendJson( 200, json );
    return;
  }
  if ( lastEnrollResult == ENROLL_RESULT_ID_EXISTS ) {
    String json = "{";
    json += "\"success\":false,";
    json += "\"message\":\"ID da ton tai, vui long chon ID khac\",";
    json += "\"id\":";
    json += String(id);
    json += "}";
    sendJson( 409, json );
    return;
  }
  if ( lastEnrollResult == ENROLL_RESULT_DUPLICATE ) {
    String json = "{";
    json += "\"success\":false,";
    json += "\"message\":\"Van tay nay da duoc dang ky\",";
    json += "\"existingId\":";
    json += String( duplicateFingerprintId );
    json += "}";
    sendJson( 409, json );
    return;
  }
  if ( lastEnrollResult == ENROLL_RESULT_MISMATCH ) {
    sendJson( 422, "{\"success\":false," "\"message\":\"Hai lan quet khong giong nhau\"}" );
    return;
  }
  if ( lastEnrollResult == ENROLL_RESULT_TIMEOUT ) {
    sendJson( 408, "{\"success\":false," "\"message\":\"Qua thoi gian dang ky van tay\"}" );
    return;
  }
  sendJson( 500, "{\"success\":false," "\"message\":\"Dang ky van tay that bai\"}" );
}
void handleApiDeleteFingerprint() {
  noteAppContact();
  apiRequestCount++;
  if ( systemState == STATE_LOCKOUT ) {
    sendJson( 423, "{\"success\":false," "\"message\":\"He thong dang khoa tam\"}" );
    return;
  }
  if ( systemState != STATE_WAITING ) {
    sendBusyResponse();
    return;
  }
  if (!fingerOK) {
    sendJson( 503, "{\"success\":false," "\"message\":\"AS608 khong ket noi\"}" );
    return;
  }
  if ( !server.hasArg( "id" ) ) {
    sendJson( 400, "{\"success\":false," "\"message\":\"Thieu tham so id\"}" );
    return;
  }
  int id = server.arg( "id" ).toInt();
  if ( id < MIN_FINGER_ID || id > MAX_FINGER_ID ) {
    showResult( "ID KHONG HOP LE", "CHI TU 1 DEN 127" );
    String json = "{";
    json += "\"success\":false,";
    json += "\"message\":\"ID khong hop le. Chi duoc nhap tu 1 den 127\",";
    json += "\"id\":";
    json += String(id);
    json += "}";
    sendJson( 400, json );
    return;
  }
  if ( !fingerprintIdExists( id ) ) {
    showResult( "ID KHONG TON TAI", "KHONG THE XOA" );
    String json = "{";
    json += "\"success\":false,";
    json += "\"message\":\"ID van tay chua co. Khong the xoa\",";
    json += "\"id\":";
    json += String(id);
    json += "}";
    sendJson( 404, json );
    return;
  }
  bool success = deleteFingerprint( (uint16_t)id );
  if (success) {
    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"Xoa van tay thanh cong\",";
    json += "\"id\":";
    json += String(id);
    json += ",";
    json += "\"count\":";
    json += String( cachedFingerprintCount );
    json += "}";
    sendJson( 200, json );
  } else {
    sendJson( 500, "{\"success\":false," "\"message\":\"Xoa van tay that bai\"}" );
  }
}
void handleApiHistory() {
  noteAppContact();
  apiRequestCount++;
  String json = "{";
  json += "\"success\":true,";
  json += "\"count\":";
  json += String( doorHistoryCount );
  json += ",";
  json += "\"history\":[";
  for ( byte i = 0; i < doorHistoryCount; i++ ) {
    if ( i > 0 ) {
      json += ",";
    }
    json += "{";
    json += "\"time\":\"";
    json += escapeJsonString( doorHistory[i].time );
    json += "\",";
    json += "\"method\":\"";
    json += escapeJsonString( doorHistory[i].method );
    json += "\",";
    json += "\"detail\":\"";
    json += escapeJsonString( doorHistory[i].detail );
    json += "\"";
    json += "}";
  }
  json += "]";
  json += "}";
  sendJson( 200, json );
}
void setupWebServer() {
  server.on( "/api/status", HTTP_GET, handleApiStatus );
  server.on( "/api/door/open", HTTP_POST, handleApiOpenDoor );
  server.on( "/api/door/lock", HTTP_POST, handleApiLockDoor );
  server.on( "/api/fingerprint/count", HTTP_GET, handleApiFingerprintCount );
  server.on( "/api/fingerprint/enroll", HTTP_POST, handleApiEnrollFingerprint );
  server.on( "/api/fingerprint/delete", HTTP_POST, handleApiDeleteFingerprint );
  server.on( "/api/history", HTTP_GET, handleApiHistory );
  server.onNotFound( []() { apiRequestCount++; sendJson( 404, "{\"success\":false," "\"message\":\"Khong tim thay API\"}" ); } );
  server.begin();
  webServerStarted = true;
  Serial.println( "WEB SERVER DA CHAY" );
  if ( WiFi.status() == WL_CONNECTED ) {
    Serial.print( "IP API: http://" );
    Serial.println( WiFi.localIP() );
    Serial.println( "HOST: http://smartdoor.local" );
  }
  if ( wifiConfigPortalActive ) {
    Serial.println( "AP TRUC TIEP: SmartDoor_Direct" );
    Serial.println( "API TRUC TIEP: http://192.168.4.1" );
  }
}
void setup() {
  pinMode( RELAY_LOCK_PIN, OUTPUT );
  digitalWrite( RELAY_LOCK_PIN, DOOR_LOCK_LEVEL );
  pinMode( BUZZER_PIN, OUTPUT );
  digitalWrite( BUZZER_PIN, BUZZER_OFF_LEVEL );
  Serial.begin( 115200 );
  delay(500);
  Serial.println();
  Serial.println( "=================================" );
  Serial.println( "SMART DOOR LOCK" );
  Serial.println( "=================================" );
  setupWatchdog();
  feedWatchdog();
  prefs.begin( "smartdoor", false );
  loadSecurityConfig();
  loadHistoryFromFlash();
  feedWatchdog();
  setupKeypad();
  Wire.begin( LCD_SDA, LCD_SCL );
  int lcdStatus = lcd.begin( 16, 2 );
  if ( lcdStatus != 0 ) {
    lcdOK = false;
    Serial.println( "LCD LOI - TIEP TUC CHAY" );
  } else {
    lcdOK = true;
    lcd.backlight();
    systemState = STATE_BOOT;
    updateLCDByState();
    unsigned long bootStart = millis();
    while ( millis() - bootStart < 1000 ) {
      feedWatchdog();
      delay(20);
    }
  }
  feedWatchdog();
  setupFingerprint();
  feedWatchdog();
  setupRFID();
  feedWatchdog();
  setupWiFi();
  feedWatchdog();
  setupWebServer();
  showAPCredentialsAtBoot();
  digitalWrite( RELAY_LOCK_PIN, DOOR_LOCK_LEVEL );
  stopBuzzer();
  failedAttempts = 0;
  lastAppContactAt = 0;
  appConnected = false;
  systemState = STATE_WAITING;
  inputBuffer = "";
  lastKeyInputAt = 0;
  currentDoorMethod = "";
  resetFingerprintWaiting();
  showLCD( "APP MAT KET NOI", "CHE DO OFFLINE" );
  networkDisplayState = NET_DISPLAY_APP_LOST;
  networkDisplayStartedAt = millis();
  Serial.println();
  Serial.println( "=================================" );
  Serial.println( "HE THONG SAN SANG" );
  Serial.println( "=================================" );
  Serial.print( "AS608: " );
  Serial.println( fingerOK ? "OK" : "LOI" );
  Serial.print( "RC522: " );
  Serial.println( rfidOK ? "OK" : "LOI" );
  Serial.print( "WIFI: " );
  Serial.println( WiFi.status() == WL_CONNECTED ? "OK" : "OFFLINE" );
  if ( wifiConfigPortalActive ) {
    Serial.println( "AP TRUC TIEP: SmartDoor_Direct" );
    Serial.println( "MAT KHAU AP: 12345678" );
    Serial.println( "IP AP: http://192.168.4.1" );
  }
  Serial.print( "MDNS: " );
  Serial.println( mdnsOK ? "OK" : "LOI" );
  if ( WiFi.status() == WL_CONNECTED ) {
    Serial.print( "IP: " );
    Serial.println( WiFi.localIP() );
    Serial.println( "HOST: http://smartdoor.local" );
  }
  Serial.print( "SO VAN TAY: " );
  Serial.println( cachedFingerprintCount );
  Serial.print( "SO THE RFID: " );
  Serial.println( allowedRfidCount );
  Serial.print( "SO HISTORY: " );
  Serial.println( doorHistoryCount );
  Serial.print( "STATE: " );
  Serial.println( getSystemStateName() );
  Serial.println( "=================================" );
}
void loop() {
  feedWatchdog();
  maintainWiFi();
  maintainAPClients();
  maintainSensors();
  if ( webServerStarted && ( WiFi.status() == WL_CONNECTED || wifiConfigPortalActive ) ) {
    server.handleClient();
  }
  updateAppConnectionState();
  updateNetworkDisplay();
  updateLockout();
  updateDoor();
  updateBuzzer();
  updateResultScreen();
  processKeypad();
  processRFID();
  processFingerprint();
  delay(3);
}
