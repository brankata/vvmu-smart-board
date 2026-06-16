#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Keypad.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi настройки
const char* ssid = "VVMU";
const char* password = "vvmu1234";
const char* serverIP = "10.208.241.176";
const int serverPort = 8000;

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// RTC
RTC_DS3231 rtc;

// Клавиатура
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 33, 32, 5};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Дни от седмицата
String days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Данни за разписанието
struct Lesson {
  String discipline;
  String start_time;
  String end_time;
  String hall;
  String teacher;
  String type;
};

Lesson schedule[10];
int scheduleCount = 0;
int currentLesson = 0;
int currentDayIndex = 0;
int currentView = 1;

void beep(int duration = 100) {
  tone(25, 1000, duration);
}

void displayLesson() {
  lcd.clear();
  if (scheduleCount == 0) {
    lcd.setCursor(0, 0);
    lcd.print(days[currentDayIndex]);
    lcd.setCursor(0, 1);
    if (WiFi.status() != WL_CONNECTED) {
      lcd.print("No data");     // Няма данни поради липса на WiFi
    } else {
      lcd.print("No lessons");  // Има връзка но няма занятия за деня
    }
    return;
  }

  if (currentView == 1) {
    lcd.setCursor(0, 0);
    String header = days[currentDayIndex] + " " + String(currentLesson + 1) + "/" + String(scheduleCount);
    lcd.print(header.substring(0, 16));
    lcd.setCursor(0, 1);
    String timeHall = schedule[currentLesson].start_time.substring(0, 5) + "-" +
                      schedule[currentLesson].end_time.substring(0, 5) + " " +
                      "z." + schedule[currentLesson].hall;
    lcd.print(timeHall.substring(0, 16));
  }
  else if (currentView == 2) {
    lcd.setCursor(0, 0);
    lcd.print(schedule[currentLesson].discipline.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(schedule[currentLesson].type.substring(0, 16));
  }
  else if (currentView == 3) {
    lcd.setCursor(0, 0);
    lcd.print("Teacher:");
    lcd.setCursor(0, 1);
    lcd.print(schedule[currentLesson].teacher.substring(0, 16));
  }
}

bool fetchSchedule(int dayIndex) {
  // Проверка за активна WiFi връзка
  if (WiFi.status() != WL_CONNECTED) {
    // Опит за автоматично повторно свързване
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
    // При неуспешно свързване запазва последните данни
    if (WiFi.status() != WL_CONNECTED) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("No WiFi!");
      return false;
    }
  }

  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/schedule/day/" + String(dayIndex);
  http.begin(url);
  http.setTimeout(10000); // Таймаут от 10 секунди
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc; //Буфер от 4096 байта за JSON
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("JSON error!");
      http.end();
      return false;
    }
    JsonArray arr = doc["schedule"].as<JsonArray>();
    scheduleCount = 0;
    for (JsonObject lesson : arr) {
      if (scheduleCount >= 10) break;
      schedule[scheduleCount].discipline = lesson["discipline"].as<String>();
      schedule[scheduleCount].start_time = lesson["start_time"].as<String>();
      schedule[scheduleCount].end_time   = lesson["end_time"].as<String>();
      schedule[scheduleCount].hall       = lesson["hall"].as<String>();
      schedule[scheduleCount].teacher    = lesson["teacher"].as<String>();
      schedule[scheduleCount].type       = lesson["type"].as<String>();
      scheduleCount++;
    }
    http.end();
    return true;
  }
  http.end();
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  Wire1.begin(19, 18);
  rtc.begin(&Wire1);

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Първи опит за свързване
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi connected!");
    beep(200);
    delay(1000);
  } else {
    // Първият опит неуспешен
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi error!");
    delay(2000);

    // Автоматичен втори опит
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      // Вторият опит успешен
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi connected!");
      beep(200);
      delay(1000);
    } else {
      // Вторият опит неуспешен
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("No WiFi!");
      delay(2000);
    }
  }

  DateTime now = rtc.now();
  currentDayIndex = now.dayOfTheWeek();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loading...");
  fetchSchedule(currentDayIndex);
  currentLesson = 0;
  currentView = 1;
  displayLesson();
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    beep(50);

    if (key == '1') {
      currentView = 1;
      displayLesson();
    }
    else if (key == '2') {
      currentView = 2;
      displayLesson();
    }
    else if (key == '3') {
      currentView = 3;
      displayLesson();
    }
    else if (key == 'A') {
      if (currentLesson < scheduleCount - 1) {
        currentLesson++;
        currentView = 1;
        displayLesson();
      }
    }
    else if (key == 'B') {
      if (currentLesson > 0) {
        currentLesson--;
        currentView = 1;
        displayLesson();
      }
    }
    else if (key == 'C') {
      currentDayIndex = (currentDayIndex + 1) % 7;
      fetchSchedule(currentDayIndex);
      currentLesson = 0;
      currentView = 1;
      displayLesson();
    }
    else if (key == 'D') {
      currentDayIndex = (currentDayIndex - 1 + 7) % 7;
      fetchSchedule(currentDayIndex);
      currentLesson = 0;
      currentView = 1;
      displayLesson();
    }
    else if (key == '#') {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Refreshing...");
      fetchSchedule(currentDayIndex);
      currentLesson = 0;
      currentView = 1;
      displayLesson();
      beep(300);
    }
    else if (key == '*') {
      DateTime now = rtc.now();
      currentDayIndex = now.dayOfTheWeek();
      fetchSchedule(currentDayIndex);
      currentLesson = 0;
      currentView = 1;
      displayLesson();
    }
  }

  delay(100);
}