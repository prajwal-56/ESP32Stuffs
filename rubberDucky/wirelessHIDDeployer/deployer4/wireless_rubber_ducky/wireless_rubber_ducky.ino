#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include <ArduinoJson.h>

// ─── Hardware ────────────────────────────────────────────────────────────────
#define RGB_PIN    48
#define NUMPIXELS   1
const char* AP_SSID = "Not Rubber Ducky";
const char*  AP_PASS  = "FS0ciety#";

Adafruit_NeoPixel rgbLed(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

USBHIDKeyboard     Keyboard;
USBHIDConsumerControl ConsumerControl;

// ─── RGB state machine ───────────────────────────────────────────────────────
enum LedMode {
  LED_IDLE,       // slow pulse blue   – no clients
  LED_READY,      // solid green       – client connected
  LED_ACTIVE,     // fast white blink  – payload firing
  LED_TYPING,     // solid orange      – text injection
  LED_DANGER,     // fast red blink    – shutdown / reboot
};
LedMode  ledMode       = LED_IDLE;
uint32_t ledLastMs     = 0;
bool     ledBlinkState = false;
uint32_t ledReturnMs   = 0;   // when to return to READY after a timed event
bool     ledTimed      = false;

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// Call this to temporarily switch mode for ~durationMs then revert to READY
void ledEvent(LedMode mode, uint32_t durationMs = 2000) {
  ledMode    = mode;
  ledTimed   = true;
  ledReturnMs = millis() + durationMs;
}

void updateLed() {
  uint32_t now = millis();

  if (ledTimed && now >= ledReturnMs) {
    ledTimed = false;
    ledMode  = (WiFi.softAPgetStationNum() > 0) ? LED_READY : LED_IDLE;
  }

  switch (ledMode) {
    case LED_IDLE: {
      // slow sine-ish pulse on blue using millis
      float t   = (now % 3000) / 3000.0f;
      uint8_t b = (uint8_t)(40 + 40 * sin(t * 2 * 3.14159f));
      setRGB(0, 0, b);
      break;
    }
    case LED_READY:
      setRGB(0, 60, 0);
      break;
    case LED_ACTIVE:
      if (now - ledLastMs > 120) { ledLastMs = now; ledBlinkState = !ledBlinkState; }
      setRGB(ledBlinkState ? 200 : 0, ledBlinkState ? 200 : 0, ledBlinkState ? 200 : 0);
      break;
    case LED_TYPING:
      setRGB(180, 80, 0);
      break;
    case LED_DANGER:
      if (now - ledLastMs > 120) { ledLastMs = now; ledBlinkState = !ledBlinkState; }
      setRGB(ledBlinkState ? 220 : 0, 0, 0);
      break;
  }
}

// ─── Modifier latch state ────────────────────────────────────────────────────
bool ctrlHeld = false, shiftHeld = false, altHeld = false, guiHeld = false;

void applyModifiers() {
  if (ctrlHeld)  Keyboard.press(KEY_LEFT_CTRL);  else Keyboard.release(KEY_LEFT_CTRL);
  if (shiftHeld) Keyboard.press(KEY_LEFT_SHIFT); else Keyboard.release(KEY_LEFT_SHIFT);
  if (altHeld)   Keyboard.press(KEY_LEFT_ALT);   else Keyboard.release(KEY_LEFT_ALT);
  if (guiHeld)   Keyboard.press(KEY_LEFT_GUI);   else Keyboard.release(KEY_LEFT_GUI);
}

void releaseAllModifiers() {
  ctrlHeld = shiftHeld = altHeld = guiHeld = false;
  Keyboard.releaseAll();
}

// ─── Keyboard spammer state machine (replaces the while(1) version) ──────────
bool     spamActive   = false;
const char spamChars[] = "1234567890qwertyuiopasdfghjklzxcvbnm[;,.'] ";
const int  spamLen     = sizeof(spamChars) - 1;
int        spamWordLeft = 0;

void spamTick() {
  if (!spamActive) return;
  uint32_t deadline = millis() + 8;
  while (spamActive && millis() < deadline) {
    if (spamWordLeft <= 0) {
      spamWordLeft = random(3, 16);
      Keyboard.write(' ');
      Keyboard.write(KEY_RETURN);
    }
    Keyboard.write(spamChars[random(0, spamLen)]);
    spamWordLeft--;
    delayMicroseconds(200);
  }
}

// ─── Shared helpers ──────────────────────────────────────────────────────────
void openRunWin() {
  Keyboard.press(KEY_LEFT_GUI); Keyboard.press('r');
  delay(100); Keyboard.releaseAll(); delay(500);
}

void openTermLinux() {
  Keyboard.press(KEY_LEFT_CTRL); Keyboard.press(KEY_LEFT_ALT); Keyboard.press('t');
  delay(100); Keyboard.releaseAll(); delay(800);
}

// ─── Windows payloads ────────────────────────────────────────────────────────
void plRickWin()     { ledEvent(LED_ACTIVE); openRunWin(); Keyboard.print("https://www.youtube.com/watch?v=dQw4w9WgXcQ"); Keyboard.write(KEY_RETURN); }
void plMatrixWin()   { ledEvent(LED_ACTIVE); openRunWin(); Keyboard.print("cmd"); Keyboard.write(KEY_RETURN); delay(600); Keyboard.print("color 0a && cls && echo Initializing Matrix... && timeout 2 > nul && tree c:\\"); Keyboard.write(KEY_RETURN); }
void plVolUpWin()    { ledEvent(LED_ACTIVE, 800); for (int i=0;i<10;i++){ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);delay(50);ConsumerControl.release();} }
void plMsinfoWin()   { ledEvent(LED_ACTIVE); openRunWin(); Keyboard.print("msinfo32"); Keyboard.write(KEY_RETURN); }
void plSettingsWin() { ledEvent(LED_ACTIVE); Keyboard.press(KEY_LEFT_GUI); Keyboard.write('i'); delay(100); Keyboard.releaseAll(); }
void plShutdownWin() { ledEvent(LED_DANGER, 3000); openRunWin(); Keyboard.print("shutdown /s /t 60 /c \"ESP32-S3 Remote Triggered Shutdown\""); Keyboard.write(KEY_RETURN); }
void plRebootWin()   { ledEvent(LED_DANGER, 3000); openRunWin(); Keyboard.print("shutdown /r /t 60 /c \"ESP32-S3 Remote Triggered Reboot\""); Keyboard.write(KEY_RETURN); }

// ─── Linux payloads ──────────────────────────────────────────────────────────
void plRickLinux()     { ledEvent(LED_ACTIVE); openTermLinux(); Keyboard.print("xdg-open https://www.youtube.com/watch?v=dQw4w9WgXcQ &"); Keyboard.write(KEY_RETURN); }
void plRickAscii()     { ledEvent(LED_ACTIVE); openTermLinux(); Keyboard.print("curl ascii.live/rick"); Keyboard.write(KEY_RETURN); }
void plMatrixLinux()   { ledEvent(LED_ACTIVE); openTermLinux(); Keyboard.print("cmatrix || while :; do echo $LINENO $RANDOM; done"); Keyboard.write(KEY_RETURN); }
void plVolUpLinux()    { ledEvent(LED_ACTIVE, 800); for (int i=0;i<10;i++){ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);delay(50);ConsumerControl.release();} }
void plSettingsLinux() { ledEvent(LED_ACTIVE); Keyboard.press(KEY_LEFT_GUI); delay(100); Keyboard.releaseAll(); delay(400); Keyboard.print("system settings"); delay(300); Keyboard.write(KEY_RETURN); }
void plShutdownLinux() { ledEvent(LED_DANGER, 3000); openTermLinux(); Keyboard.print("shutdown -h +1"); Keyboard.write(KEY_RETURN); }
void plRebootLinux()   { ledEvent(LED_DANGER, 3000); openTermLinux(); Keyboard.print("shutdown -r +1"); Keyboard.write(KEY_RETURN); }

// ─── Named single key press ──────────────────────────────────────────────────
void pressNamed(const String& k) {
  if      (k=="enter")     Keyboard.write(KEY_RETURN);
  else if (k=="tab")       Keyboard.write(KEY_TAB);
  else if (k=="esc")       Keyboard.write(KEY_ESC);
  else if (k=="space")     Keyboard.write(' ');
  else if (k=="backspace") Keyboard.write(KEY_BACKSPACE);
  else if (k=="delete")    Keyboard.write(KEY_DELETE);
  else if (k=="insert")    Keyboard.write(KEY_INSERT);
  else if (k=="home")      Keyboard.write(KEY_HOME);
  else if (k=="end")       Keyboard.write(KEY_END);
  else if (k=="pgup")      Keyboard.write(KEY_PAGE_UP);
  else if (k=="pgdn")      Keyboard.write(KEY_PAGE_DOWN);
  else if (k=="up")        Keyboard.write(KEY_UP_ARROW);
  else if (k=="down")      Keyboard.write(KEY_DOWN_ARROW);
  else if (k=="left")      Keyboard.write(KEY_LEFT_ARROW);
  else if (k=="right")     Keyboard.write(KEY_RIGHT_ARROW);
  else if (k=="f1")        Keyboard.write(KEY_F1);
  else if (k=="f2")        Keyboard.write(KEY_F2);
  else if (k=="f3")        Keyboard.write(KEY_F3);
  else if (k=="f4")        Keyboard.write(KEY_F4);
  else if (k=="f5")        Keyboard.write(KEY_F5);
  else if (k=="f6")        Keyboard.write(KEY_F6);
  else if (k=="f7")        Keyboard.write(KEY_F7);
  else if (k=="f8")        Keyboard.write(KEY_F8);
  else if (k=="f9")        Keyboard.write(KEY_F9);
  else if (k=="f10")       Keyboard.write(KEY_F10);
  else if (k=="f11")       Keyboard.write(KEY_F11);
  else if (k=="f12")       Keyboard.write(KEY_F12);
  else if (k=="wintap")    { Keyboard.press(KEY_LEFT_GUI); delay(80); Keyboard.releaseAll(); }
  else if (k=="alttab")    { Keyboard.press(KEY_LEFT_ALT); Keyboard.press(KEY_TAB); delay(80); Keyboard.releaseAll(); }
  else if (k=="altf4")     { Keyboard.press(KEY_LEFT_ALT); Keyboard.press(KEY_F4); delay(80); Keyboard.releaseAll(); }
  else if (k=="ctrlaltdel"){ Keyboard.press(KEY_LEFT_CTRL); Keyboard.press(KEY_LEFT_ALT); Keyboard.press(KEY_DELETE); delay(80); Keyboard.releaseAll(); }
  else if (k.length()==1)  Keyboard.write((char)k[0]); // single char fallback
}

// ─── HTML ────────────────────────────────────────────────────────────────────
const char* HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>HID Command Deck</title>
<style>
  :root {
    --bg:    #0e0e0e;
    --sur:   #181818;
    --sur2:  #222;
    --bdr:   #2e2e2e;
    --txt:   #e8e8e8;
    --dim:   #888;
    --acc:   #e8e8e8;
    --red:   #c0392b;
    --redbdr:#7a1f1f;
    --nav-h: 60px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
  body { background: var(--bg); color: var(--txt); font-family: -apple-system, 'Segoe UI', sans-serif; font-size: 14px; padding-bottom: calc(var(--nav-h) + 12px); }

  /* ── Nav ── */
  nav { position: fixed; bottom: 0; left: 0; right: 0; height: var(--nav-h); background: var(--sur); border-top: 1px solid var(--bdr); display: flex; z-index: 100; }
  nav button { flex: 1; background: none; border: none; color: var(--dim); display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 3px; font-size: 10px; cursor: pointer; padding: 6px 0; transition: color .15s; }
  nav button.active { color: var(--txt); }
  nav button svg { width: 20px; height: 20px; stroke: currentColor; fill: none; stroke-width: 1.8; stroke-linecap: round; stroke-linejoin: round; }

  /* ── Pages ── */
  .page { display: none; padding: 16px; max-width: 480px; margin: auto; }
  .page.active { display: block; }
  .page-title { font-size: 17px; font-weight: 600; margin-bottom: 4px; }
  .page-sub   { color: var(--dim); font-size: 12px; margin-bottom: 18px; }

  /* ── Sections ── */
  .section { margin-bottom: 20px; }
  .section-label { color: var(--dim); font-size: 11px; text-transform: uppercase; letter-spacing: .7px; margin-bottom: 8px; }

  /* ── Buttons ── */
  .btn { width: 100%; padding: 12px 14px; background: var(--sur); border: 1px solid var(--bdr); border-radius: 7px; color: var(--txt); font-size: 14px; font-weight: 500; cursor: pointer; text-align: left; display: block; margin-bottom: 8px; transition: background .1s; }
  .btn:active { background: var(--sur2); }
  .btn-danger { border-color: var(--redbdr); color: #e07070; }
  .btn-danger:active { background: #1a0a0a; }
  .btn-sm { padding: 9px 10px; font-size: 13px; }
  .btn.on { background: var(--acc); color: #111; border-color: var(--acc); }

  /* ── Grid ── */
  .grid { display: grid; gap: 7px; margin-bottom: 8px; }
  .g2  { grid-template-columns: 1fr 1fr; }
  .g3  { grid-template-columns: 1fr 1fr 1fr; }
  .g4  { grid-template-columns: 1fr 1fr 1fr 1fr; }
  .g5  { grid-template-columns: repeat(5,1fr); }
  .grid .btn { margin: 0; text-align: center; padding: 11px 4px; }

  /* ── Modifier row ── */
  .mod-row { display: grid; grid-template-columns: repeat(4,1fr); gap: 7px; margin-bottom: 10px; }
  .mod-row .btn { text-align: center; padding: 11px 4px; font-size: 13px; font-weight: 600; }

  /* ── Combo quick-fire ── */
  .combo-row { display: grid; grid-template-columns: repeat(5,1fr); gap: 7px; margin-bottom: 8px; }
  .combo-row .btn { text-align: center; padding: 9px 2px; font-size: 12px; }

  /* ── Text area ── */
  textarea { width: 100%; min-height: 72px; background: #0d0d0d; border: 1px solid var(--bdr); border-radius: 7px; color: var(--txt); padding: 10px; font-size: 14px; resize: vertical; font-family: inherit; }
  .row2 { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-top: 8px; }

  /* ── Delay slider ── */
  .slider-row { display: flex; align-items: center; gap: 10px; margin-top: 8px; }
  .slider-row input { flex: 1; accent-color: var(--acc); }
  .slider-val { color: var(--dim); font-size: 12px; min-width: 44px; text-align: right; }

  /* ── Builder ── */
  .step-list { background: var(--sur); border: 1px solid var(--bdr); border-radius: 7px; min-height: 60px; padding: 8px; margin-bottom: 10px; }
  .step-item { display: flex; align-items: center; justify-content: space-between; background: var(--sur2); border-radius: 5px; padding: 8px 10px; margin-bottom: 6px; font-size: 13px; }
  .step-item:last-child { margin-bottom: 0; }
  .step-del { background: none; border: none; color: var(--dim); font-size: 16px; cursor: pointer; padding: 0 4px; }
  .step-empty { color: var(--dim); font-size: 13px; text-align: center; padding: 12px 0; }
  .picker-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 7px; }
  .picker-grid .btn { font-size: 13px; padding: 10px; text-align: center; }
  input[type=text], input[type=number] { width: 100%; background: #0d0d0d; border: 1px solid var(--bdr); border-radius: 6px; color: var(--txt); padding: 10px; font-size: 14px; font-family: inherit; margin-top: 6px; }
  .builder-name-row { display: flex; gap: 8px; margin-bottom: 10px; }
  .builder-name-row input { flex: 1; margin: 0; }
  .saved-list { }
  .saved-item { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; }
  .saved-item .btn { flex: 1; margin: 0; }
  .saved-del { background: none; border: 1px solid var(--redbdr); border-radius: 5px; color: #e07070; padding: 10px 12px; cursor: pointer; font-size: 14px; }

  /* ── Log ── */
  .log-box { background: #0d0d0d; border: 1px solid var(--bdr); border-radius: 7px; min-height: 90px; max-height: 200px; overflow-y: auto; padding: 10px; font-family: 'SF Mono', 'Fira Mono', monospace; font-size: 12px; color: #aaa; }
  .log-entry { margin-bottom: 3px; }
  .log-entry span { color: #555; margin-right: 6px; }

  /* ── Status ── */
  .stat-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 16px; }
  .stat-card { background: var(--sur); border: 1px solid var(--bdr); border-radius: 8px; padding: 14px; }
  .stat-val  { font-size: 22px; font-weight: 600; margin-bottom: 2px; }
  .stat-lbl  { color: var(--dim); font-size: 11px; text-transform: uppercase; letter-spacing: .5px; }

  /* ── Divider ── */
  hr { border: none; border-top: 1px solid var(--bdr); margin: 18px 0; }
</style>
</head>
<body>

<!-- ════════════════════ PAGE: KEYS ════════════════════ -->
<div class="page active" id="pg-keys">
  <div class="page-title">Keys</div>
  <div class="page-sub">Tap modifiers to latch, then tap a key</div>

  <div class="section">
    <div class="section-label">Modifiers</div>
    <div class="mod-row">
      <button class="btn" id="m-ctrl"  onclick="toggleMod('ctrl')">Ctrl</button>
      <button class="btn" id="m-shift" onclick="toggleMod('shift')">Shift</button>
      <button class="btn" id="m-alt"   onclick="toggleMod('alt')">Alt</button>
      <button class="btn" id="m-gui"   onclick="toggleMod('gui')">Win</button>
    </div>
    <button class="btn btn-sm" onclick="releaseMods()" style="text-align:center;color:var(--dim)">Release All Modifiers</button>
  </div>

  <div class="section">
    <div class="section-label">Quick Combos</div>
    <div class="combo-row">
      <button class="btn" onclick="combo('ctrl','a')">Ctrl+A</button>
      <button class="btn" onclick="combo('ctrl','c')">Ctrl+C</button>
      <button class="btn" onclick="combo('ctrl','v')">Ctrl+V</button>
      <button class="btn" onclick="combo('ctrl','z')">Ctrl+Z</button>
      <button class="btn" onclick="combo('ctrl','s')">Ctrl+S</button>
    </div>
    <div class="combo-row">
      <button class="btn" onclick="combo('ctrl','x')">Ctrl+X</button>
      <button class="btn" onclick="combo('ctrl','y')">Ctrl+Y</button>
      <button class="btn" onclick="sendKey('alttab')">Alt+Tab</button>
      <button class="btn" onclick="sendKey('altf4')">Alt+F4</button>
      <button class="btn" onclick="sendKey('wintap')">Win</button>
    </div>
  </div>

  <div class="section">
    <div class="section-label">Control Keys</div>
    <div class="grid g4">
      <button class="btn" onclick="sendKey('enter')">Enter</button>
      <button class="btn" onclick="sendKey('tab')">Tab</button>
      <button class="btn" onclick="sendKey('esc')">Esc</button>
      <button class="btn" onclick="sendKey('space')">Space</button>
      <button class="btn" onclick="sendKey('backspace')">Bksp</button>
      <button class="btn" onclick="sendKey('delete')">Del</button>
      <button class="btn" onclick="sendKey('insert')">Ins</button>
      <button class="btn" onclick="sendKey('ctrlaltdel')">C+A+D</button>
    </div>
  </div>

  <div class="section">
    <div class="section-label">Navigation</div>
    <div class="grid g3" style="margin-bottom:7px">
      <button class="btn" onclick="sendKey('home')">Home</button>
      <button class="btn" onclick="sendKey('end')">End</button>
      <div></div>
      <button class="btn" onclick="sendKey('pgup')">PgUp</button>
      <button class="btn" onclick="sendKey('pgdn')">PgDn</button>
      <div></div>
    </div>
    <div class="grid g3">
      <div></div>
      <button class="btn" onclick="sendKey('up')">↑</button>
      <div></div>
      <button class="btn" onclick="sendKey('left')">←</button>
      <button class="btn" onclick="sendKey('down')">↓</button>
      <button class="btn" onclick="sendKey('right')">→</button>
    </div>
  </div>

  <div class="section">
    <div class="section-label">Function Keys</div>
    <div class="grid g4">
      <button class="btn" onclick="sendKey('f1')">F1</button>
      <button class="btn" onclick="sendKey('f2')">F2</button>
      <button class="btn" onclick="sendKey('f3')">F3</button>
      <button class="btn" onclick="sendKey('f4')">F4</button>
      <button class="btn" onclick="sendKey('f5')">F5</button>
      <button class="btn" onclick="sendKey('f6')">F6</button>
      <button class="btn" onclick="sendKey('f7')">F7</button>
      <button class="btn" onclick="sendKey('f8')">F8</button>
      <button class="btn" onclick="sendKey('f9')">F9</button>
      <button class="btn" onclick="sendKey('f10')">F10</button>
      <button class="btn" onclick="sendKey('f11')">F11</button>
      <button class="btn" onclick="sendKey('f12')">F12</button>
    </div>
  </div>

  <div class="section">
    <div class="section-label">Letters</div>
    <div class="grid g5">
      <button class="btn" onclick="sendKey('q')">Q</button><button class="btn" onclick="sendKey('w')">W</button><button class="btn" onclick="sendKey('e')">E</button><button class="btn" onclick="sendKey('r')">R</button><button class="btn" onclick="sendKey('t')">T</button>
      <button class="btn" onclick="sendKey('y')">Y</button><button class="btn" onclick="sendKey('u')">U</button><button class="btn" onclick="sendKey('i')">I</button><button class="btn" onclick="sendKey('o')">O</button><button class="btn" onclick="sendKey('p')">P</button>
      <button class="btn" onclick="sendKey('a')">A</button><button class="btn" onclick="sendKey('s')">S</button><button class="btn" onclick="sendKey('d')">D</button><button class="btn" onclick="sendKey('f')">F</button><button class="btn" onclick="sendKey('g')">G</button>
      <button class="btn" onclick="sendKey('h')">H</button><button class="btn" onclick="sendKey('j')">J</button><button class="btn" onclick="sendKey('k')">K</button><button class="btn" onclick="sendKey('l')">L</button><button class="btn" onclick="sendKey('z')">Z</button>
      <button class="btn" onclick="sendKey('x')">X</button><button class="btn" onclick="sendKey('c')">C</button><button class="btn" onclick="sendKey('v')">V</button><button class="btn" onclick="sendKey('b')">B</button><button class="btn" onclick="sendKey('n')">N</button>
      <button class="btn" onclick="sendKey('m')">M</button>
    </div>
  </div>

  <div class="section">
    <div class="section-label">Numbers</div>
    <div class="grid g5">
      <button class="btn" onclick="sendKey('1')">1</button><button class="btn" onclick="sendKey('2')">2</button><button class="btn" onclick="sendKey('3')">3</button><button class="btn" onclick="sendKey('4')">4</button><button class="btn" onclick="sendKey('5')">5</button>
      <button class="btn" onclick="sendKey('6')">6</button><button class="btn" onclick="sendKey('7')">7</button><button class="btn" onclick="sendKey('8')">8</button><button class="btn" onclick="sendKey('9')">9</button><button class="btn" onclick="sendKey('0')">0</button>
    </div>
  </div>

  <div class="section">
    <div class="section-label">Custom Text</div>
    <textarea id="typeBox" placeholder="Type something to send…"></textarea>
    <div class="slider-row">
      <span style="color:var(--dim);font-size:12px">Delay per char</span>
      <input type="range" id="typeDelay" min="0" max="200" value="0" oninput="document.getElementById('delayVal').textContent=this.value+'ms'">
      <span class="slider-val" id="delayVal">0ms</span>
    </div>
    <div class="row2">
      <button class="btn" onclick="sendType(false)">Type</button>
      <button class="btn" onclick="sendType(true)">Type + Enter</button>
    </div>
  </div>
</div>

<!-- ════════════════════ PAGE: PAYLOADS ════════════════════ -->
<div class="page" id="pg-payloads">
  <div class="page-title">Payloads</div>
  <div class="page-sub">Pre-built keystroke sequences</div>

  <div class="section">
    <div class="section-label">Windows</div>
    <button class="btn" onclick="run('/run/rick')">Rick Roll</button>
    <button class="btn" onclick="run('/run/matrix')">Matrix Screen</button>
    <button class="btn" onclick="run('/run/volume')">Volume Up ×10</button>
    <button class="btn" onclick="run('/run/msinfo')">System Info</button>
    <button class="btn" onclick="run('/run/opensettings')">Open Settings</button>
    <button class="btn btn-danger" onclick="confirmRun('/run/shutdown','shut down this Windows machine')">Shutdown (60s timer)</button>
    <button class="btn btn-danger" onclick="confirmRun('/run/reboot','reboot this Windows machine')">Reboot (60s timer)</button>
  </div>

  <hr>

  <div class="section">
    <div class="section-label">Linux</div>
    <button class="btn" onclick="run('/run/opentermlinux')">Open Terminal</button>
    <button class="btn" onclick="run('/run/ricklinux')">Rick Roll</button>
    <button class="btn" onclick="run('/run/rickascii')">Rick Roll (ASCII)</button>
    <button class="btn" onclick="run('/run/cmatrixlinux')">Matrix Screen</button>
    <button class="btn" onclick="run('/run/volumeuplinux')">Volume Up ×10</button>
    <button class="btn" onclick="run('/run/settingslinux')">Open Settings</button>
    <button class="btn" onclick="confirmRun('/run/spam','start the keyboard spammer — only a device reset stops it')">Keyboard Spammer</button>
    <button class="btn" onclick="run('/run/stopspam')">Stop Spammer</button>
    <button class="btn btn-danger" onclick="confirmRun('/run/shutdownlinux','shut down this Linux machine')">Shutdown</button>
    <button class="btn btn-danger" onclick="confirmRun('/run/rebootlinux','reboot this Linux machine')">Reboot</button>
  </div>
</div>

<!-- ════════════════════ PAGE: BUILDER ════════════════════ -->
<div class="page" id="pg-builder">
  <div class="page-title">Payload Builder</div>
  <div class="page-sub">Build a custom sequence, save and replay it</div>

  <div class="section">
    <div class="section-label">Sequence</div>
    <div class="step-list" id="stepList">
      <div class="step-empty" id="stepEmpty">No steps yet. Add one below.</div>
    </div>

    <div class="section-label">Add Step</div>
    <div class="picker-grid">
      <button class="btn" onclick="addStep('text')">Type Text</button>
      <button class="btn" onclick="addStep('key')">Single Key</button>
      <button class="btn" onclick="addStep('combo')">Key Combo</button>
      <button class="btn" onclick="addStep('delay')">Delay</button>
      <button class="btn" onclick="addStep('openrun')">Open Run (Win)</button>
      <button class="btn" onclick="addStep('openterm')">Open Terminal (Linux)</button>
      <button class="btn" onclick="addStep('enter')">Press Enter</button>
      <button class="btn" onclick="addStep('tab')">Press Tab</button>
    </div>
  </div>

  <div class="section">
    <div class="section-label">Step Input</div>
    <div id="stepInput" style="display:none">
      <input type="text"   id="stepText"  placeholder="Text to type" style="display:none">
      <input type="text"   id="stepKey"   placeholder="Key name, e.g. f5 or a" style="display:none">
      <div id="stepComboWrap" style="display:none">
        <div class="mod-row" style="margin-bottom:8px">
          <button class="btn" id="bc-ctrl"  onclick="toggleBCombo('ctrl')">Ctrl</button>
          <button class="btn" id="bc-shift" onclick="toggleBCombo('shift')">Shift</button>
          <button class="btn" id="bc-alt"   onclick="toggleBCombo('alt')">Alt</button>
          <button class="btn" id="bc-gui"   onclick="toggleBCombo('gui')">Win</button>
        </div>
        <input type="text" id="stepComboKey" placeholder="Key, e.g. z or f4">
      </div>
      <input type="number" id="stepDelay" placeholder="Delay in ms, e.g. 500" style="display:none" min="0" max="9999">
      <button class="btn" style="margin-top:10px" onclick="confirmStep()">Add to Sequence</button>
    </div>
  </div>

  <div class="section">
    <div class="builder-name-row">
      <input type="text" id="seqName" placeholder="Sequence name…" style="margin:0">
      <button class="btn" style="width:auto;white-space:nowrap;margin:0" onclick="saveSeq()">Save</button>
    </div>
    <div class="row2">
      <button class="btn" onclick="runSeq()">Run Now</button>
      <button class="btn btn-danger" onclick="clearSeq()">Clear</button>
    </div>
  </div>

  <hr>

  <div class="section">
    <div class="section-label">Saved Sequences</div>
    <div id="savedList"></div>
  </div>

  <hr>

  <div class="section">
    <div class="section-label">Log</div>
    <div class="log-box" id="logBox"></div>
    <button class="btn btn-sm" style="margin-top:8px;text-align:center;color:var(--dim)" onclick="clearLog()">Clear Log</button>
  </div>
</div>

<!-- ════════════════════ PAGE: STATUS ════════════════════ -->
<div class="page" id="pg-status">
  <div class="page-title">Status</div>
  <div class="page-sub">Device info — refreshes every 3 seconds</div>

  <div class="stat-grid">
    <div class="stat-card"><div class="stat-val" id="s-clients">—</div><div class="stat-lbl">Clients</div></div>
    <div class="stat-card"><div class="stat-val" id="s-uptime">—</div><div class="stat-lbl">Uptime</div></div>
    <div class="stat-card"><div class="stat-val" id="s-heap">—</div><div class="stat-lbl">Free Heap</div></div>
    <div class="stat-card"><div class="stat-val" id="s-ip">—</div><div class="stat-lbl">AP IP</div></div>
  </div>

  <div class="section">
    <div class="section-label">Log</div>
    <div class="log-box" id="logBox2"></div>
  </div>
</div>

<!-- ════════════════════ NAV ════════════════════ -->
<nav>
  <button class="active" onclick="goPage('keys',this)">
    <svg viewBox="0 0 24 24"><rect x="2" y="6" width="20" height="13" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M6 14h.01M10 14h4M18 14h.01"/></svg>
    Keys
  </button>
  <button onclick="goPage('payloads',this)">
    <svg viewBox="0 0 24 24"><polygon points="5 3 19 12 5 21 5 3"/></svg>
    Payloads
  </button>
  <button onclick="goPage('builder',this)">
    <svg viewBox="0 0 24 24"><line x1="8" y1="6" x2="21" y2="6"/><line x1="8" y1="12" x2="21" y2="12"/><line x1="8" y1="18" x2="21" y2="18"/><line x1="3" y1="6" x2="3.01" y2="6"/><line x1="3" y1="12" x2="3.01" y2="12"/><line x1="3" y1="18" x2="3.01" y2="18"/></svg>
    Builder
  </button>
  <button onclick="goPage('status',this)">
    <svg viewBox="0 0 24 24"><path d="M22 12h-4l-3 9L9 3l-3 9H2"/></svg>
    Status
  </button>
</nav>

<script>
// ── Navigation ──────────────────────────────────────────────────────────────
function goPage(id, btn) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));
  document.getElementById('pg-' + id).classList.add('active');
  btn.classList.add('active');
  if (id === 'status') fetchStatus();
}

// ── Log ─────────────────────────────────────────────────────────────────────
function log(msg) {
  const now = new Date();
  const ts  = now.getHours().toString().padStart(2,'0') + ':' +
               now.getMinutes().toString().padStart(2,'0') + ':' +
               now.getSeconds().toString().padStart(2,'0');
  const entry = `<div class="log-entry"><span>${ts}</span>${msg}</div>`;
  ['logBox','logBox2'].forEach(id => {
    const el = document.getElementById(id);
    if (el) { el.innerHTML += entry; el.scrollTop = el.scrollHeight; }
  });
}
function clearLog() { ['logBox','logBox2'].forEach(id => { const el = document.getElementById(id); if(el) el.innerHTML=''; }); }

// ── Fetch helpers ────────────────────────────────────────────────────────────
function run(path) {
  log('Run: ' + path);
  fetch(path).catch(()=>{});
}
function confirmRun(path, label) {
  if (confirm('Are you sure you want to ' + label + '?')) run(path);
}
function sendKey(name) {
  log('Key: ' + name);
  fetch('/key/' + name).catch(()=>{});
}
function combo(mod, key) {
  log('Combo: ' + mod + '+' + key);
  fetch('/combo?'+mod+'=1&key='+encodeURIComponent(key)).catch(()=>{});
}

// ── Modifiers ────────────────────────────────────────────────────────────────
const mods = { ctrl:false, shift:false, alt:false, gui:false };
function toggleMod(name) {
  fetch('/modkey/' + name).then(r=>r.text()).then(state => {
    mods[name] = state.trim() === 'on';
    document.getElementById('m-' + name).classList.toggle('on', mods[name]);
    log('Modifier ' + name + ': ' + state.trim());
  }).catch(()=>{});
}
function releaseMods() {
  fetch('/modkey/releaseall').then(() => {
    ['ctrl','shift','alt','gui'].forEach(n => {
      mods[n] = false;
      document.getElementById('m-' + n).classList.remove('on');
    });
    log('All modifiers released');
  }).catch(()=>{});
}

// ── Type ─────────────────────────────────────────────────────────────────────
function sendType(withEnter) {
  const text  = document.getElementById('typeBox').value;
  const delay = document.getElementById('typeDelay').value;
  if (!text) return;
  log('Type: "' + text.substring(0,30) + (text.length>30?'…':'"'));
  fetch('/type', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'text='+encodeURIComponent(text)+'&enter='+(withEnter?'1':'0')+'&delay='+delay
  }).catch(()=>{});
}

// ── Builder ──────────────────────────────────────────────────────────────────
let steps = [];
let curStepType = null;
const bCombo = { ctrl:false, shift:false, alt:false, gui:false };

function toggleBCombo(name) {
  bCombo[name] = !bCombo[name];
  document.getElementById('bc-'+name).classList.toggle('on', bCombo[name]);
}

function addStep(type) {
  curStepType = type;
  const ids = ['stepText','stepKey','stepComboWrap','stepDelay'];
  ids.forEach(id => document.getElementById(id).style.display='none');
  document.getElementById('stepInput').style.display = 'block';

  if (type==='text')     document.getElementById('stepText').style.display='block';
  else if (type==='key') document.getElementById('stepKey').style.display='block';
  else if (type==='combo') document.getElementById('stepComboWrap').style.display='block';
  else if (type==='delay') document.getElementById('stepDelay').style.display='block';
  // for enter/tab/openrun/openterm — no extra input needed
}

function confirmStep() {
  let label='', data={type:curStepType};
  if (curStepType==='text') {
    const v = document.getElementById('stepText').value; if(!v) return;
    data.text=v; label='Type: "'+v.substring(0,20)+(v.length>20?'…':'"');
  } else if (curStepType==='key') {
    const v = document.getElementById('stepKey').value.trim().toLowerCase(); if(!v) return;
    data.key=v; label='Key: '+v;
  } else if (curStepType==='combo') {
    const k = document.getElementById('stepComboKey').value.trim().toLowerCase(); if(!k) return;
    const m = Object.keys(bCombo).filter(x=>bCombo[x]);
    data.mods=m; data.key=k; label='Combo: '+[...m,k].join('+');
    Object.keys(bCombo).forEach(x=>{ bCombo[x]=false; document.getElementById('bc-'+x).classList.remove('on'); });
  } else if (curStepType==='delay') {
    const v = parseInt(document.getElementById('stepDelay').value)||500;
    data.ms=v; label='Delay: '+v+'ms';
  } else if (curStepType==='openrun')  label='Open Run (Win)';
  else if (curStepType==='openterm') label='Open Terminal (Linux)';
  else if (curStepType==='enter')    label='Enter';
  else if (curStepType==='tab')      label='Tab';

  steps.push({data, label});
  renderSteps();
  document.getElementById('stepInput').style.display='none';
}

function renderSteps() {
  const list = document.getElementById('stepList');
  const empty = document.getElementById('stepEmpty');
  if (steps.length===0) { list.innerHTML=''; list.appendChild(empty); return; }
  list.innerHTML = steps.map((s,i)=>
    `<div class="step-item">${i+1}. ${s.label}<button class="step-del" onclick="removeStep(${i})">×</button></div>`
  ).join('');
}

function removeStep(i) { steps.splice(i,1); renderSteps(); }
function clearSeq() { steps=[]; renderSteps(); }

function runSeq() {
  if (steps.length===0) return;
  log('Running sequence ('+steps.length+' steps)');
  fetch('/builder/run', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(steps.map(s=>s.data))
  }).catch(()=>{});
}

function saveSeq() {
  const name = document.getElementById('seqName').value.trim();
  if (!name || steps.length===0) return;
  const saved = JSON.parse(localStorage.getItem('hid_seqs')||'{}');
  saved[name] = steps;
  localStorage.setItem('hid_seqs', JSON.stringify(saved));
  renderSaved();
  log('Saved sequence: '+name);
}

function renderSaved() {
  const saved = JSON.parse(localStorage.getItem('hid_seqs')||'{}');
  const el = document.getElementById('savedList');
  const keys = Object.keys(saved);
  if (keys.length===0) { el.innerHTML='<div style="color:var(--dim);font-size:13px">No saved sequences yet.</div>'; return; }
  el.innerHTML = keys.map(name =>
    `<div class="saved-item">
      <button class="btn" onclick="loadAndRun('${name}')">${name}</button>
      <button class="saved-del" onclick="deleteSeq('${name}')">×</button>
    </div>`
  ).join('');
}

function loadAndRun(name) {
  const saved = JSON.parse(localStorage.getItem('hid_seqs')||'{}');
  if (!saved[name]) return;
  steps = saved[name];
  renderSteps();
  log('Loaded + running: '+name);
  runSeq();
}

function deleteSeq(name) {
  const saved = JSON.parse(localStorage.getItem('hid_seqs')||'{}');
  delete saved[name];
  localStorage.setItem('hid_seqs', JSON.stringify(saved));
  renderSaved();
}

renderSaved();

// ── Status ───────────────────────────────────────────────────────────────────
function fetchStatus() {
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('s-clients').textContent = d.clients;
    document.getElementById('s-uptime').textContent  = d.uptime;
    document.getElementById('s-heap').textContent    = d.heap+'K';
    document.getElementById('s-ip').textContent      = d.ip;
  }).catch(()=>{});
}
setInterval(()=>{ if(document.getElementById('pg-status').classList.contains('active')) fetchStatus(); }, 3000);
</script>
</body>
</html>
)rawliteral";

// ─── HTTP Handlers ────────────────────────────────────────────────────────────
void handleRoot()   { server.send(200,"text/html",HTML); }

void handleStatus() {
  uint32_t upSec = millis()/1000;
  char buf[180];
  snprintf(buf, sizeof(buf),
    "{\"clients\":%d,\"uptime\":\"%02lu:%02lu:%02lu\",\"heap\":%lu,\"ip\":\"%s\"}",
    WiFi.softAPgetStationNum(),
    upSec/3600, (upSec%3600)/60, upSec%60,
    (unsigned long)(ESP.getFreeHeap()/1024),
    WiFi.softAPIP().toString().c_str()
  );
  server.send(200,"application/json",buf);
}

void handleType() {
  if (server.hasArg("text")) {
    String text  = server.arg("text");
    int    delay_ms = server.hasArg("delay") ? server.arg("delay").toInt() : 0;
    ledEvent(LED_TYPING, (text.length() * max(delay_ms,2)) + 500);
    for (int i=0; i<(int)text.length(); i++) {
      Keyboard.write((uint8_t)text[i]);
      if (delay_ms > 0) delay(delay_ms);
    }
    if (server.hasArg("enter") && server.arg("enter")=="1") Keyboard.write(KEY_RETURN);
  }
  server.send(200,"text/plain","OK");
}

void handleKey() {
  String k = server.uri().substring(5); // strip "/key/"
  ledEvent(LED_ACTIVE, 400);
  pressNamed(k);
  server.send(200,"text/plain","OK");
}

void handleCombo() {
  bool ctrl  = server.hasArg("ctrl");
  bool shift = server.hasArg("shift");
  bool alt   = server.hasArg("alt");
  bool gui   = server.hasArg("gui");
  String key = server.hasArg("key") ? server.arg("key") : "";
  ledEvent(LED_ACTIVE, 400);
  if (ctrl)  Keyboard.press(KEY_LEFT_CTRL);
  if (shift) Keyboard.press(KEY_LEFT_SHIFT);
  if (alt)   Keyboard.press(KEY_LEFT_ALT);
  if (gui)   Keyboard.press(KEY_LEFT_GUI);
  if (key.length()>1) pressNamed(key); else if(key.length()==1) Keyboard.write((char)key[0]);
  delay(80);
  Keyboard.releaseAll();
  server.send(200,"text/plain","OK");
}

void handleModKey() {
  String name = server.uri().substring(8); // strip "/modkey/"
  if (name=="releaseall") { releaseAllModifiers(); server.send(200,"text/plain","off"); return; }
  String state;
  // toggle
  if      (name=="ctrl")  { ctrlHeld=!ctrlHeld;  if(ctrlHeld) Keyboard.press(KEY_LEFT_CTRL);  else Keyboard.release(KEY_LEFT_CTRL);  state=ctrlHeld?"on":"off"; }
  else if (name=="shift") { shiftHeld=!shiftHeld; if(shiftHeld)Keyboard.press(KEY_LEFT_SHIFT); else Keyboard.release(KEY_LEFT_SHIFT); state=shiftHeld?"on":"off"; }
  else if (name=="alt")   { altHeld=!altHeld;     if(altHeld)  Keyboard.press(KEY_LEFT_ALT);   else Keyboard.release(KEY_LEFT_ALT);   state=altHeld?"on":"off"; }
  else if (name=="gui")   { guiHeld=!guiHeld;     if(guiHeld)  Keyboard.press(KEY_LEFT_GUI);   else Keyboard.release(KEY_LEFT_GUI);   state=guiHeld?"on":"off"; }
  server.send(200,"text/plain",state);
}

void handleBuilderRun() {
  if (!server.hasArg("plain")) { server.send(400,"text/plain","no body"); return; }
  String body = server.arg("plain");
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, body)) { server.send(400,"text/plain","bad json"); return; }
  server.send(200,"text/plain","OK"); // respond before executing so browser doesn't time out
  ledEvent(LED_ACTIVE, 5000);

  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject step : arr) {
    String type = step["type"] | "";
    if (type=="text") {
      String t = step["text"] | "";
      Keyboard.print(t);
    } else if (type=="key") {
      pressNamed(step["key"] | "");
    } else if (type=="combo") {
      JsonArray m = step["mods"];
      for (const char* mod : m) {
        String ms(mod);
        if (ms=="ctrl")  Keyboard.press(KEY_LEFT_CTRL);
        if (ms=="shift") Keyboard.press(KEY_LEFT_SHIFT);
        if (ms=="alt")   Keyboard.press(KEY_LEFT_ALT);
        if (ms=="gui")   Keyboard.press(KEY_LEFT_GUI);
      }
      String k = step["key"] | "";
      if (k.length()>1) pressNamed(k); else if(k.length()==1) Keyboard.write((char)k[0]);
      delay(80); Keyboard.releaseAll();
    } else if (type=="delay") {
      delay(step["ms"] | 500);
    } else if (type=="openrun")  { openRunWin(); }
    else if (type=="openterm") { openTermLinux(); }
    else if (type=="enter")    { Keyboard.write(KEY_RETURN); }
    else if (type=="tab")      { Keyboard.write(KEY_TAB); }
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  rgbLed.begin();
  setRGB(150, 75, 0); // Amber: booting

  USB.begin();
  Keyboard.begin();
  ConsumerControl.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/",             HTTP_GET,  handleRoot);
  server.on("/status",       HTTP_GET,  handleStatus);
  server.on("/type",         HTTP_POST, handleType);
  server.on("/combo",        HTTP_GET,  handleCombo);
  server.on("/modkey/ctrl",      HTTP_GET, handleModKey);
  server.on("/modkey/shift",     HTTP_GET, handleModKey);
  server.on("/modkey/alt",       HTTP_GET, handleModKey);
  server.on("/modkey/gui",       HTTP_GET, handleModKey);
  server.on("/modkey/releaseall",HTTP_GET, handleModKey);
  server.on("/builder/run",  HTTP_POST, handleBuilderRun);

  // Key routes — register each named key
  auto regKey = [](const char* path){ server.on(path, HTTP_GET, handleKey); };
  regKey("/key/enter"); regKey("/key/tab"); regKey("/key/esc"); regKey("/key/space");
  regKey("/key/backspace"); regKey("/key/delete"); regKey("/key/insert");
  regKey("/key/home"); regKey("/key/end"); regKey("/key/pgup"); regKey("/key/pgdn");
  regKey("/key/up"); regKey("/key/down"); regKey("/key/left"); regKey("/key/right");
  regKey("/key/f1");  regKey("/key/f2");  regKey("/key/f3");  regKey("/key/f4");
  regKey("/key/f5");  regKey("/key/f6");  regKey("/key/f7");  regKey("/key/f8");
  regKey("/key/f9");  regKey("/key/f10"); regKey("/key/f11"); regKey("/key/f12");
  regKey("/key/wintap"); regKey("/key/alttab"); regKey("/key/altf4"); regKey("/key/ctrlaltdel");
  // letters
  const char letters[] = "abcdefghijklmnopqrstuvwxyz";
  for (int i=0;i<26;i++){
    char path[12]; snprintf(path,sizeof(path),"/key/%c",letters[i]);
    server.on(path, HTTP_GET, handleKey);
  }
  // numbers
  for (int i=0;i<=9;i++){
    char path[10]; snprintf(path,sizeof(path),"/key/%d",i);
    server.on(path, HTTP_GET, handleKey);
  }

  // Payload routes
  server.on("/run/rick",         HTTP_GET, [](){ server.send(200,"text/plain","OK"); plRickWin(); });
  server.on("/run/matrix",       HTTP_GET, [](){ server.send(200,"text/plain","OK"); plMatrixWin(); });
  server.on("/run/volume",       HTTP_GET, [](){ server.send(200,"text/plain","OK"); plVolUpWin(); });
  server.on("/run/msinfo",       HTTP_GET, [](){ server.send(200,"text/plain","OK"); plMsinfoWin(); });
  server.on("/run/opensettings", HTTP_GET, [](){ server.send(200,"text/plain","OK"); plSettingsWin(); });
  server.on("/run/shutdown",     HTTP_GET, [](){ server.send(200,"text/plain","OK"); plShutdownWin(); });
  server.on("/run/reboot",       HTTP_GET, [](){ server.send(200,"text/plain","OK"); plRebootWin(); });
  server.on("/run/ricklinux",    HTTP_GET, [](){ server.send(200,"text/plain","OK"); plRickLinux(); });
  server.on("/run/rickascii",    HTTP_GET, [](){ server.send(200,"text/plain","OK"); plRickAscii(); });
  server.on("/run/cmatrixlinux", HTTP_GET, [](){ server.send(200,"text/plain","OK"); plMatrixLinux(); });
  server.on("/run/volumeuplinux",HTTP_GET, [](){ server.send(200,"text/plain","OK"); plVolUpLinux(); });
  server.on("/run/settingslinux",HTTP_GET, [](){ server.send(200,"text/plain","OK"); plSettingsLinux(); });
  server.on("/run/shutdownlinux",HTTP_GET, [](){ server.send(200,"text/plain","OK"); plShutdownLinux(); });
  server.on("/run/rebootlinux",  HTTP_GET, [](){ server.send(200,"text/plain","OK"); plRebootLinux(); });
  server.on("/run/opentermlinux",HTTP_GET, [](){ server.send(200,"text/plain","OK"); ledEvent(LED_ACTIVE); openTermLinux(); });
  server.on("/run/spam",         HTTP_GET, [](){ server.send(200,"text/plain","OK"); spamActive=true; ledEvent(LED_ACTIVE,30000); });
  server.on("/run/stopspam",     HTTP_GET, [](){ spamActive=false; server.send(200,"text/plain","OK"); });

  server.begin();
  ledMode = LED_IDLE;
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  spamTick();
  updateLed();

  // Update base LED mode based on client count (only when not in a timed event)
  static uint32_t lastClientCheck = 0;
  if (!ledTimed && millis() - lastClientCheck > 2000) {
    lastClientCheck = millis();
    ledMode = (WiFi.softAPgetStationNum() > 0) ? LED_READY : LED_IDLE;
  }
}