#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>

const char* WIFI_SSID = "Niraj";
const char* WIFI_PASS = "niraj024";

WebServer server(80);

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(800, 600);

bool shouldCapture = false; // Flag to control image capture
String lastCaptureTime = "";

void serveJpg() {
  if (!shouldCapture) {
    server.send(200, "text/plain", "No capture requested.");
    return;
  }

  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);

  // Save the current time as last capture time
  lastCaptureTime = getCurrentTime();
  shouldCapture = false; // Reset the flag after capturing
}

String getCurrentTime() {
  // This is a simple time function for demonstration
  // In a real application, you might want to use NTP to get accurate time
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  minutes %= 60;
  seconds %= 60;
  
  return String(hours) + ":" + String(minutes) + ":" + String(seconds);
}

void handleJpgLo() {
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleJpgMid() {
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}

// New endpoint to trigger image capture
void handleCapture() {
  shouldCapture = true; // Set the flag to capture an image
  server.send(200, "text/plain", "Capture request received.");
}

// New endpoint to check last capture time
void handleLastCapture() {
  server.send(200, "text/plain", lastCaptureTime);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");
  Serial.println("  /capture"); // Endpoint to trigger capture
  Serial.println("  /last-capture"); // Endpoint to check last capture time

  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
  server.on("/capture", handleCapture); // Endpoint to trigger capture
  server.on("/last-capture", handleLastCapture); // Endpoint to check last capture time

  server.begin();
}

void loop() {
  server.handleClient();
}