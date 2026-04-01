#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define LED_PIN 18
#define NUM_LEDS 60

const char* ssid = "SSID";
const char* password = "PASSWORD";

CRGB leds[NUM_LEDS];
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);

  // LCD setup
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  // LED strip setup
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::Cyan);
  FastLED.show();

  // WiFi connection
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected");

  fetchJoke();
}

void loop() {
}

void fetchJoke() {
  HTTPClient http;

  http.begin("https://api.chucknorris.io/jokes/random");
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();

    Serial.println("Joke JSON:");
    Serial.println(payload);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Joke fetched!");
  } else {
    Serial.println("Request failed");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("API failed");
  }

  http.end();
}
