#include <Arduino.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- WiFi ----------
#define WIFI_SSID "Airtel_536"
#define WIFI_PASSWORD "Rekhasharma@121168"
WiFiMulti wifiMulti;

// ---------- Supabase ----------
const char* supabase_url = "URL"; // replace with your own url
const char* supabase_key = "KEY"; // replace with your anon key

void setup() {
  Serial.begin(921600);

  // OLED init
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Connecting to WiFi...");
  display.display();

  // WiFi
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
  }

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WiFi Connected!");
  display.display();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {

    // ---------- GET unseen messages ----------
    HTTPClient http;
    String getURL = String(supabase_url) + "?seen=eq.false"; // fetch unseen
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
        display.setCursor(0,0);
        display.println(msg);
        display.display();

        // wait 30 seconds
        delay(30000);

        // ---------- PATCH to set seen = true ----------
        HTTPClient patchHttp;
        String patchURL = String(supabase_url) + "?id=eq." + msgId;
        patchHttp.begin(patchURL);
        patchHttp.addHeader("apikey", supabase_key);
        patchHttp.addHeader("Authorization", String("Bearer ") + supabase_key);
        patchHttp.addHeader("Content-Type", "application/json");

        String body = "{\"seen\":true}";
        int patchCode = patchHttp.sendRequest("PATCH", body);
        Serial.print("PATCH code: "); Serial.println(patchCode);
        patchHttp.end();

        // ---------- Turn screen off ----------
        display.clearDisplay();
        display.display();
      } else {
        // No unseen messages, keep screen off
        display.clearDisplay();
        display.display();
      }

    } else {
      Serial.print("GET Error: "); Serial.println(httpCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    display.clearDisplay();
    display.display();
  }

  delay(10000); // check every 10 seconds for new messages (make this 8-10 mins in production to save power)
}
