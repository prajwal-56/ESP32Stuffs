#include <WiFi.h>
#include "esp_wifi.h"
#include <Adafruit_NeoPixel.h> 

#define RGB_PIN        48 
#define NUMPIXELS       1 

// Initialize the NeoPixel object
Adafruit_NeoPixel rgbLed(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);


// You can Modify this list for custom SSIDs
const char* ssids[] = {
  "weAreFS0Ci3TY01",
  "weAreFS0Ci3TY02",
  "weAreFS0Ci3TY03",
  "weAreFS0Ci3TY04",
  "weAreFS0Ci3TY05",
  "weAreFS0Ci3TY06",
  "weAreFS0Ci3TY07",
  "weAreFS0Ci3TY08",
  "weAreFS0Ci3TY09",
  "weAreFS0Ci3TY10",
  "weAreFS0Ci3TY11",
  "weAreFS0Ci3TY12",
  "Arch User",
  "govtIsHidingAlotOfThings",
  "SYNC_IS_SCAM",
  "SYNC_IS_SCAM",
  "SYNC_IS_SCAM",
  "SYNC_IS_SCAM",
  "SYNC_IS_SCAM",
  "Fuck Modi",
  "Modi Sucks"
};

// total frame size 
const int total_ssids = sizeof(ssids) / sizeof(ssids[0]);

// Template for standard 802.11 Beacon Frame
uint8_t packet[128] = {
  0x80, 0x00,                         // Define this Frame as a "Beacon"
  0x00, 0x00,                         // Duration
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (Broadcast)
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source MAC (Modified dynamically)
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // BSSID
  0x00, 0x00,                         // Sequence Control
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
  0x64, 0x00,                         // Beacon Interval
  0x11, 0x00,                         // Capability Info
  0x00                                // Tag 0 (SSID Start)
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize RGB LED and set to SOLID BLUE for initialization stage
  rgbLed.begin();
  rgbLed.setPixelColor(0, rgbLed.Color(0, 0, 255)); // Blue
  rgbLed.show();
  
  Serial.println("\n[STAGE] Initializing hardware...");

  WiFi.mode(WIFI_STA);  // Station 
  esp_wifi_stop();
  
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);     // set interface to Promiscuous mode - Allows us to read and write raw data headers over the air 
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);   //  Locks the frequency to channel 1
  

  Serial.println("[STAGE] Initialization complete. Starting transmission loop.");
  
  // Changed RGB to RED to indicate it's about to do it's job
  rgbLed.setPixelColor(0, rgbLed.Color(160, 0, 0));
  rgbLed.show();
}

void loop() {
  bool any_error = false;     // To detect whether a packet failed to send

  for (int i = 0; i < total_ssids; i++) {
    int ssid_len = strlen(ssids[i]);
    
    // Randomize MAC addressing per loop item
    packet[10] = 0x02; 
    packet[11] = 0x3A;
    packet[12] = 0x5C;
    packet[13] = 0x7E;
    packet[14] = 0x90;
    packet[15] = i;    
    memcpy(&packet[16], &packet[10], 6);

    packet[37] = ssid_len; 
    memcpy(&packet[38], ssids[i], ssid_len); 

    int current_idx = 38 + ssid_len;
    packet[current_idx] = 0x03;     
    packet[current_idx + 1] = 0x01; 
    packet[current_idx + 2] = 0x01; 

    int total_packet_len = 38 + ssid_len + 3;

    // Inject the packet
    esp_err_t res = esp_wifi_80211_tx(WIFI_IF_STA, packet, total_packet_len, true);
    
    if (res != ESP_OK) {
      any_error = true;
      Serial.printf("[ERROR] Failed packet: %s\n", ssids[i]);
    }
    delay(10); 
  }

  // Visual status update after handling the SSID batch
  if (any_error) {
    // BLINK RED if any injection error occurred
    rgbLed.setPixelColor(0, rgbLed.Color(255, 0, 0)); // Red
    rgbLed.show();
    delay(50);
    rgbLed.setPixelColor(0, rgbLed.Color(0, 0, 0)); // Off
    rgbLed.show();
  } else {
    // BLINK GREEN for a completely successful transmission cycle
    rgbLed.setPixelColor(0, rgbLed.Color(0, 255, 0)); // Green
    rgbLed.show();
    delay(50);
    rgbLed.setPixelColor(0, rgbLed.Color(0, 0, 0)); // Off
    rgbLed.show();
  }

  delay(300); // Wait a few seconds before the next sweep
}
