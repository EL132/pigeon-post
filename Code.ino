#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// --- PROTOTYPES ---
bool I2S_Record_Init();
bool Record_Start(String audio_filename);
bool Record_Available(String audio_filename, float* audiolength_sec);
String SpeechToText_Deepgram(String audio_filename);

// --- PINS ---
#define LED_PIN      18
#define SERVO_PIN    19
#define REC_BUTTON   36  // RIGHT Button (Pin 36 / VP)
#define MSG_BUTTON   32  // LEFT Button (Pin 32)
#define NUM_LEDS     60
#define SD_SCK  14
#define SD_MOSI 13
#define SD_MISO 27
#define SD_CS   26

#define AUDIO_FILE "/audio.wav"

// --- SETTINGS ---
const char* ssid = "EliasPhone";
const char* password = "bet you'd like that";
const char* postUrl = "https://pigeon-post-message-server.onrender.com/message";
const char* getUrl  = "https://pigeon-post-message-server.onrender.com/message";
const String myDeviceID = "Pigeon_Alpha"; // Give this specific ESP32 a unique name

// --- STATE MANAGEMENT ---
enum SystemState { RESTING, RECORDING, REVIEWING, SENDING, INCOMING };
SystemState currentState = RESTING;
SystemState lastState = SENDING; // Force first refresh

CRGB leds[NUM_LEDS];
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

String currentTranscript = "";
unsigned long lastPollTime = 0;
unsigned long stateStartTime = 0;
int scrollPos = 0;
unsigned long lastScrollTime = 0;

// --- HELPERS ---

void updateLEDs(struct CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void displayStatic(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void scrollLineOne(String text) {
  if (text.length() <= 16) {
    lcd.setCursor(0, 0);
    lcd.print(text);
    return;
  }
  
  if (millis() - lastScrollTime > 350) {
    String displayStr = text + "   "; 
    lcd.setCursor(0, 0);
    for (int i = 0; i < 16; i++) {
      int index = (scrollPos + i) % displayStr.length();
      lcd.print(displayStr[index]);
    }
    scrollPos++;
    lastScrollTime = millis();
  }
}

// --- NETWORK ---

bool checkServerForMessages() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, getUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      int secondsAgo = doc["seconds_ago"] | 999; 
      String senderID = doc["sender_id"] | ""; // Get the sender ID from the server

      // NEW LOGIC: Only trigger if it's fresh AND not sent by me
      if (secondsAgo < 15 && senderID != myDeviceID) {
        currentTranscript = doc["message"] | "";
        http.end();
        return true;
      }
    }
  }
  
  http.end();
  return false;
}

void postMessage(String text) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, postUrl);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"message\":\"" + text + "\", \"id\":\"" + myDeviceID + "\"}";
  http.POST(body);
  http.end();
}

// --- SETUP ---

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(80);
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(100);

  pinMode(REC_BUTTON, INPUT); 
  pinMode(MSG_BUTTON, INPUT_PULLUP);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  SD.begin(SD_CS);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  I2S_Record_Init();
  updateLEDs(CRGB(50, 50, 50));
}

// --- MAIN LOOP ---

void loop() {
  bool recPressed = (digitalRead(REC_BUTTON) == LOW); 
  bool sendPressed = (digitalRead(MSG_BUTTON) == LOW);

  switch (currentState) {
    
    case RESTING:
      // Only update the screen once when entering this state to stop flickering
      if (lastState != RESTING) {
        displayStatic("Something", "to share?");
        updateLEDs(CRGB(50, 50, 50));
        lastState = RESTING;
      }
      
      if (recPressed) {
        currentState = RECORDING;
      } 
      else if (millis() - lastPollTime > 10000) {
        lastPollTime = millis();
        if (checkServerForMessages()) {
          currentState = INCOMING;
          stateStartTime = millis();
        }
      }
      break;

    case RECORDING:
      if (lastState != RECORDING) {
        updateLEDs(CRGB::Red);
        displayStatic("Recording...", "Keep Holding");
        Record_Start(AUDIO_FILE);
        lastState = RECORDING;
      }

      if (!recPressed) {
        currentState = REVIEWING; // Process after release
      } else {
        Record_Start(AUDIO_FILE); 
      }
      break;

    case REVIEWING:
      if (lastState != REVIEWING) {
        updateLEDs(CRGB::Purple);
        displayStatic("Transcribing", "Please wait...");
        float dur = 0;
        if (Record_Available(AUDIO_FILE, &dur)) {
          currentTranscript = SpeechToText_Deepgram(AUDIO_FILE);
          scrollPos = 0;
          lcd.clear();
          updateLEDs(CRGB::Blue);
          lastState = REVIEWING;
        } else {
          currentState = RESTING;
        }
      }

      scrollLineOne(currentTranscript);
      lcd.setCursor(0, 1);
      lcd.print("L:Send R:Redo   ");

      if (sendPressed) currentState = SENDING;
      if (recPressed) currentState = RECORDING;
      break;

    case SENDING:
      updateLEDs(CRGB::Yellow);
      displayStatic("Sending...", "");
      postMessage(currentTranscript);
      displayStatic("Message sent", "");
      delay(5000); 
      myServo.write(100); // Ensure DOOR IS CLOSED after sending
      currentState = RESTING;
      break;

    case INCOMING:
      // Show Alert for the first 2.5 seconds
      if (millis() - stateStartTime < 2500) {
        if (lastState != INCOMING) {
          updateLEDs(CRGB::Green);
          displayStatic("MESSAGE", "INCOMING");
          for(int pos = 90; pos >= 10; pos--) {
            myServo.write(pos);
            delay(30); // Increase this number to move slower, decrease to move faster
          }
          lastState = INCOMING;
        }
      } 
      // After 2.5 seconds, start scrolling the message
      else {
        scrollLineOne(currentTranscript);
        lcd.setCursor(0, 1);
        lcd.print("                "); // Keep second line clear
      }
      
      if (millis() - stateStartTime > 15000 || recPressed || sendPressed) {
        myServo.write(100); // CLOSE DOOR
        currentState = RESTING;
      }
      break;
  }
  delay(10);
}
