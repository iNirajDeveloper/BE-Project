#include <WiFi.h>
#include <HTTPClient.h>

// Replace with your Wi-Fi credentials
const char* ssid = "Niraj";
const char* password = "niraj024";

// Replace with the IP address of your ESP32-CAM
const char* serverUrl = "http://192.168.30.76/capture";

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    if (command == "capture") {
      HTTPClient http;
      http.begin(serverUrl);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        Serial.println("Image captured!");
      } else {
        Serial.println("Failed to capture image");
      }
      http.end();
    }
  }
}