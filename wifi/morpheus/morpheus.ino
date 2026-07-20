#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#include <Adafruit_NeoPixel.h>
#include <iostream>
#include "esp_wifi.h"

#define RGB_PIN        48   // RGB pin
#define NUMPIXELS       1   // There is 1 RGB LED on your board

Adafruit_NeoPixel pixel(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);


#ifndef LED_BUILTIN
#define LED_BUILTIN 48  // RGB LED 
#endif

// Set these to your desired credentials.
const char *ssid = "ESPraw";
const char *password = "pppppppp";

NetworkServer server(80);


int numOfClientsConnected = 0;

void twitchRGB(WiFiEvent_t event , WiFiEventInfo_t info){
  pixel.setPixelColor( 0 , pixel.Color( 255, 0, 0));
  pixel.show();
  delay(750);
  pixel.setPixelColor( 0 , pixel.Color( 0, 255, 0));
  pixel.show();
  delay(750);
  pixel.setPixelColor( 0 , pixel.Color( 0, 0 , 255));
  pixel.show();
  delay(3000);

  pixel.setPixelColor( 0 , pixel.Color( 0, 255 ,0 ));  // finally set it to Green
  pixel.show();
}

void setup() {

  // When just powered on - Shows Blue
  pixel.begin();
  pixel.setPixelColor(0, pixel.Color( 0,255,0)); // When Powered on : Blue
  pixel.show();


  Serial.begin(115200);

  esp_wifi_set_ps(WIFI_PS_NONE);  // Ensures Radio is at maximum performance (diabled power saving)

  WiFi.onEvent( twitchRGB , ARDUINO_EVENT_WIFI_AP_STACONNECTED);    // twitches the RGB led when a new host is connected 

  Serial.println("\nSerial Monitor Connected Successfully....\n");
  Serial.println();
  Serial.println("Configuring access point...");

// Valideating password
  if (!WiFi.softAP(ssid, password)) {
    Serial.println("Soft AP creation failed.");

    pixel.setPixelColor(0, pixel.Color(255,0,0)); // red when connection fails (incorrect password)
    pixel.show();

    // while (1);
  }


  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();

  Serial.println("Server started");
  pixel.setPixelColor(0, pixel.Color(242, 19, 212)); // Magenta when ready to connect.
  pixel.show();
}

void loop() {
  WiFiClient client = server.available();  // listen for incoming clients

  if (client) {                     // if you get a client,
    Serial.println("New Client.");  

    // Led changes when there's clients connected.
    // if( numOfClientsConnected >0 ){
    //   pixel.setPixelColor(0, pixel.Color(0, 150, 0));   // Green to indicate a client is connected 
    // } else { 
    //   pixel.setPixelColor(0, pixel.Color(242, 19, 212));   // Magneta to indicate No connected Clients or waiting for connections
    // }



    String request = "";        // make a String to hold incoming data from the client
    while (client.connected()) {    // loop while the client's connected
      if (client.available()) {     // if there's bytes to read from the client,
        String line = client.readStringUntil('\n');     // reads entire line 
        request += line;
        if (line == "\r") {            // if end of requests

          if (request.indexOf("GET /H") >= 0) pixel.setPixelColor(0, pixel.Color(150, 0, 0));  // Red
          if (request.indexOf("GET /L") >= 0) pixel.setPixelColor(0, pixel.Color(0, 0, 150));  // Blue
          pixel.show();

          // Webpage content : 
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.print(" Take the Red Pill : <a href=\"/H\"> <button style=`color:Black;font-weight:bold;background-color:red;`> Red </button> </a> .<br>");
          client.print(" Take the Blue Pill <a href=\"/L\"> <button style=`color:Black;font-weight:bold;background-color:Blue;`> Blue </button> </a>.<br>");
          client.println();
      // The HTTP response ends with another blank line:
          client.println();
          break;
        } 
      }
    }

    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  } 
}
