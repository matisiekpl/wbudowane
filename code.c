// Glucose Meter
// Author: Mateusz Wozniak
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SD.h>

#define POT_PIN A0
#define BUTTON_PIN 2
#define BUZZER_PIN 9
#define SD_CS 10

const bool DEBUG = true;

const unsigned long LOOP_INTERVAL = 10;
const unsigned long MEAS_INTERVAL = 1000;

const unsigned long GLUCOSE_MIN = 60;
const unsigned long GLUCOSE_MAX = 250;

const unsigned long GLUCOSE_WARNING_THRESHOLD = 180;
const unsigned long GLUCOSE_CRITICAL_THRESHOLD = 200;

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

unsigned long lastMeasurementTime = 0;
unsigned long lastMeasurementValue = 0;

int lastDisplayedGlucose = -1;

int readGlucose() {
  int raw = analogRead(POT_PIN);
  int glucose = map(raw, 0, 1023, GLUCOSE_MIN,
       GLUCOSE_MAX); // mg/dL
  return glucose;
}

void printDataFile() {
  Serial.println("---");
  File dataFile = SD.open("glucose.csv");
  while (dataFile.available()) {
    Serial.write(dataFile.read());
  }
  dataFile.close();
}

void archive(int glucose) {
  DateTime now = rtc.now();

  File dataFile = SD.open("glucose.csv", FILE_WRITE);
  dataFile.print(now.unixtime());
  dataFile.print(",");
  dataFile.println(glucose);
  dataFile.close();
}

void printCurrentMeasurement(int glucose) {
  if (glucose != lastDisplayedGlucose) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Glucose: ");
    lcd.print(glucose);
    lcd.print(" mg/dL");
    lastDisplayedGlucose = glucose;

    if (glucose > GLUCOSE_CRITICAL_THRESHOLD) {
      lcd.setCursor(0, 1);
      lcd.print("Critical: ");
      lcd.setCursor(0, 2);
      lcd.print("Glucose > ");
      lcd.print(GLUCOSE_CRITICAL_THRESHOLD);
      lcd.print(" mg/dL");
    } else if (glucose > GLUCOSE_WARNING_THRESHOLD) {
      lcd.setCursor(0, 1);
      lcd.print("Warning: ");
      lcd.setCursor(0, 2);
      lcd.print("Glucose > ");
      lcd.print(GLUCOSE_WARNING_THRESHOLD);
      lcd.print(" mg/dL");
    } else {
      lcd.setCursor(0, 1);
      int blocks = map(glucose, GLUCOSE_MIN,
        GLUCOSE_WARNING_THRESHOLD, 0, 7);
      for(int i = 0; i < blocks; i++) {
        lcd.write(byte(255));
      }
    }
  }
}

void printStatistics() {
  lastDisplayedGlucose = -1;
  File dataFile = SD.open("glucose.csv");
  if (!dataFile) {
    lcd.clear();
    lcd.print("No data found");
    return;
  }

  unsigned long nowUnix = rtc.now().unixtime();

  unsigned long lastMinuteSum = 0, lastMinuteCount = 0;
  unsigned long lastHourSum = 0, lastHourCount = 0;

  while (dataFile.available()) {
    String line = dataFile.readStringUntil('\n');
    int commaIndex = line.indexOf(',');
    if (commaIndex == -1) continue;

    unsigned long timestamp = line.substring(0,
        commaIndex).toInt();
    int glucose = line.substring(commaIndex + 1).toInt();

    unsigned long age = nowUnix - timestamp;

    // Last minute: 60 seconds
    if (age <= 60UL) {
      lastMinuteSum += glucose;
      lastMinuteCount++;
    }
    // Last hour: 3600 seconds
    if (age <= 3600UL) {
      lastHourSum += glucose;
      lastHourCount++;
    }
  }
  dataFile.close();

  lcd.clear();
  lcd.setCursor(0, 0);
  if(lastMinuteCount) {
    lcd.print("1min avg:");
    lcd.print(lastMinuteSum / lastMinuteCount);
  } else {
    lcd.print("1min avg:N/A");
  }

  lcd.setCursor(0, 1);
  if(lastHourCount) {
    lcd.print("1h avg:");
    lcd.print(lastHourSum / lastHourCount);
  } else {
    lcd.print("1h avg:N/A");
  }
}


void draw(int glucose) {
  int buttonState = digitalRead(BUTTON_PIN);
  if(buttonState == LOW) {
    printCurrentMeasurement(glucose);
  } else {
    printStatistics();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.print("Glucose Meter");
  lcd.setCursor(0, 1);
  lcd.print("Mateusz Wozniak");

  if (!rtc.begin()) {
    Serial.println("RTC setup failed!");
    lcd.setCursor(0, 1);
    lcd.print("RTC error!");
  }
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card setup failed!");
    lcd.setCursor(0, 1);
    lcd.print("SD error!");
  }
  Serial.println("Initialized");

  delay(100);
  lcd.clear();
}

void checkAlert(int glucose) {
  if (glucose > GLUCOSE_CRITICAL_THRESHOLD) {
    tone(BUZZER_PIN, 1000);
  } else if (glucose > GLUCOSE_WARNING_THRESHOLD) {
    tone(BUZZER_PIN, 600, 100);
  } else {
    noTone(BUZZER_PIN);
  }
}

void loop() {
  unsigned long now = millis();

  if (now - lastMeasurementTime >= MEAS_INTERVAL) {
    // Fetch
    int glucose = readGlucose();
    lastMeasurementTime = now;
    lastMeasurementValue = glucose;
    // Process
    archive(glucose);
    checkAlert(glucose);
    if(DEBUG) {
      printDataFile();
    }
  }
  // Display
  draw(lastMeasurementValue);
  delay(LOOP_INTERVAL);
}