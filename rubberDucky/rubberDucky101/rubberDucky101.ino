#include "USB.h"
#include "USBHIDKeyboard.h"
#include <Adafruit_NeoPixel.h>

#define RGB_PIN        48   // Standard ESP32-S3 RGB pin
#define NUMPIXELS       1   // There is 1 RGB LED on your board
#define BOOT_BUTTON_PIN 0 

Adafruit_NeoPixel pixel(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
USBHIDKeyboard Keyboard;


bool isPayloadActive = false;

void setup(){
  Serial.begin(115200);
  Keyboard.begin();
  USB.begin();
  pixel.begin();

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  delay(3000);

  pixel.setPixelColor(0, pixel.Color( 0, 156, 0)); // Green waiting to press the BOOT button
  pixel.show();

}

void loop(){

  if (isPayloadActive == false){
      // When you hold down the BOOT button, It's interpreted as LOW.
      if(digitalRead(BOOT_BUTTON_PIN) == LOW){
      
      // Wait until you let go of the boot button so it registers a clean tap
      while(digitalRead(BOOT_BUTTON_PIN) == LOW) {
        delay(10);
      }

      isPayloadActive = true;

      }
  } else if( isPayloadActive == true){

      // RGB Turns red 
      pixel.setPixelColor( 0, pixel.Color( 160, 0 , 0));  // Red - about to start the payload
      pixel.show();

      // opens terminal
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press('t');  
      delay(50);
      Keyboard.releaseAll();

      delay(1000);  // wait for the terminal to open completely 


      Keyboard.println("figlet 'Wubba Lubba Dub Dub !!!' ");
      delay(1000);

      // Stops from executing the payload multiple times when holding down 
      // while(digitalRead(BOOT_BUTTON_PIN) == LOW) {
      //   delay(10);
      // }

      // Return to Green
      pixel.setPixelColor(0, pixel.Color(0, 160, 0)); //Green 
      pixel.show();
      delay(1000);
  }

}