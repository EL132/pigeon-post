#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define LED_PIN 18
#define NUM_LEDS 60

const char* ssid = "SSID";
const char* password = "PSWD";

const char* postUrl = "https://pigeon-post-message-server.onrender.com/message";
const char* getUrl  = "https://pigeon-post-message-server.onrender.com/message";

CRGB leds[NUM_LEDS];
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // LED strip starts dull yellow
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB(180, 140, 0));
  FastLED.show();

  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  // WiFi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");

  sendMessage();
  delay(1000);
  fetchMessage();
}

void loop() {
}

void sendMessage() {
  HTTPClient http;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("sending msg...");

  http.begin(postUrl);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"message\":\"pigeon fly away\"}";

  int httpCode = http.POST(body);

  Serial.print("POST code: ");
  Serial.println(httpCode);

  http.end();

  // update LEDs to light blue
  fill_solid(leds, NUM_LEDS, CRGB::Cyan);
  FastLED.show();
}

void fetchMessage() {
  HTTPClient http;

  http.begin(getUrl);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();

    Serial.println("Server response:");
    Serial.println(payload);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("msg received");

    lcd.setCursor(0, 1);
    lcd.print("fly away");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GET failed");
  }

  http.end();
}
