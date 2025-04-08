// Include required libraries
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <Adafruit_Fingerprint.h>

// Define WiFi credentials
#define WIFI_SSID "Niraj"
#define WIFI_PASSWORD "niraj024"

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

  // Get the next available slot ID from Firestore
  getNextAvailableSlotID();
}

void loop() {
  Serial.println("\nMain Menu:");
  Serial.println("1. View existing fingerprints");
  Serial.println("2. Add new fingerprint (auto ID)");
  Serial.println("3. Add fingerprint to specific ID");
  Serial.println("4. Delete a fingerprint");
  Serial.println("5. Delete all fingerprints");
  Serial.println("Enter your choice (1-5):");

  // Wait for user input
  while (!Serial.available()) {
    delay(100);
  }

  char choice = Serial.read();
  Serial.println(choice); // Echo the choice

  switch (choice) {
    case '1':
      viewExistingFingerprints();
      break;
    case '2':
      addNewFingerprint(true); // Auto ID
      break;
    case '3':
      addNewFingerprint(false); // Specific ID
      break;
    case '4':
      deleteFingerprint();
      break;
    case '5':
      deleteAllFingerprints();
      break;
    default:
      Serial.println("Invalid choice. Please enter 1-5.");
      break;
  }

  // Clear any remaining input
  while (Serial.available()) {
    Serial.read();
  }
}

void viewExistingFingerprints() {
  Serial.println("\nFetching existing fingerprints from Firestore...");

  // List all documents in the FingerprintData collection
  if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", "FingerprintData", 20, "", "", "", false)) {
    FirebaseJson payload;
    FirebaseJsonData jsonData;
    payload.setJsonData(fbdo.payload().c_str());
    
    // Get the documents array
    if (payload.get(jsonData, "documents")) {
      FirebaseJsonArray documents;
      jsonData.getArray(documents);
      
      Serial.println("\nExisting Fingerprints:");
      Serial.println("----------------------");

      for (size_t i = 0; i < documents.size(); i++) {
        FirebaseJsonData docData;
        if (documents.get(docData, i)) {
          FirebaseJson doc;
          docData.getJSON(doc);
          
          // Get document path (ID)
          FirebaseJsonData nameData;
          if (doc.get(nameData, "name")) {
            String documentPath = nameData.stringValue;
            
            // Extract the fingerprint ID from the path
            int lastSlash = documentPath.lastIndexOf('/');
            String fingerprintID = documentPath.substring(lastSlash + 1);
            
            // Get the name field
            FirebaseJsonData fieldsData;
            if (doc.get(fieldsData, "fields")) {
              FirebaseJson fields;
              fieldsData.getJSON(fields);
              
              FirebaseJsonData nameValueData;
              if (fields.get(nameValueData, "Name/stringValue")) {
                String name = nameValueData.stringValue;
                
                Serial.printf("ID: %s, Name: %s\n", fingerprintID.c_str(), name.c_str());
              }
            }
          }
        }
      }
    }
  } else {
    Serial.println("Failed to fetch documents: " + fbdo.errorReason());
  }
}

void addNewFingerprint(bool autoID) {
  int id;
  
  if (autoID) {
    id = nextSlotID;
    Serial.printf("\nStarting automatic enrollment at ID %d...\n", id);
  } else {
    Serial.println("\nEnter the specific ID number to enroll to:");
    
    // Clear any existing input
    while (Serial.available()) {
      Serial.read();
    }
    
    // Wait for user input with timeout
    unsigned long startTime = millis();
    String input = "";
    bool inputReceived = false;
    
    while (!inputReceived) {
      if (Serial.available()) {
        input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() > 0) {
          inputReceived = true;
        }
      }
      
      // Timeout after 30 seconds if no input received
      if (millis() - startTime > 30000) {
        Serial.println("\nTimeout: No ID received. Enrollment cancelled.");
        return;
      }
      delay(100);
    }
    
    id = input.toInt();
    if (id <= 0) {
      Serial.println("Invalid ID. Please enter a positive number.");
      return;
    }
    Serial.printf("\nStarting enrollment at ID %d...\n", id);
  }

  // Perform the fingerprint enrollment
  if (enrollFingerprint(id)) {
    // Clear any leftover serial input
    while (Serial.available()) {
      Serial.read();
    }
    
    // Ask for the name of the fingerprint holder
    Serial.println("\nPlease enter the name of the fingerprint holder:");
    Serial.println("(Type the name and press Enter)");
    
    // Wait for user input with timeout
    unsigned long startTime = millis();
    String name = "";
    bool nameReceived = false;
    
    while (!nameReceived) {
      if (Serial.available()) {
        name = Serial.readStringUntil('\n');
        name.trim();
        if (name.length() > 0) {
          nameReceived = true;
        } else {
          Serial.println("Invalid name. Please try again:");
        }
      }
      
      // Timeout after 30 seconds if no input received
      if (millis() - startTime > 30000) {
        Serial.println("\nTimeout: No name received. Fingerprint will not be saved to Firestore.");
        // Delete the fingerprint from sensor if name wasn't received
        finger.deleteModel(id);
        return;
      }
    }

    // Save fingerprint details to Firebase
    String documentPath = "FingerprintData/Finger" + String(id);

    // Create a FirebaseJson object for storing data
    FirebaseJson content;
    content.set("fields/Name/stringValue", name);

    Serial.print("\nSaving fingerprint data to Firebase... ");
    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw())) {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      if (autoID && id >= nextSlotID) {
        nextSlotID = id + 1; // Update the next available slot ID
      }
    } else {
      Serial.println(fbdo.errorReason());
      // Delete the fingerprint from sensor if Firestore save failed
      finger.deleteModel(id);
    }
  }
}

void deleteFingerprint() {
  viewExistingFingerprints(); // Show existing fingerprints first
  
  Serial.println("\nDelete Fingerprint");
  Serial.println("Enter the fingerprint ID to delete (or '0' to cancel):");
  
  // Clear any existing input
  while (Serial.available()) {
    Serial.read();
  }
  
  // Wait for user input with timeout
  unsigned long startTime = millis();
  String input = "";
  bool inputReceived = false;
  
  while (!inputReceived) {
    if (Serial.available()) {
      input = Serial.readStringUntil('\n');
      input.trim();
      inputReceived = true;
    }
    
    // Timeout after 30 seconds if no input received
    if (millis() - startTime > 30000) {
      Serial.println("\nTimeout: No input received. Deletion cancelled.");
      return;
    }
    delay(100);
  }
  
  if (input == "0") {
    Serial.println("Deletion cancelled.");
    return;
  }
  
  int id = input.toInt();
  if (id <= 0) {
    Serial.println("Invalid ID. Please enter a positive number.");
    return;
  }

  // Delete from fingerprint sensor
  Serial.printf("\nAttempting to delete fingerprint ID %d...\n", id);
  int p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.printf("Fingerprint ID %d deleted from sensor.\n", id);
  } else {
    Serial.printf("Error deleting fingerprint ID %d from sensor (Error code: %d).\n", id, p);
  }

  // Delete from Firestore
  String documentPath = "FingerprintData/Finger" + String(id);
  Serial.printf("Deleting document %s from Firestore... ", documentPath.c_str());
  
  if (Firebase.Firestore.deleteDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str())) {
    Serial.println("success.");
  } else {
    Serial.println("failed: " + fbdo.errorReason());
  }

  // Update nextSlotID if we deleted the highest ID
  if (id == nextSlotID - 1) {
    nextSlotID--;
    Serial.printf("Next available slot ID updated to %d\n", nextSlotID);
  }
}

void deleteAllFingerprints() {
  Serial.println("\nWARNING: This will delete ALL fingerprints!");
  Serial.println("Are you sure? (y/n)");
  
  // Wait for user confirmation
  while (!Serial.available()) {
    delay(100);
  }
  
  char confirm = Serial.read();
  if (tolower(confirm) != 'y') {
    Serial.println("Deletion cancelled.");
    return;
  }

  // Empty the fingerprint database on the sensor
  int p = finger.emptyDatabase();
  if (p == FINGERPRINT_OK) {
    Serial.println("All fingerprints deleted from sensor.");
  } else {
    Serial.println("Error deleting fingerprints from sensor.");
  }

  // Delete all documents from Firestore collection
  if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", "FingerprintData", 20, "", "", "", false)) {
    FirebaseJson payload;
    FirebaseJsonData jsonData;
    payload.setJsonData(fbdo.payload().c_str());
    
    if (payload.get(jsonData, "documents")) {
      FirebaseJsonArray documents;
      jsonData.getArray(documents);
      
      for (size_t i = 0; i < documents.size(); i++) {
        FirebaseJsonData docData;
        if (documents.get(docData, i)) {
          FirebaseJson doc;
          docData.getJSON(doc);
          
          FirebaseJsonData nameData;
          if (doc.get(nameData, "name")) {
            String documentPath = nameData.stringValue;
            Serial.printf("Deleting %s... ", documentPath.c_str());
            
            if (Firebase.Firestore.deleteDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str())) {
              Serial.println("success.");
            } else {
              Serial.println("failed: " + fbdo.errorReason());
            }
          }
        }
      }
    }
  } else {
    Serial.println("Failed to fetch documents: " + fbdo.errorReason());
  }

  // Reset nextSlotID
  nextSlotID = 1;
  Serial.println("All fingerprints deleted. Next available slot ID reset to 1.");
}

void getNextAvailableSlotID() {
  // List all documents in the FingerprintData collection to find the highest ID
  if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", "FingerprintData", 20, "", "", "", false)) {
    FirebaseJson payload;
    FirebaseJsonData jsonData;
    payload.setJsonData(fbdo.payload().c_str());
    
    // Get the documents array
    if (payload.get(jsonData, "documents")) {
      FirebaseJsonArray documents;
      jsonData.getArray(documents);

      int maxID = 0;
      for (size_t i = 0; i < documents.size(); i++) {
        FirebaseJsonData docData;
        if (documents.get(docData, i)) {
          FirebaseJson doc;
          docData.getJSON(doc);
          
          // Get document path (ID)
          FirebaseJsonData nameData;
          if (doc.get(nameData, "name")) {
            String documentPath = nameData.stringValue;
            
            // Extract the fingerprint ID from the path
            int lastSlash = documentPath.lastIndexOf('/');
            String fingerprintIDStr = documentPath.substring(lastSlash + 1);
            
            // Remove "Finger" prefix if present
            if (fingerprintIDStr.startsWith("Finger")) {
              fingerprintIDStr = fingerprintIDStr.substring(6);
            }
            
            int currentID = fingerprintIDStr.toInt();
            if (currentID > maxID) {
              maxID = currentID;
            }
          }
        }
      }
      
      nextSlotID = maxID + 1;
      Serial.printf("Next available slot ID: %d\n", nextSlotID);
    }
  } else {
    Serial.println("Failed to fetch documents to determine next slot ID. Starting from 1.");
    nextSlotID = 1;
  }
}

int enrollFingerprint(int id) {
  int p = -1;
  
  // First scan
  Serial.println("Place your finger on the sensor for the first scan...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("First image taken successfully");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        delay(100);
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return 0;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        return 0;
      default:
        Serial.println("Unknown error");
        return 0;
    }
  }

  // Convert first image
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting first image");
    return 0;
  }
  Serial.println("First image converted successfully");

  // Remove finger
  Serial.println("Remove your finger...");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  // Second scan
  Serial.println("Place the same finger again for the second scan...");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Second image taken successfully");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        delay(100);
        break;
      default:
        Serial.println("Error during second scan");
        return 0;
    }
  }

  // Convert second image
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting second image");
    return 0;
  }
  Serial.println("Second image converted successfully");

  // Create model
  Serial.println("Creating fingerprint model...");
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Error: Fingerprints didn't match");
    Serial.println("Please try again with the same finger");
    return 0;
  }
  Serial.println("Fingerprint model created successfully");

  // Store model
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error storing fingerprint model");
    return 0;
  }
  Serial.printf("Fingerprint enrolled successfully with ID: %d\n", id);
  return 1;
}