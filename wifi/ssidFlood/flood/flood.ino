#include <WiFi.h>
#include "esp_wifi.h"
#include <Adafruit_NeoPixel.h>

// ---- RGB LED config ----
#define LED_PIN     48
#define NUM_LEDS    1
Adafruit_NeoPixel pixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setLED(uint8_t r, uint8_t g, uint8_t b) {
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
}

void blinkLED(uint8_t r, uint8_t g, uint8_t b, int times, int on_ms, int off_ms) {
    for (int i = 0; i < times; i++) {
        setLED(r, g, b);
        delay(on_ms);
        setLED(0, 0, 0);
        delay(off_ms);
    }
}

// ---- Config: fake APs ----
const char* ssids[] = {"Prawmathean0", "Prawmathean1", "Prawmathean2"};
uint8_t macs[][6] = {
    {0x00, 0x1A, 0x11, 0x22, 0x33, 0x44},
    {0x00, 0x1A, 0x11, 0x22, 0x33, 0x45},
    {0x00, 0x1A, 0x11, 0x22, 0x33, 0x46}
};
int channels[] = {6, 6, 6};
const int NUM_APS = 3;

// ---- Beacon frame template ----
uint8_t beacon_raw[128] = {
    0x80, 0x00, 0x00, 0x00,                         // Frame Control, Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,             // Destination: broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // Source MAC (filled per-packet)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // BSSID (filled per-packet)
    0x00, 0x00,                                     // Seq/frag (auto handled)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
    0x64, 0x00,                                     // Beacon interval (100 TU)
    0x21, 0x04                                      // Capability info
};

void send_beacon(const char* ssid, uint8_t* fake_mac, int channel) {
    int ssid_len = strlen(ssid);
    uint8_t packet[128];
    memcpy(packet, beacon_raw, 34); // header + fixed params

    memcpy(&packet[10], fake_mac, 6); // source MAC
    memcpy(&packet[16], fake_mac, 6); // BSSID

    int idx = 34; // start of tagged params

    packet[idx++] = 0x00;        // SSID tag
    packet[idx++] = ssid_len;
    memcpy(&packet[idx], ssid, ssid_len);
    idx += ssid_len;

    packet[idx++] = 0x03;        // DS Parameter (channel)
    packet[idx++] = 0x01;
    packet[idx++] = channel;

    esp_wifi_80211_tx(WIFI_IF_AP, packet, idx, false);
}

void broadcast_ap(const char* ssid, uint8_t* mac, int channel) {
    // Yellow flash = switching channel
    setLED(50, 50, 0);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    delay(10);

    // Green flash = beacon sent
    setLED(0, 50, 0);
    send_beacon(ssid, mac, channel);
    delay(30);

    setLED(0, 0, 0); // off between sends
}

void setup() {
    Serial.begin(115200);

    pixel.begin();
    pixel.setBrightness(100);

    // Blue = initializing
    setLED(0, 0, 50);

    WiFi.mode(WIFI_AP);
    WiFi.disconnect();
    esp_wifi_start();

    // Purple flash x3 = WiFi radio ready
    blinkLED(50, 0, 50, 3, 150, 150);

    Serial.println("Beacon spam started.");
}

void loop() {
    for (int i = 0; i < NUM_APS; i++) {
        broadcast_ap(ssids[i], macs[i], channels[i]);
        delay(100);
    }
}
