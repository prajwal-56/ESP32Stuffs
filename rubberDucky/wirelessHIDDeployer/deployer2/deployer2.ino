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

// ======================================================
// ================  SHARED HELPERS  ===================
// ======================================================

void openRunMenuWindows() {
  Keyboard.press(KEY_LEFT_GUI); // Windows Key
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500); // Wait for Run dialog to appear
}

// Opens a terminal on most GNOME/Ubuntu-based Linux desktops.
// If your DE doesn't bind Ctrl+Alt+T, change this to whatever your
// distro uses (e.g. Super -> type "terminal" -> Enter).
void openTerminalLinux() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press('t');
  delay(100);
  Keyboard.releaseAll();
  delay(800); // give the terminal time to open
}

// ======================================================
// ================  WINDOWS PAYLOADS  =================
// ======================================================

void payloadRickRollWindows() {
  setRGBColor(255, 0, 255); // Pink/Magenta for Active Payload Execution
  openRunMenuWindows();
  Keyboard.print("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  Keyboard.write(KEY_RETURN);
}

void payloadCMatrixWindows() {
  setRGBColor(255, 0, 255);
  openRunMenuWindows();
  Keyboard.print("cmd");
  Keyboard.write(KEY_RETURN);
  delay(600); // Wait for command prompt to visually pull up

  // Fake a green "matrix" screen using built-in tree (no extra installs needed)
  Keyboard.print("color 0a && cls && echo Initializing Matrix... && timeout 2 > nul && tree c:\\");
  Keyboard.write(KEY_RETURN);
}

void payloadVolumeUpWindows() {
  setRGBColor(0, 255, 255); // Cyan for Utility actions
  for (int i = 0; i < 10; i++) {
    Keyboard.press(HID_KEY_VOLUME_UP);
    delay(50);
    Keyboard.release(HID_KEY_VOLUME_UP);
  }
}

void payloadShutdownWindows() {
  setRGBColor(255, 0, 0); // Warning Bright Red
  openRunMenuWindows();
  // Shuts down computer in 60 seconds with a custom comment box popping up
  Keyboard.print("shutdown /s /t 60 /c \"ESP32-S3 Remote Triggered System Power Down!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadMsinfoWindows() {
  setRGBColor(0, 255, 255);
  openRunMenuWindows();
  Keyboard.print("msinfo32");
  Keyboard.write(KEY_RETURN);
}

void payloadSettingsWindows() {
  setRGBColor(0, 255, 255);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.write('i');
  delay(100);
  Keyboard.releaseAll();
}

// ======================================================
// =================  LINUX PAYLOADS  ===================
// ======================================================

void payloadRickRollLinux() {
  setRGBColor(255, 0, 255);
  openTerminalLinux();
  Keyboard.print("xdg-open https://www.youtube.com/watch?v=dQw4w9WgXcQ &");
  Keyboard.write(KEY_RETURN);
}

// Classic telnet ASCII Star Wars as a fun "ascii" alternative
void payloadRickRollAsciiLinux() {
  setRGBColor(255, 0, 255);
  openTerminalLinux();
  Keyboard.print("telnet towel.blinkenlights.nl");
  Keyboard.write(KEY_RETURN);
}

void payloadCMatrixLinux() {
  setRGBColor(255, 0, 255);
  openTerminalLinux();
  // Uses cmatrix if installed; falls back to a scrolling-number effect if not
  Keyboard.print("cmatrix || while :; do echo $LINENO $RANDOM; done");
  Keyboard.write(KEY_RETURN);
}

void payloadVolumeUpLinux() {
  setRGBColor(0, 255, 255);
  // Multimedia keys are handled at the HID level, same as Windows
  for (int i = 0; i < 10; i++) {
    Keyboard.press(HID_KEY_VOLUME_UP);
    delay(50);
    Keyboard.release(HID_KEY_VOLUME_UP);
  }
}

void payloadShutdownLinux() {
  setRGBColor(255, 0, 0);
  openTerminalLinux();
  // NOTE: most distros need sudo here, so this will likely stop at a
  // password prompt unless the target has passwordless sudo configured.
  Keyboard.print("shutdown -h +1 \"ESP32-S3 Remote Triggered System Power Down!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadSettingsLinux() {
  setRGBColor(0, 255, 255);
  // Opens the GNOME Activities/search overlay, then searches for Settings.
  // On KDE/XFCE/etc you'll want to change this binding.
  Keyboard.press(KEY_LEFT_GUI);
  delay(100);
  Keyboard.releaseAll();
  delay(400);
  Keyboard.print("settings");
  delay(300);
  Keyboard.write(KEY_RETURN);
}

// ======================================================
// ==============  GENERIC KEY / TEXT INPUT =============
// ======================================================

void pressNamedKey(const String& name) {
  if (name == "enter")       { Keyboard.write(KEY_RETURN); }
  else if (name == "tab")    { Keyboard.write(KEY_TAB); }
  else if (name == "esc")    { Keyboard.write(KEY_ESC); }
  else if (name == "space")  { Keyboard.write(' '); }
  else if (name == "backspace") { Keyboard.write(KEY_BACKSPACE); }
  else if (name == "win") {
    Keyboard.press(KEY_LEFT_GUI);
    delay(80);
    Keyboard.releaseAll();
  }
  else if (name == "alttab") {
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press(KEY_TAB);
    delay(80);
    Keyboard.releaseAll();
  }
  else if (name == "altf4") {
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press(KEY_F4);
    delay(80);
    Keyboard.releaseAll();
  }
}

// ======================================================
// ==================  HTML CONTROL PANEL ===============
// ======================================================

const char* CONTROL_PANEL_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>HID Command Deck</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background: #1e1e24; color: #fff; margin: 0; padding: 20px 0 60px; }
    .deck { max-width: 460px; margin: auto; padding: 20px; background: #2a2a35; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.3); }
    h2 { color: #00ffcc; margin-bottom: 15px; }
    h3 { color: #9adfff; margin: 25px 0 10px; border-top: 1px solid #444; padding-top: 15px; }
    .btn { display: block; width: 85%; margin: 12px auto; padding: 14px; font-size: 15px; font-weight: bold; color: #fff; border: none; border-radius: 6px; cursor: pointer; transition: 0.2s; text-decoration: none; }
    .btn-rick { background: #ff007f; }
    .btn-matrix { background: #28a745; }
    .btn-vol { background: #17a2b8; }
    .btn-down { background: #dc3545; }
    .btn:active { transform: scale(0.97); filter: brightness(1.2); }
    .keyrow { display: flex; flex-wrap: wrap; justify-content: center; gap: 8px; margin: 10px auto; width: 90%; }
    .keyrow .btn { width: auto; flex: 1 1 70px; margin: 0; padding: 12px 6px; font-size: 13px; background: #444a5a; }
    textarea { width: 88%; min-height: 70px; border-radius: 6px; border: none; padding: 10px; font-size: 14px; resize: vertical; }
    .typebtns { display: flex; justify-content: center; gap: 10px; margin-top: 10px; }
    .typebtns button { flex: 1; max-width: 160px; padding: 12px; border: none; border-radius: 6px; font-weight: bold; color: #fff; background: #ff9500; cursor: pointer; }
  </style>
</head>
<body>
  <div class='deck'>
    <h2>&#128757; ESP32-S3 HID Control Deck</h2>
    <p>Tap a macro, send a live key, or type a custom string.</p>

    <h3>Custom Text Injection</h3>
    <textarea id='typeBox' placeholder='Type something to send to the target machine...'></textarea>
    <div class='typebtns'>
      <button onclick='sendType(false)'>Type</button>
      <button onclick='sendType(true)'>Type + Enter</button>
    </div>

    <h3>Generic Keys</h3>
    <div class='keyrow'>
      <button class='btn' onclick="sendKey('win')">Win</button>
      <button class='btn' onclick="sendKey('enter')">Enter</button>
      <button class='btn' onclick="sendKey('tab')">Tab</button>
      <button class='btn' onclick="sendKey('esc')">Esc</button>
      <button class='btn' onclick="sendKey('space')">Space</button>
      <button class='btn' onclick="sendKey('backspace')">Backspace</button>
      <button class='btn' onclick="sendKey('alttab')">Alt+Tab</button>
      <button class='btn' onclick="sendKey('altf4')">Alt+F4</button>
    </div>

    <h3>Windows Macros</h3>
    <a href='/run/rick' class='btn btn-rick'>Rick Roll</a>
    <a href='/run/matrix' class='btn btn-matrix'>Deploy Matrix Screen</a>
    <a href='/run/volume' class='btn btn-vol'>Crank Volume Up</a>
    <a href='/run/shutdown' class='btn btn-down'>Initiate Shutdown Timer</a>
    <a href='/run/msinfo' class='btn btn-down'>View MSinfo</a>
    <a href='/run/opensettings' class='btn btn-down'>Open Settings</a>

    <h3>Linux Macros</h3>
    <a href='/run/ricklinux' class='btn btn-rick'>Rick Roll</a>
    <a href='/run/rickascii' class='btn btn-rick'>Rick Roll (ASCII)</a>
    <a href='/run/cmatrixlinux' class='btn btn-matrix'>Deploy Matrix Screen</a>
    <a href='/run/volumeuplinux' class='btn btn-vol'>Crank Volume Up</a>
    <a href='/run/shutdownlinux' class='btn btn-down'>Shutdown</a>
    <a href='/run/settingslinux' class='btn btn-down'>Open Settings</a>
  </div>

  <script>
    function sendKey(name) {
      fetch('/key/' + name);
    }
    function sendType(withEnter) {
      const text = document.getElementById('typeBox').value;
      if (!text) return;
      fetch('/type', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'text=' + encodeURIComponent(text) + '&enter=' + (withEnter ? '1' : '0')
      });
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", CONTROL_PANEL_HTML);
}

void handleType() {
  if (server.hasArg("text")) {
    String text = server.arg("text");
    setRGBColor(255, 149, 0); // Orange while actively typing
    Keyboard.print(text);
    if (server.hasArg("enter") && server.arg("enter") == "1") {
      Keyboard.write(KEY_RETURN);
    }
    setRGBColor(0, 80, 0);
  }
  server.send(200, "text/plain", "OK");
}

void handleKey() {
  String path = server.uri(); // e.g. /key/enter
  String keyName = path.substring(String("/key/").length());
  pressNamedKey(keyName);
  server.send(200, "text/plain", "OK");
}

// ======================================================
// =======================  SETUP  ======================
// ======================================================

void setup() {
  rgbLed.begin();
  setRGBColor(150, 75, 0); // Amber: Starting subsystems

  USB.begin();
  Keyboard.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);

  server.on("/", HTTP_GET, handleRoot);

  // --- Custom text injection ---
  server.on("/type", HTTP_POST, handleType);

  // --- Generic keys: /key/<name> ---
  server.on("/key/enter", HTTP_GET, handleKey);
  server.on("/key/tab", HTTP_GET, handleKey);
  server.on("/key/esc", HTTP_GET, handleKey);
  server.on("/key/space", HTTP_GET, handleKey);
  server.on("/key/backspace", HTTP_GET, handleKey);
  server.on("/key/win", HTTP_GET, handleKey);
  server.on("/key/alttab", HTTP_GET, handleKey);
  server.on("/key/altf4", HTTP_GET, handleKey);

  // --- Windows macros ---
  server.on("/run/rick", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying Rick Roll...");
    payloadRickRollWindows();
  });
  server.on("/run/matrix", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying Matrix Stream...");
    payloadCMatrixWindows();
  });
  server.on("/run/volume", HTTP_GET, []() {
    server.send(200, "text/plain", "Increasing Volume...");
    payloadVolumeUpWindows();
  });
  server.on("/run/shutdown", HTTP_GET, []() {
    server.send(200, "text/plain", "Shutdown command queued.");
    payloadShutdownWindows();
  });
  server.on("/run/opensettings", HTTP_GET, []() {
    server.send(200, "text/plain", "Opening settings...");
    payloadSettingsWindows();
  });
  server.on("/run/msinfo", HTTP_GET, []() {
    server.send(200, "text/plain", "Opening msinfo.");
    payloadMsinfoWindows();
  });

  // --- Linux macros ---
  server.on("/run/ricklinux", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying Rick Roll (Linux)...");
    payloadRickRollLinux();
  });
  server.on("/run/rickascii", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying ASCII Star Wars...");
    payloadRickRollAsciiLinux();
  });
  server.on("/run/cmatrixlinux", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying Matrix Stream (Linux)...");
    payloadCMatrixLinux();
  });
  server.on("/run/volumeuplinux", HTTP_GET, []() {
    server.send(200, "text/plain", "Increasing Volume (Linux)...");
    payloadVolumeUpLinux();
  });
  server.on("/run/shutdownlinux", HTTP_GET, []() {
    server.send(200, "text/plain", "Shutdown command queued (Linux).");
    payloadShutdownLinux();
  });
  server.on("/run/settingslinux", HTTP_GET, []() {
    server.send(200, "text/plain", "Opening settings (Linux)...");
    payloadSettingsLinux();
  });

  server.begin();

  setRGBColor(0, 0, 80); // Deep Blue: Ready and monitoring for client hookups
}

void loop() {
  server.handleClient();

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
