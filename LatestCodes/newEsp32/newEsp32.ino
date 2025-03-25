#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <Adafruit_Fingerprint.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>

#define WIFI_SSID "Niraj"
#define WIFI_PASSWORD "niraj024"

#define API_KEY "AIzaSyCfDXcLGvuus4uIUJVvKawkapXoxTMwcns"
#define FIREBASE_PROJECT_ID "attendaceesp"
#define USER_EMAIL "cnp7832@gmail.com"
#define USER_PASSWORD "niraj12345"

#define BOTtoken "7359255896:AAGIg4s-50IWMfHFBA29MuK5JYT-Wzr3BDo"
#define CHAT_ID "1626983092"

// ESP32-CAM server URL
#define CAMERA_SERVER_URL "http://192.168.252.76/capture"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Initialize a hardware serial port for the fingerprint sensor
HardwareSerial mySerial(2); // UART2
Adafruit_Fingerprint finger(&mySerial);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "in.pool.ntp.org", 19800, 60000); // Using India-specific NTP server

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Initialize UART2 for the fingerprint sensor
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX2 = GPIO16, TX2 = GPIO17

  // Initialize the fingerprint sensor
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found!");
  } else {
    Serial.println("Fingerprint sensor not detected. Please check connections.");
    while (1) delay(1);
  }

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  int wifiRetryCount = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetryCount < 10) {
    Serial.print(".");
    delay(500);
    wifiRetryCount++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi. Restarting...");
    ESP.restart();
  }
  
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Initialize NTP client with improved settings
  timeClient.begin();
  timeClient.setTimeOffset(19800); // Explicitly set IST offset
  timeClient.setUpdateInterval(60000);

  // Force initial NTP update with retry logic
  int ntpRetryCount = 0;
  while (!timeClient.forceUpdate() && ntpRetryCount < 5) {
    Serial.println("Retrying NTP update...");
    delay(1000);
    ntpRetryCount++;
  }
  
  if (ntpRetryCount >= 5) {
    Serial.println("Warning: Failed to initialize NTP client. Time may be inaccurate.");
  } else {
    Serial.println("NTP time updated successfully.");
    Serial.print("Current time: ");
    Serial.println(timeClient.getFormattedTime());
  }

  // Print Firebase client version
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // Assign the API key
  config.api_key = API_KEY;

  // Assign the user sign-in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the callback function for the long-running token generation task
  config.token_status_callback = tokenStatusCallback;

  // Begin Firebase with configuration and authentication
  Firebase.begin(&config, &auth);

  // Reconnect to Wi-Fi if necessary
  Firebase.reconnectWiFi(true);

  // Initialize Telegram Bot
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
}

bool captureImage() {
  HTTPClient http;
  http.begin(CAMERA_SERVER_URL);
  int httpCode = http.GET();
  http.end();
  
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Image captured successfully!");
    return true;
  } else {
    Serial.println("Failed to capture image");
    return false;
  }
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(2000); // Wait for reconnection
    return; // Skip this iteration
  }

  // Start fingerprint verification
  Serial.println("Place your finger on the sensor for verification...");
  int fingerprintID = verifyFingerprint();

  if (fingerprintID > 0) {
    // Retrieve the name from Firebase
    String documentPath = "FingerprintData/Finger" + String(fingerprintID);
    Serial.print("Retrieving name for fingerprint ID ");
    Serial.println(fingerprintID);

    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str())) {
      FirebaseJson payload;
      payload.setJsonData(fbdo.payload().c_str());

      FirebaseJsonData nameData;
      payload.get(nameData, "fields/Name/stringValue");

      if (nameData.success) {
        String name = nameData.stringValue;
        Serial.println("Fingerprint verified!");

        // Capture image from ESP32-CAM
        if (captureImage()) {
          // Update the time from NTP server with retry logic
          bool timeUpdated = false;
          for (int i = 0; i < 3; i++) {
            if (timeClient.update() || timeClient.forceUpdate()) {
              timeUpdated = true;
              break;
            }
            delay(1000);
          }

          if (timeUpdated) {
            Serial.println("NTP time updated successfully.");

            // Get the epoch time
            unsigned long epochTime = timeClient.getEpochTime();

            // Convert epoch time to a readable date
            time_t rawtime = (time_t)epochTime;
            struct tm *ptm = gmtime(&rawtime);

            int currentYear = ptm->tm_year + 1900;
            int currentMonth = ptm->tm_mon + 1;
            int currentDay = ptm->tm_mday;

            String formattedDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(currentDay);
            String formattedTime = timeClient.getFormattedTime();

            Serial.print("Name: ");
            Serial.println(name);
            Serial.print("Date: ");
            Serial.println(formattedDate);
            Serial.print("Time: ");
            Serial.println(formattedTime);

            // Send attendance data to Telegram
            String message = "Attendance Recorded!\n";
            message += "Name: " + name + "\n";
            message += "Date: " + formattedDate + "\n";
            message += "Time: " + formattedTime + "\n";
            message += "Image captured and saved.";
            
            if (bot.sendMessage(CHAT_ID, message, "")) {
              Serial.println("Telegram notification sent successfully");
            } else {
              Serial.println("Failed to send Telegram notification");
            }
          } else {
            Serial.println("Failed to update NTP time after multiple attempts.");
          }
        } else {
          Serial.println("Failed to capture image from ESP32-CAM");
        }
      } else {
        Serial.println("Name not found in Firebase.");
      }
    } else {
      Serial.println("Failed to retrieve document from Firebase.");
      Serial.println("Reason: " + fbdo.errorReason());
    }
  } else {
    Serial.println("Fingerprint not recognized.");
  }

  delay(5000);  // Delay for 5 seconds before next verification
}

int verifyFingerprint() {
  int p = -1;
  Serial.println("Place your finger on the sensor...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        delay(100);
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image");
    return -1;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint found!");
    return finger.fingerID;  // Return the ID of the matched fingerprint
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Fingerprint not found in database.");
    return -1;
  } else {
    Serial.println("Error during fingerprint search.");
    return -1;
  }
}