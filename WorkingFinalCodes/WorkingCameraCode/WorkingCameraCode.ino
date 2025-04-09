#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* WIFI_SSID = "Niraj";
const char* WIFI_PASS = "niraj024";

#define BOTtoken "7359255896:AAGIg4s-50IWMfHFBA29MuK5JYT-Wzr3BDo"
#define CHAT_ID "1626983092"

WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(800, 600);

String lastCaptureTime = "";

void serveJpg() {
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
}

bool captureAndSendToTelegram() {
  // Set to highest resolution for better quality
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
    return false;
  }

  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    return false;
  }

  String response = sendPhotoToTelegram(frame->data(), frame->size());
  if (response.indexOf("true") >= 0) {
    Serial.println("Photo sent successfully to Telegram");
    lastCaptureTime = getCurrentTime();
    return true;
  } else {
    Serial.println("Failed to send photo to Telegram");
    Serial.println(response);
    return false;
  }
}

String sendPhotoToTelegram(uint8_t *buf, size_t len) {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  Serial.println("Connect to " + String(myDomain));

  if (client.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    // Build the multipart form data header
    String head = String("--ESP32-CAM\r\n") +
                 "Content-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" +
                 CHAT_ID + "\r\n" +
                 "--ESP32-CAM\r\n" +
                 "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\n" +
                 "Content-Type: image/jpeg\r\n\r\n";
    
    String tail = "\r\n--ESP32-CAM--\r\n";

    uint16_t imageLen = len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    // Build the HTTP request
    client.print("POST /bot");
    client.print(BOTtoken);
    client.println("/sendPhoto HTTP/1.1");
    client.println("Host: " + String(myDomain));
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=ESP32-CAM");
    client.println();
    client.print(head);
  
    // Send the image data in chunks
    uint8_t *fbBuf = buf;
    size_t fbLen = len;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024 > 0) {
        size_t remainder = fbLen%1024;
        client.write(fbBuf, remainder);
      }
    }  
    
    client.print(tail);
    
    // Wait for response
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
      delay(100);      
      while (client.available()) {
        char c = client.read();
        if (state == true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length() == 0) state = true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length() > 0) break;
    }
    client.stop();
    Serial.println(getBody);
  }
  else {
    getBody = "Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

String getCurrentTime() {
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

void handleCapture() {
  if (captureAndSendToTelegram()) {
    server.send(200, "text/plain", "Photo captured and sent to Telegram successfully");
  } else {
    server.send(500, "text/plain", "Failed to capture and send photo");
  }
}

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
  Serial.println("  /capture");
  Serial.println("  /last-capture");

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
  server.on("/capture", handleCapture);
  server.on("/last-capture", handleLastCapture);

  server.begin();
}

void loop() {
  server.handleClient();
}