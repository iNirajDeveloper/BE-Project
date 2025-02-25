#include <Adafruit_Fingerprint.h>

// Define UART pins for ESP32 (HardwareSerial)
#define FINGERPRINT_RX 16  // Sensor TX -> ESP32 RX
#define FINGERPRINT_TX 17  // Sensor RX -> ESP32 TX

// Use HardwareSerial for ESP32
HardwareSerial mySerial(1); // UART1 (can use UART2 as well)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial monitor

  // Initialize UART for the fingerprint sensor
  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);

  if (finger.verifyPassword()) {
    Serial.println("Sensor connected!");
    deleteAllFingerprints();
  } else {
    Serial.println("Sensor not found!");
    while (1);
  }
}

void loop() {
  // Nothing here
}

// Function to delete ALL fingerprints
void deleteAllFingerprints() {
  // Method 1: Use emptyDatabase() if supported
  Serial.println("Attempting to delete all fingerprints...");
  uint8_t result = finger.emptyDatabase();

  if (result == FINGERPRINT_OK) {
    Serial.println("Database cleared successfully!");
  } else {
    Serial.println("Failed to clear database. Trying manual deletion...");
    
    // Method 2: Loop through all possible IDs (1-1000)
    for (int id = 1; id <= 1000; id++) {
      finger.deleteModel(id); // Delete each ID
      delay(10); // Small delay to avoid overloading
    }
    Serial.println("All fingerprints deleted manually.");
  }
}