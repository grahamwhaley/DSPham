// SPDX-License-Identifier: GNU General Public License v3.0 or later

#include <Arduino.h>

#include "global.h"

#include "rgb_lcd.h"
#include "morseGen.h"

extern rgb_lcd lcd;

// WARNING - the teensy 4.0 onboard led on pin13 is also shared with the SDcard on the
// audio daughterboard - so, if we ever go to use that SD slot, we'll need to find another
// LED to use...
//#define ledPin 13
#define ledPin 22

void morsePrint(char c)
{
  static char buf[17] = "                ";

  memcpy(buf, buf+1, 15);
  buf[15] = c;

  if (display ) {
    lcd.setCursor(0, 0);
    lcd.print(buf);
  }
}

void morseLed(bool on)
{
  if (on) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
}

//---------------------------------------------------------------------------------
void morseInit() {                                  // Speak callsign on boot   
  toneDetect.frequency(morse_frequency, morse_cycles);
  toneDetect.threshold(morse_threshold);
  pinMode(ledPin, OUTPUT);
}
