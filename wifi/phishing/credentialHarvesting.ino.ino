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
*/

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <vector>

// ---------- CONFIG ----------

// Optional: your phone's hotspot, so the ESP32 has internet.
// Leave HOME_SSID as "" to skip this and run AP-only (fully offline demo).
const char* HOME_SSID = "";       // e.g. "Prajwal's iPhone"
const char* HOME_PASS = "";       // e.g. "hotspot_password"

// The open demo AP that people will connect to
const char* AP_SSID   = "captive portal demo";
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
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>CampusWiFi Portal</title>
  <style>
    body {
      font-family: -apple-system, Arial, sans-serif;
      background: #0d1117;
      color: #e6edf3;
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      margin: 0;
      padding: 20px;
      box-sizing: border-box;
    }
    .card {
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 12px;
      padding: 28px;
      max-width: 360px;
      width: 100%;
    }
    h2 { margin-top: 0; }
    .warn {
      background: #3d2b00;
      border: 1px solid #d29922;
      color: #f0c05a;
      padding: 10px 12px;
      border-radius: 8px;
      font-size: 13px;
      margin-bottom: 18px;
      line-height: 1.4;
    }
    input {
      width: 100%;
      padding: 10px;
      margin: 6px 0 14px 0;
      border-radius: 6px;
      border: 1px solid #30363d;
      background: #0d1117;
      color: #e6edf3;
      box-sizing: border-box;
      font-size: 14px;
    }
    button {
      width: 100%;
      padding: 11px;
      border-radius: 6px;
      border: none;
      background: #238636;
      color: white;
      font-size: 15px;
      cursor: pointer;
    }
    button:hover { background: #2ea043; }
    .footer {
      margin-top: 16px;
      font-size: 11px;
      color: #8b949e;
      text-align: center;
    }
  </style>
</head>
<body>
  <div class="card">
    <h2>CampusWiFi Portal</h2>
    <div class="warn">
      <strong> — Blockchain Academy Kerala. </strong> <br>
      This is a fake login page for a workshop on public WiFi risks.
      <strong>Do NOT enter your real username or password.</strong>
      Type anything you like.
    </div>
    <form action="/login" method="POST">
      <label>Username</label>
      <input type="text" name="username" placeholder="anything works" autocomplete="off">
      <label>Password</label>
      <input type="text" name="password" placeholder="anything works" autocomplete="off">
      <button type="submit">Login &amp; Get Internet Access</button>
    </form>
    <div class="footer">This network is part of a live security demo, not a real service.</div>
  </div>
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

  // 1. Start AP (open network, custom name)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(AP_SSID, AP_PASS[0] == '\0' ? NULL : AP_PASS);
  Serial.print("AP started: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // 2. Optionally join a hotspot for real internet (not required for the demo)
  if (strlen(HOME_SSID) > 0) {
    WiFi.begin(HOME_SSID, HOME_PASS);
    Serial.print("Connecting to home WiFi");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(300);
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
}

// ---------- LOOP ----------

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
