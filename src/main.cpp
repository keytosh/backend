#include <Arduino.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- WiFi ----------
#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "pass"
WiFiMulti wifiMulti;

// ---------- Supabase ----------
const char* supabase_url = "url";
const char* supabase_key = "anon_key";

// ---------- Servo ----------
Servo myServo;
#define SERVO_PIN 14

// ---------- Dummy Load Task ----------
void dummyTask(void* parameter) {
  volatile float x = 0;
  while (true) {
    // Do a bunch of math so ESP32 keeps pulling current
    for (int i = 0; i < 50000; i++) {
      x += sin(i) * cos(i) * tan(i % 90);
    }
    // very tiny pause so it doesn't block watchdog
    vTaskDelay(1); 
  }
}

void setup() {
  Serial.begin(115200);

  // Start dummy load task on core 0 (ignore this if powering directly with electricity)
  xTaskCreatePinnedToCore(
    dummyTask,   // function
    "DummyLoad", // name
    4096,        // stack size
    NULL,        // param
    1,           // priority
    NULL,        // task handle
    0            // core 0
  );

  // OLED init with explicit pins
  Wire.begin(21, 22); // SDA=21, SCL=22
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  // WiFi connect
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("WiFi Connected!");
  display.display();
  delay(1000);
  display.clearDisplay();
  display.display();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // ---------- GET unseen messages ----------
    HTTPClient http;
    String getURL = String(supabase_url) + "?seen=eq.false";
    http.begin(getURL);
    http.addHeader("apikey", supabase_key);
    http.addHeader("Authorization", String("Bearer ") + supabase_key);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(payload);

      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error && doc.size() > 0) {
        int msgId = doc[0]["id"];
        const char* msg = doc[0]["msg"];

        // ---------- Display message ----------
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.println(msg);
        display.display();

        // ---------- Move servo to 90 ----------
        myServo.attach(SERVO_PIN);
        myServo.write(90);
        Serial.println("Servo moved to 90°");

        // ---------- PATCH to set seen = true ----------
        HTTPClient patchHttp;
        String patchURL = String(supabase_url) + "?id=eq." + msgId;
        patchHttp.begin(patchURL);
        patchHttp.addHeader("apikey", supabase_key);
        patchHttp.addHeader("Authorization", String("Bearer ") + supabase_key);
        patchHttp.addHeader("Content-Type", "application/json");

        String body = "{\"seen\":true}";
        int patchCode = patchHttp.sendRequest("PATCH", body);
        Serial.print("PATCH code: ");
        Serial.println(patchCode);
        patchHttp.end();

        // ---------- Wait 1 minute at 90 ----------
        delay(60000);

        // ---------- Return to 0 ----------
        myServo.write(0);
        delay(500);
        myServo.detach();
        Serial.println("Servo returned to 0° and detached");

        // ---------- Clear OLED ----------
        display.clearDisplay();
        display.display();
      } else {
        Serial.println("No unseen messages");
      }
    } else {
      Serial.print("GET Error: ");
      Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    display.clearDisplay();
    display.display();
  }

  delay(3000); // check every 3 seconds
}
