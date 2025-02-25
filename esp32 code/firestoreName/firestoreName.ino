// Include required libraries
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <Adafruit_Fingerprint.h>

// Define WiFi credentials
#define WIFI_SSID "Niraj"
#define WIFI_PASSWORD "niraj0244"

// Define Firebase API Key, Project ID, and user credentials
#define API_KEY "AIzaSyCfDXcLGvuus4uIUJVvKawkapXoxTMwcns"
#define FIREBASE_PROJECT_ID "attendaceesp"
#define USER_EMAIL "cnp7832@gmail.com"
#define USER_PASSWORD "niraj12345"

// Define Firebase Data object, Firebase authentication, and configuration
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Initialize a hardware serial port for the fingerprint sensor
HardwareSerial mySerial(2); // UART2
Adafruit_Fingerprint finger(&mySerial);

// Variable to track the next available slot ID
int nextSlotID = 1;

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
  // Start fingerprint enrollment
  Serial.println("Waiting for a valid fingerprint...");
  int id = enrollFingerprint();

  if (id > 0) {
    // Ask for the name of the fingerprint holder
    Serial.println("Please enter the name of the fingerprint holder:");
    while (!Serial.available()) {
      delay(100); // Wait for user input
    }
    String name = Serial.readStringUntil('\n');
    name.trim(); // Remove any extra whitespace

    // Save fingerprint details to Firebase
    String documentPath = "FingerprintData/Finger" + String(id);

    // Create a FirebaseJson object for storing data
    FirebaseJson content;
    content.set("fields/Name/stringValue", name);

    Serial.print("Saving fingerprint data to Firebase... ");
    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw())) {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
    } else {
      Serial.println(fbdo.errorReason());
    }

    // Increment the slot ID for the next fingerprint
    nextSlotID++;
  }

  delay(10000);  // Delay for 10 seconds
}

int enrollFingerprint() {
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

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image");
    return -1;
  }

  Serial.println("Remove your finger...");
  delay(2000);

  Serial.println("Place the same finger again...");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image");
    return -1;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Error creating model");
    return -1;
  }

  // Store the fingerprint in the next available slot
  p = finger.storeModel(nextSlotID);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint enrolled successfully");
    return nextSlotID;  // Return the ID of the stored fingerprint
  } else {
    Serial.println("Error storing fingerprint");
    return -1;
  }
}