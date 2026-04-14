#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// --- RECORDING & TRANSCRIPTION PROTOTYPES ---
bool I2S_Record_Init();
bool Record_Start(String audio_filename);
bool Record_Available(String audio_filename, float* audiolength_sec);
String SpeechToText_Deepgram(String audio_filename);

// --- PIN DEFINITIONS ---
#define LED_PIN      18
#define SERVO_PIN    19
#define REC_BUTTON   36  // VP Pin (Hold to record)
#define MSG_BUTTON   32  // (Optional) Manual trigger button
#define NUM_LEDS     60

// MICROSD PINS (Safe Pins)
#define SD_SCK  14
#define SD_MOSI 13
#define SD_MISO 27
#define SD_CS   26

#define AUDIO_FILE "/audio.wav"

// --- SETTINGS ---
const char* ssid = "EliasPhone";
const char* password = "pswd";
const char* postUrl = "https://pigeon-post-message-server.onrender.com/message";
const char* getUrl  = "https://pigeon-post-message-server.onrender.com/message";

// --- GLOBALS ---
CRGB leds[NUM_LEDS];
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;
bool wasRecording = false;

// --- HELPERS ---
void updateStatus(String line1, String line2, struct CRGB color) {
  // Re-run init to clear any noise-induced gibberish
  lcd.init(); 
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (line2.length() > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
  
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void moveServo() {
  for (int i = 0; i < 3; i++) {
    myServo.write(90); delay(300);
    myServo.write(0);  delay(300);
  }
}

void sendMessage(String text) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  updateStatus("Sending...", "Pigeon Post", CRGB::Blue);
  
  http.begin(client, postUrl);
  http.addHeader("Content-Type", "application/json");

  // Create JSON body with the transcription
  String body = "{\"message\":\"" + text + "\"}";
  int httpCode = http.POST(body);

  if (httpCode > 0) {
    updateStatus("Sent!", "Waiting for reply", CRGB::Cyan);
  } else {
    updateStatus("POST Failed", "Error", CRGB::Red);
  }
  http.end();
}

void fetchMessage() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  updateStatus("Checking Mail", "...", CRGB::Purple);
  http.begin(client, getUrl);
  int httpCode = http.GET();

  if (httpCode > 0) {
    updateStatus("Msg Received", "Fly Away!", CRGB::Green);
    moveServo();
  } else {
    updateStatus("Fetch Failed", "", CRGB::Red);
  }
  http.end();
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);

  delay(1000);

  Serial.println("made it here");

  // 1. Peripherals
  Wire.begin(21, 22);
  Wire.setClock(100000); // 100kHz is the "Safe" speed for LCDs
  lcd.init();
  lcd.backlight();
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(80);
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(0);

  Serial.println("made it here 2.0");

  // 2. Pins
  pinMode(REC_BUTTON, INPUT); // Needs external pull-up
  pinMode(MSG_BUTTON, INPUT_PULLUP);

  Serial.println("made it here 3.0");

  updateStatus("Connecting...", "", CRGB::Yellow);

  // 3. Storage
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    updateStatus("SD Fail", "Check Wiring", CRGB::Red);
    while(true);
  }

  Serial.println("made it here 4.0");

  // 4. WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");

  Serial.println("made it here 5.0");

  // 5. Microphone
  if (!I2S_Record_Init()) {
    updateStatus("Mic Fail", "I2S Error", CRGB::Red);
    while(true);
  }

  updateStatus("Ready", "Hold to Record", CRGB::Orange);
  Serial.println("here!!");
}

// --- MAIN LOOP ---
void loop() {
  // Check if recording button (D36) is pressed (Logic depends on your wiring)
  // Assuming LOW when pressed if using a pull-up resistor
  bool recordingButtonPressed = digitalRead(REC_BUTTON) == LOW;

  // START RECORDING
  if (recordingButtonPressed) {
    if (!wasRecording) {
      updateStatus("Recording...", "Keep Holding", CRGB::Red);
      wasRecording = true;
    }
    Record_Start(AUDIO_FILE);
  }

  // STOP RECORDING & PROCESS
  if (!recordingButtonPressed && wasRecording) {
    float duration = 0.0;
    wasRecording = false;

    if (Record_Available(AUDIO_FILE, &duration)) {
      updateStatus("Transcribing...", "Deepgram", CRGB::Purple);
      
      String transcript = SpeechToText_Deepgram(AUDIO_FILE);
      
      Serial.println("Transcript: " + transcript);

      if (transcript.length() > 0) {
        // Send the transcription to your pigeon post server
        sendMessage(transcript);
        delay(500);
        // Fetch response
        fetchMessage();
      } else {
        updateStatus("No Audio", "Try Again", CRGB::Orange);
      }
    }
  }

  delay(10);
}