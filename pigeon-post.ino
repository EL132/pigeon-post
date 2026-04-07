#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

#define LED_PIN 18
#define SERVO_PIN 19
#define BUTTON_PIN 13
#define NUM_LEDS 60

/* campus wifi */
const char* ssid = "GTother";
const char* password = "pswd";

const char* postUrl = "https://pigeon-post-message-server.onrender.com/message";
const char* getUrl  = "https://pigeon-post-message-server.onrender.com/message";

CRGB leds[NUM_LEDS];
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

bool messageSent = false;

// Updates LCD + LED strip together
void updateStatus(String line1, String line2, CRGB color) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);

  if (line2.length() > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }

  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void setup() {
  Serial.begin(115200);

  // LCD setup
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // LED strip setup
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(80);

  // Servo setup
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(0);

  // Button setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Show connecting status
  updateStatus("Connecting...", "", CRGB::Yellow);

  // WiFi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");

  updateStatus("Press button", "", CRGB::Orange);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW && !messageSent) {
    Serial.println("Button pressed");

    sendMessage();
    delay(1000);
    fetchMessage();

    messageSent = true;
  }

  // reset when button released
  if (digitalRead(BUTTON_PIN) == HIGH) {
    messageSent = false;
  }
}

void sendMessage() {
  HTTPClient http;

  updateStatus("sending msg...", "", CRGB::Blue);

  http.begin(postUrl);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"message\":\"pigeon fly away\"}";
  int httpCode = http.POST(body);

  Serial.print("POST code: ");
  Serial.println(httpCode);

  http.end();
}

void fetchMessage() {
  HTTPClient http;

  http.begin(getUrl);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();

    Serial.println("Server response:");
    Serial.println(payload);

    // SUCCESS = green
    updateStatus("msg received", "fly away", CRGB::Green);

    moveServo();
  } else {
    updateStatus("GET failed", "", CRGB::Red);
  }

  http.end();
}

void moveServo() {
  for (int i = 0; i < 3; i++) {
    myServo.write(90);
    delay(300);
    myServo.write(0);
    delay(300);
  }
}
