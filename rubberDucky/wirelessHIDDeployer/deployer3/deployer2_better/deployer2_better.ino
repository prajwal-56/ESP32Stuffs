#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

// --- Hardware Setup ---
#define RGB_PIN        48    // Switch to 38 or 21 if your onboard LED stays dark
#define NUMPIXELS       1
const char* AP_SSID = "ESP32-S3 Command Center";

Adafruit_NeoPixel rgbLed(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl; // Media keys (volume etc.) live on a
                                       // separate HID usage page from the
                                       // keyboard, hence the separate class.

// --- Functional RGB Helper ---
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// ======================================================
// ==============  MODIFIER LATCH STATE  ================
// ======================================================
// Ctrl/Shift/Alt/Win are "latched": one click holds the key down,
// another click releases it. This lets you build combos from the
// web UI, e.g. latch Ctrl, then type a letter, then unlatch Ctrl.

bool ctrlHeld  = false;
bool shiftHeld = false;
bool altHeld   = false;
bool guiHeld   = false;

void toggleModifier(const String& name, String& stateOut) {
  if (name == "ctrl") {
    ctrlHeld = !ctrlHeld;
    if (ctrlHeld) Keyboard.press(KEY_LEFT_CTRL); else Keyboard.release(KEY_LEFT_CTRL);
    stateOut = ctrlHeld ? "on" : "off";
  } else if (name == "shift") {
    shiftHeld = !shiftHeld;
    if (shiftHeld) Keyboard.press(KEY_LEFT_SHIFT); else Keyboard.release(KEY_LEFT_SHIFT);
    stateOut = shiftHeld ? "on" : "off";
  } else if (name == "alt") {
    altHeld = !altHeld;
    if (altHeld) Keyboard.press(KEY_LEFT_ALT); else Keyboard.release(KEY_LEFT_ALT);
    stateOut = altHeld ? "on" : "off";
  } else if (name == "gui") {
    guiHeld = !guiHeld;
    if (guiHeld) Keyboard.press(KEY_LEFT_GUI); else Keyboard.release(KEY_LEFT_GUI);
    stateOut = guiHeld ? "on" : "off";
  }
}

void releaseAllModifiers() {
  ctrlHeld = shiftHeld = altHeld = guiHeld = false;
  Keyboard.releaseAll();
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
  setRGBColor(200, 200, 200);
  openRunMenuWindows();
  Keyboard.print("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  Keyboard.write(KEY_RETURN);
}

void payloadCMatrixWindows() {
  setRGBColor(200, 200, 200);
  openRunMenuWindows();
  Keyboard.print("cmd");
  Keyboard.write(KEY_RETURN);
  delay(600); // Wait for command prompt to visually pull up

  // Fake a green "matrix" screen using built-in tree (no extra installs needed)
  Keyboard.print("color 0a && cls && echo Initializing Matrix... && timeout 2 > nul && tree c:\\");
  Keyboard.write(KEY_RETURN);
}

void payloadVolumeUpWindows() {
  setRGBColor(120, 120, 120);
  for (int i = 0; i < 10; i++) {
    ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
    delay(50);
    ConsumerControl.release();
  }
}

void payloadShutdownWindows() {
  setRGBColor(180, 40, 40); // Warning
  openRunMenuWindows();
  // Shuts down computer in 60 seconds with a custom comment box popping up
  Keyboard.print("shutdown /s /t 60 /c \"ESP32-S3 Remote Triggered System Power Down!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadRebootWindows() {
  setRGBColor(180, 40, 40); // Warning
  openRunMenuWindows();
  Keyboard.print("shutdown /r /t 60 /c \"ESP32-S3 Remote Triggered Reboot!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadMsinfoWindows() {
  setRGBColor(120, 120, 120);
  openRunMenuWindows();
  Keyboard.print("msinfo32");
  Keyboard.write(KEY_RETURN);
}

void payloadSettingsWindows() {
  setRGBColor(120, 120, 120);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.write('i');
  delay(100);
  Keyboard.releaseAll();
}

// ======================================================
// =================  LINUX PAYLOADS  ===================
// ======================================================

void payloadRickRollLinux() {
  setRGBColor(200, 200, 200);
  openTerminalLinux();
  Keyboard.print("xdg-open https://www.youtube.com/watch?v=dQw4w9WgXcQ &");
  Keyboard.write(KEY_RETURN);
}

// Classic telnet ASCII Star Wars as a fun "ascii" alternative
void payloadRickRollAsciiLinux() {
  setRGBColor(200, 200, 200);
  openTerminalLinux();
  Keyboard.print("curl ascii.live/rick");
  Keyboard.write(KEY_RETURN);
}

void payloadCMatrixLinux() {
  setRGBColor(200, 200, 200);
  openTerminalLinux();
  // Uses cmatrix if installed; falls back to a scrolling-number effect if not
  Keyboard.print("cmatrix || while :; do echo $LINENO $RANDOM; done");
  Keyboard.write(KEY_RETURN);
}

void payloadVolumeUpLinux() {
  setRGBColor(120, 120, 120);
  // Consumer control usage codes are OS-agnostic; same call as Windows.
  for (int i = 0; i < 10; i++) {
    ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
    delay(50);
    ConsumerControl.release();
  }
}

void payloadShutdownLinux() {
  setRGBColor(180, 40, 40);
  openTerminalLinux();
  // NOTE: most distros need sudo here, so this will likely stop at a
  // password prompt unless the target has passwordless sudo configured.
  Keyboard.print("shutdown -h +1 \"ESP32-S3 Remote Triggered System Power Down!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadRebootLinux() {
  setRGBColor(180, 40, 40);
  openTerminalLinux();
  // Same sudo caveat as shutdown above.
  Keyboard.print("shutdown -r +1 \"ESP32-S3 Remote Triggered Reboot!\"");
  Keyboard.write(KEY_RETURN);
}

void payloadSettingsLinux() {
  setRGBColor(120, 120, 120);
  // Opens the GNOME Activities/search overlay, then searches for Settings.
  // On KDE/XFCE/etc you'll want to change this binding.
  Keyboard.press(KEY_LEFT_GUI);
  delay(100);
  Keyboard.releaseAll();
  delay(400);
  Keyboard.print("system settings");
  delay(300);
  Keyboard.write(KEY_RETURN);
}

void speedtype(){
  setRGBColor(120, 120, 120);
  // Opens the GNOME Activities/search overlay, then searches for Settings.
  // On KDE/XFCE/etc you'll want to change this binding.
  openTerminalLinux();
  delay(400);
  Keyboard.print("ttyper -w 50");
  delay(200);
  Keyboard.write(KEY_RETURN);
  delay(500);

  // actual typing things 
  Keyboard.print("Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies nec, pellentesque eu, pretium quis, sem. Nulla consequat massa quis enim. Donec pede justo, fringilla vel, aliquet nec, vulputate eget, arcu. In enim justo, rhoncus ut, imperdiet a, venenatis");
  delay(200);
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
  else if (name == "wintap") {
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
    * { box-sizing: border-box; }
    body { font-family: -apple-system, 'Segoe UI', Helvetica, Arial, sans-serif; text-align: center; background: #111; color: #eaeaea; margin: 0; padding: 24px 0 60px; }
    .deck { max-width: 440px; margin: auto; padding: 24px; background: #1a1a1a; border: 1px solid #2b2b2b; border-radius: 8px; }
    h2 { font-size: 18px; font-weight: 600; letter-spacing: 0.3px; margin: 0 0 4px; }
    .sub { color: #888; font-size: 13px; margin-bottom: 20px; }
    h3 { color: #999; font-size: 12px; text-transform: uppercase; letter-spacing: 0.6px; margin: 24px 0 10px; border-top: 1px solid #2b2b2b; padding-top: 18px; text-align: left; }
    .btn { display: block; width: 100%; margin: 8px 0; padding: 12px; font-size: 14px; font-weight: 500; color: #eaeaea; border: 1px solid #333; background: #202020; border-radius: 5px; cursor: pointer; }
    .btn:active { background: #2a2a2a; }
    .btn-danger { border-color: #5a2626; color: #e0a0a0; }
    .btn-danger:active { background: #2a1414; }
    .row { display: flex; flex-wrap: wrap; gap: 8px; }
    .row .btn { flex: 1 1 auto; width: auto; min-width: 64px; padding: 10px 6px; font-size: 13px; }
    .btn.active { background: #eaeaea; color: #111; border-color: #eaeaea; }
    textarea { width: 100%; min-height: 64px; border-radius: 5px; border: 1px solid #333; background: #0d0d0d; color: #eaeaea; padding: 10px; font-size: 14px; resize: vertical; font-family: inherit; }
    .typebtns { display: flex; gap: 8px; margin-top: 8px; }
    .typebtns .btn { flex: 1; margin: 0; }
    section { text-align: left; }
  </style>
</head>
<body>
  <div class='deck'>
    <h2>HID Command Deck</h2>
    <div class='sub'>ESP32-S3 remote keyboard control</div>

    <section>
      <h3>Custom Text</h3>
      <textarea id='typeBox' placeholder='Text to send to the target machine'></textarea>
      <div class='typebtns'>
        <button class='btn' onclick='sendType(false)'>Type</button>
        <button class='btn' onclick='sendType(true)'>Type + Enter</button>
      </div>
    </section>

    <section>
      <h3>Modifiers (latching — tap to hold, tap again to release)</h3>
      <div class='row'>
        <button class='btn' id='mod-ctrl'  onclick="toggleMod('ctrl')">Ctrl</button>
        <button class='btn' id='mod-shift' onclick="toggleMod('shift')">Shift</button>
        <button class='btn' id='mod-alt'   onclick="toggleMod('alt')">Alt</button>
        <button class='btn' id='mod-gui'   onclick="toggleMod('gui')">Win</button>
      </div>
      <div class='row'>
        <button class='btn' onclick="releaseMods()">Release All</button>
      </div>
    </section>

    <section>
      <h3>Keys</h3>
      <div class='row'>
        <button class='btn' onclick="sendKey('enter')">Enter</button>
        <button class='btn' onclick="sendKey('tab')">Tab</button>
        <button class='btn' onclick="sendKey('esc')">Esc</button>
        <button class='btn' onclick="sendKey('space')">Space</button>
      </div>
      <div class='row'>
        <button class='btn' onclick="sendKey('backspace')">Backspace</button>
        <button class='btn' onclick="sendKey('wintap')">Win (tap)</button>
        <button class='btn' onclick="sendKey('alttab')">Alt+Tab</button>
        <button class='btn' onclick="sendKey('altf4')">Alt+F4</button>
      </div>
    </section>

    <section>
      <h3>Windows</h3>
      <button class='btn' onclick="run('/run/rick')">Rick Roll</button>
      <button class='btn' onclick="run('/run/matrix')">Matrix Screen</button>
      <button class='btn' onclick="run('/run/volume')">Volume Up</button>
      <button class='btn' onclick="run('/run/msinfo')">System Info</button>
      <button class='btn' onclick="run('/run/opensettings')">Open Settings</button>
      <button class='btn btn-danger' onclick="confirmRun('/run/shutdown', 'shut down this Windows machine')">Shutdown</button>
      <button class='btn btn-danger' onclick="confirmRun('/run/reboot', 'reboot this Windows machine')">Reboot</button>
    </section>

    <section>
      <h3>Linux</h3>
      <button class='btn' onclick="run('/run/ricklinux')">Rick Roll</button>
      <button class='btn' onclick="run('/run/rickascii')">Rick Roll (ASCII)</button>
      <button class='btn' onclick="run('/run/cmatrixlinux')">Matrix Screen</button>
      <button class='btn' onclick="run('/run/volumeuplinux')">Volume Up</button>
      <button class='btn' onclick="run('/run/settingslinux')">Open Settings</button>
      <button class='btn' onclick="run('/run/speedtype')">Speed type flex</button>
      <button class='btn btn-danger' onclick="confirmRun('/run/shutdownlinux', 'shut down this Linux machine')">Shutdown</button>
      <button class='btn btn-danger' onclick="confirmRun('/run/rebootlinux', 'reboot this Linux machine')">Reboot</button>

    </section>
  </div>

  <script>
    function run(path) { fetch(path); }

    function confirmRun(path, label) {
      if (confirm('Are you sure you want to ' + label + '?')) {
        fetch(path);
      }
    }

    function sendKey(name) { fetch('/key/' + name); }

    function toggleMod(name) {
      fetch('/modkey/' + name)
        .then(r => r.text())
        .then(state => {
          const el = document.getElementById('mod-' + name);
          el.classList.toggle('active', state.trim() === 'on');
        });
    }

    function releaseMods() {
      fetch('/modkey/releaseall').then(() => {
        ['ctrl', 'shift', 'alt', 'gui'].forEach(n => {
          document.getElementById('mod-' + n).classList.remove('active');
        });
      });
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
    Keyboard.print(text);
    if (server.hasArg("enter") && server.arg("enter") == "1") {
      Keyboard.write(KEY_RETURN);
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleKey() {
  String path = server.uri(); // e.g. /key/enter
  String keyName = path.substring(String("/key/").length());
  pressNamedKey(keyName);
  server.send(200, "text/plain", "OK");
}

void handleModKey() {
  String path = server.uri(); // e.g. /modkey/ctrl
  String modName = path.substring(String("/modkey/").length());
  if (modName == "releaseall") {
    releaseAllModifiers();
    server.send(200, "text/plain", "off");
    return;
  }
  String state;
  toggleModifier(modName, state);
  server.send(200, "text/plain", state);
}

// ======================================================
// =======================  SETUP  ======================
// ======================================================

void setup() {
  rgbLed.begin();
  setRGBColor(150, 75, 0); // Amber: Starting subsystems

  USB.begin();
  Keyboard.begin();
  ConsumerControl.begin();

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
  server.on("/key/wintap", HTTP_GET, handleKey);
  server.on("/key/alttab", HTTP_GET, handleKey);
  server.on("/key/altf4", HTTP_GET, handleKey);

  // --- Latching modifiers: /modkey/<name> ---
  server.on("/modkey/ctrl", HTTP_GET, handleModKey);
  server.on("/modkey/shift", HTTP_GET, handleModKey);
  server.on("/modkey/alt", HTTP_GET, handleModKey);
  server.on("/modkey/gui", HTTP_GET, handleModKey);
  server.on("/modkey/releaseall", HTTP_GET, handleModKey);

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
  server.on("/run/reboot", HTTP_GET, []() {
    server.send(200, "text/plain", "Reboot command queued.");
    payloadRebootWindows();
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
  server.on("/run/rebootlinux", HTTP_GET, []() {
    server.send(200, "text/plain", "Reboot command queued (Linux).");
    payloadRebootLinux();
  });
  server.on("/run/settingslinux", HTTP_GET, []() {
    server.send(200, "text/plain", "Opening settings (Linux)...");
    payloadSettingsLinux();
  });

  // speed typing - 10 words:
  server.on("/run/speedtype", HTTP_GET, []() {
    server.send(200, "text/plain", "Deploying speed type ttyper (Linux)...");
    speedtype();
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
