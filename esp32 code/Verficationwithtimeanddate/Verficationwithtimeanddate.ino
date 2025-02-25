#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <Adafruit_Fingerprint.h>
#include <NTPClient.h>
#include <WiFiUdp.h>


#define WIFI_SSID "Niraj"
#define WIFI_PASSWORD "niraj0244"

#define API_KEY "AIzaSyCfDXcLGvuus4uIUJVvKawkapXoxTMwcns"
#define FIREBASE_PROJECT_ID "attendaceesp"
#define USER_EMAIL "cnp7832@gmail.com"
#define USER_PASSWORD "niraj12345"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Initialize a hardware serial port for the fingerprint sensor
HardwareSerial mySerial(2); // UART2
Adafruit_Fingerprint finger(&mySerial);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // 19800 = IST (UTC+5:30)

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
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Initialize NTP client
  timeClient.begin();

  // Force update NTP time
  if (timeClient.forceUpdate()) {
    Serial.println("NTP time updated successfully.");
  } else {
    Serial.println("Failed to update NTP time.");
  }

  // Print Firebase client version
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // Assign the API key
  config.api_key = API_KEY;

  // Assign the user sign-in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the callback function for the long-running token generation task
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  // Begin Firebase with configuration and authentication
  Firebase.begin(&config, &auth);

  // Reconnect to Wi-Fi if necessary
  Firebase.reconnectWiFi(true);
}

void loop() {
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

        // Update the time from NTP server
        if (timeClient.update()) {
          Serial.println("NTP time updated successfully.");

          // Get the epoch time
          unsigned long epochTime = timeClient.getEpochTime();

          // Convert epoch time to a readable date
          time_t rawtime = (time_t)epochTime; // Cast to time_t
          struct tm *ptm = gmtime(&rawtime); // Use gmtime with time_t

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
        } else {
          Serial.println("Failed to update NTP time.");
        }
      } else {
        Serial.println("Name not found in Firebase.");
      }
    } else {
      Serial.println("Failed to retrieve document from Firebase.");
      Serial.println(fbdo.errorReason());
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