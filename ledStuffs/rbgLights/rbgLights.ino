#include <Adafruit_NeoPixel.h>
#include <iostream>

#define RGB_PIN        48   // Standard ESP32-S3 RGB pin
#define NUMPIXELS       1   // There is 1 RGB LED on your board

Adafruit_NeoPixel pixel(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200); 
  pixel.begin(); // Initialize the RGB LED
}

void loop() {

  for(int i = 0; i < 255; i = i + 50){
    for(int j = 0; j < 255; j = j + 50){
      for(int l = 0; l < 255; l = l + 50){
        pixel.setPixelColor(0, pixel.Color(i, j, l));
        pixel.show();
        delay(10);
        Serial.println("Electronics Bitch !");
      }

      // blinks after 1 iteration
      pixel.setPixelColor(0, pixel.Color(0,0,0));        
      delay(200);
    }
  }


  Serial.println("It's about to end...");

  // Blue  for 5 seconds when finished 
  pixel.setPixelColor(0, pixel.Color(235, 19, 242));
  pixel.show();
  delay(2000);

  // turn off for 5 seconds
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));
  pixel.show();
  delay(5000);


}

