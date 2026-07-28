#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBHIDKeyboard.h"

// --- Hardware Setup ---
#define RGB_PIN        48    // Switch to 38 or 21 if your onboard LED stays dark
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

// --- HID Payload Functions (Windows Focused) ---

void openRunMenuWindows() {
  Keyboard.press(KEY_LEFT_GUI); // Windows Key
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500); // Wait for Run dialog to appear
}

void payloadRickRollWindows() {
  setRGBColor(255, 0, 255); // Pink/Magenta for Active Payload Execution
  openRunMenu();
  Keyboard.print("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  Keyboard.write(KEY_RETURN);
}

void payloadCMatrixWindows() {
  setRGBColor(255, 0, 255);
  openRunMenu();
  Keyboard.print("cmd");
  Keyboard.write(KEY_RETURN);
  delay(600); // Wait for command prompt to visually pull up
  
  // Try to install cmatrix if chocolatey is present, or fake a green matrix screen using built-in tree
  Keyboard.print("color 0a && cls && echo Initializing Matrix... && timeout 2 > nul && tree c:\\");
  Keyboard.write(KEY_RETURN);
}

void payloadVolumeUpWindows() {
  setRGBColor(0, 255, 255); // Cyan for Utility actions
  // Rapidly tap the Volume Up multimedia key 10 times
  for(int i = 0; i < 10; i++) {
    Keyboard.press(HID_KEY_VOLUME_UP);
    delay(50);
    Keyboard.release(HID_KEY_VOLUME_UP);
  }
}

void payloadShutdownWindows() {
  setRGBColor(255, 0, 0); // Warning Bright Red
  openRunMenu();
  // Shuts down computer in 60 seconds with a custom comment box popping up
  Keyboard.print("shutdown /s /t 60 /c \"ESP32-S3 Remote Triggered System Power Down!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadMsinfoWindows() {
  setRGBColor(255, 0, 0); // Warning Bright Red
  openRunMenu();
  // Shuts down computer in 60 seconds with a custom comment box popping up
  Keyboard.print("msinfo32");
  Keyboard.write(KEY_RETURN);
}

void payloadSettingsWindows() {
  setRGBColor(255, 0, 0); // Warning Bright Red
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.write('i');
  delay(100);
  Keyboard.releaseAll();

}

void payloadSettingsLinux() {
  setRGBColor(255, 0, 0); // Warning Bright Red
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.write('i');
  delay(100);
  Keyboard.releaseAll();

}


// --- HTML Control Panel ---
const char* CONTROL_PANEL_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>HID Command Deck</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background: #1e1e24; color: #fff; margin-top: 30px; }
    .deck { max-width: 450px; margin: auto; padding: 20px; background: #2a2a35; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.3); }
    h2 { color: #00ffcc; margin-bottom: 25px; }
    .btn { display: block; width: 85%; margin: 15px auto; padding: 15px; font-size: 16px; font-weight: bold; color: #fff; border: none; border-radius: 6px; cursor: pointer; transition: 0.2s; text-decoration: none;}
    .btn-rick { background: #ff007f; }
    .btn-matrix { background: #28a745; }
    .btn-vol { background: #17a2b8; }
    .btn-down { background: #dc3545; }
    .btn:active { transform: scale(0.97); filter: brightness(1.2); }
  </style>
</head>
<body>
  <div class='deck'>
    <h2>🛸 ESP32-S3 HID Control Deck</h2>
    <p>Tap a macro option below to inject Keystroke Payloads into the USB Host.</p>
    
    <h2> Windows </h2>
    <a href='/run/rick' class='btn btn-rick'>Rick Roll</a>
    <a href='/run/matrix' class='btn btn-matrix'>Deploy Matrix Screen</a>
    <a href='/run/volume' class='btn btn-vol'>Crank Volume Up</a>
    <a href='/run/shutdown' class='btn btn-down'> Initiating Shutdown Timer</a>
    <a href='/run/msinfo' class='btn btn-down'> View MSinfo</a>
    <a href='/run/opensettings' class='btn btn-down'> Open Settings</a>

    <h2> Linux </h2>
// Linux 
    <a href='/run/rickascii' class='btn btn-rick'>Rick Roll(ascii)</a>
    <a href='/run/ricklinux' class='btn btn-rick'>Rick Roll</a>
    <a href='/run/cmatrixlinux' class='btn btn-matrix'>Deploy Matrix Screen</a>
    <a href='/run/volumeuplinux' class='btn btn-vol'>Crank Volume Up</a>
    <a href='/run/shutdownlinux' class='btn btn-down'> Shutdown </a>
    <a href='/run/rickAscii' class='btn btn-rick'>Rick Roll</a>

  </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", CONTROL_PANEL_HTML);
}

void setup() {
  // Initialize physical status indicator
  rgbLed.begin();
  setRGBColor(150, 75, 0); // Amber: Starting subsystems

  // Initialize Native USB Core
  USB.begin();
  Keyboard.begin();

  // Setup local standalone Wi-Fi AP Mesh Architecture
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);

  // Setup Web Server API endpoint listeners
  server.on("/", HTTP_GET, handleRoot);
  

  // Windows function call...
  server.on("/run/rick", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying Rick Roll...");
    payloadRickRoll();
  });
  
  server.on("/run/matrix", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying Matrix Stream...");
    payloadCMatrix();
  });
  
  server.on("/run/volume", HTTP_GET, []() {
    server.send(200, "text/plain", "Increasing Volume...");
    payloadVolumeUp();
  });
  
  server.on("/run/shutdown", HTTP_GET, []() {
    server.send(200, "text/plain", "Shutdown command queued.");
    payloadShutdown();
  });

  server.on("/run/opensettings", HTTP_GET, []() {
    server.send(200, "text/plain", "Opening settings...");
    payloadSettings();
  });

  server.on("/run/msinfo", HTTP_GET, []() {
    server.send(200, "text/plain", "Opening msinfo.");
    payloadMsinfo();
  });


  server.begin();
  
  // Set default state tracking color
  setRGBColor(0, 0, 80); // Deep Blue: Ready and monitoring for phone hookups
}

void loop() {
  server.handleClient();

  // Update base status light indicator dynamically based on current client usage
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    lastUpdate = millis();
    int clients = WiFi.softAPgetStationNum();
    if (clients == 0) {
      setRGBColor(0, 0, 80);  // Deep Blue: Empty Room, Waiting
    } else {
      setRGBColor(0, 80, 0);  // Solid Green: Phone client linked, armed and ready
    }
  }
}
