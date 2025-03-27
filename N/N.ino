#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <Adafruit_Fingerprint.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>  // Added for ESP32-CAM communication

#define WIFI_SSID "Niraj"
#define WIFI_PASSWORD "niraj024"

#define API_KEY "AIzaSyCfDXcLGvuus4uIUJVvKawkapXoxTMwcns"
#define FIREBASE_PROJECT_ID "attendaceesp"
#define USER_EMAIL "cnp7832@gmail.com"
#define USER_PASSWORD "niraj12345"

#define BOTtoken "7359255896:AAGIg4s-50IWMfHFBA29MuK5JYT-Wzr3BDo"
#define CHAT_ID "1626983092"

// ESP32-CAM server URL - Update this with your ESP32-CAM's IP address
#define CAMERA_SERVER_URL "http://192.168.252.76/capture"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Initialize a hardware serial port for the fingerprint sensor
HardwareSerial mySerial(2); // UART2
Adafruit_Fingerprint finger(&mySerial);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "in.pool.ntp.org", 19800, 60000);

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

bool captureImageFromCamera() {
  HTTPClient http;
  
  Serial.println("Attempting to capture image from ESP32-CAM...");
  http.begin(CAMERA_SERVER_URL);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Image captured successfully from ESP32-CAM");
    http.end();
    return true;
  } else {
    Serial.printf("Failed to capture image. HTTP error code: %d\n", httpCode);
    http.end();
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found!");
  } else {
    Serial.println("Fingerprint sensor not detected.");
    while (1) delay(1);
  }

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

  timeClient.begin();
  timeClient.setTimeOffset(19800);
  timeClient.setUpdateInterval(60000);

  int ntpRetryCount = 0;
  while (!timeClient.forceUpdate() && ntpRetryCount < 5) {
    Serial.println("Retrying NTP update...");
    delay(1000);
    ntpRetryCount++;
  }
  
  if (ntpRetryCount >= 5) {
    Serial.println("Warning: Failed to initialize NTP client.");
  } else {
    Serial.println("NTP time updated successfully.");
    Serial.print("Current time: ");
    Serial.println(timeClient.getFormattedTime());
  }

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  Serial.println("Place your finger on the sensor for verification...");
  int fingerprintID = verifyFingerprint();

  if (fingerprintID > 0) {
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
        bool imageCaptured = captureImageFromCamera();

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
          unsigned long epochTime = timeClient.getEpochTime();
          time_t rawtime = (time_t)epochTime;
          struct tm *ptm = gmtime(&rawtime);

          int currentYear = ptm->tm_year + 1900;
          int currentMonth = ptm->tm_mon + 1;
          int currentDay = ptm->tm_mday;

          String formattedDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(currentDay);
          String formattedTime = timeClient.getFormattedTime();

          // Create a unique document ID for the attendance record
          String attendanceDocPath = "AttendanceRecords/" + String(epochTime) + "_" + String(fingerprintID);
          
          // Create the attendance record to send to Firebase
          FirebaseJson attendanceRecord;
          
          // Create the document fields
          FirebaseJson content;
          
          // Add fields to the document
          content.set("fields/name/stringValue", name);
          content.set("fields/fingerprintID/integerValue", String(fingerprintID));
          content.set("fields/date/stringValue", formattedDate);
          content.set("fields/time/stringValue", formattedTime);
          content.set("fields/timestamp/integerValue", String(epochTime));
          content.set("fields/imageCaptured/booleanValue", imageCaptured ? "true" : "false");

          Serial.println("Sending attendance data to Firebase...");
          
          // Set the document in Firestore
          if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", attendanceDocPath.c_str(), content.raw())) {
            Serial.println("Attendance record successfully saved to Firebase!");
            Serial.printf("Document path: %s\n", fbdo.payload().c_str());
            
            // Send Telegram notification
            String message = "Attendance Recorded!\n";
            message += "Name: " + name + "\n";
            message += "Date: " + formattedDate + "\n";
            message += "Time: " + formattedTime + "\n";
            message += "Image " + String(imageCaptured ? "captured successfully" : "capture failed");
            
            if (bot.sendMessage(CHAT_ID, message, "")) {
              Serial.println("Telegram notification sent successfully");
            } else {
              Serial.println("Failed to send Telegram notification");
            }
          } else {
            Serial.println("Failed to save attendance record to Firebase");
            Serial.println("Reason: " + fbdo.errorReason());
          }
        } else {
          Serial.println("Failed to update NTP time after multiple attempts.");
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

  delay(5000);
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
    return finger.fingerID;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Fingerprint not found in database.");
    return -1;
  } else {
    Serial.println("Error during fingerprint search.");
    return -1;
  }
}