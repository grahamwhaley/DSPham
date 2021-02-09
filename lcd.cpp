// SPDX-License-Identifier: GNU General Public License v3.0 or later

#include <arm_math.h>
#include "lcd.h"
#include "global.h"

#include "morseDecode.h"
#include "k4icy.h"
#include "tf3lj_dec.h"

rgb_lcd lcd;

//Min volume - '1' char 0x00
uint8_t custom0[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b10000,
    0b00000
};

//volume - '2' char 0x01
uint8_t custom1[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b01000,
    0b11000,
    0b00000
};

//volume - '3' char 0x02
uint8_t custom2[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00100,
    0b01100,
    0b11100,
    0b00000
};

//volume - '4' char 0x03
uint8_t custom3[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00010,
    0b00110,
    0b01110,
    0b11110,
    0b00000
};

//Max volume - '5' char 0x04
uint8_t custom4[8] = {
    0b00000,
    0b00000,
    0b00001,
    0b00011,
    0b00111,
    0b01111,
    0b11111,
    0b00000
};

//Noise blanker symbol char 0x05
uint8_t custom5[8] = {
    0b00000,
    0b00100,
    0b01100,
    0b10101,
    0b00110,
    0b00100,
    0b00000,
    0b00000
};

//Autonotch filter char 0x06
uint8_t custom6[8] = {
    0b00000,
    0b10001,
    0b01010,
    0b01010,
    0b01110,
    0b00100,
    0b00100,
    0b00000
};

//Morse tune - character 0x07 - note, dynaically updated!!!
// blanck char for when we are turned off
uint8_t morsecharblank[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

// '<<'
uint8_t morsechar0[8] = {
    0b00000,
    0b00101,
    0b01010,
    0b10100,
    0b01010,
    0b00101,
    0b00000,
    0b00000
};

// '<'
uint8_t morsechar1[8] = {
    0b00000,
    0b00001,
    0b00010,
    0b00100,
    0b00010,
    0b00001,
    0b00000,
    0b00000
};

// '><'
uint8_t morsechar2[8] = {
    0b00000,
    0b10001,
    0b01010,
    0b00100,
    0b01010,
    0b10001,
    0b00000,
    0b00000
};

// '>'
uint8_t morsechar3[8] = {
    0b00000,
    0b00100,
    0b00010,
    0b00001,
    0b00010,
    0b00100,
    0b00000,
    0b00000
};

// '>>'
uint8_t morsechar4[8] = {
    0b00000,
    0b10100,
    0b01010,
    0b00101,
    0b01010,
    0b10100,
    0b00000,
    0b00000
};

//Defaults should be over-written by eeprom loading.
uint8_t lcd_colourR = 32;
uint8_t lcd_colourG = 32;
uint8_t lcd_colourB = 32;

void lcd_setcolour(void) {
  lcd.setRGB(lcd_colourR, lcd_colourG, lcd_colourB);  
}

void lcd_setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  lcd_setcolour();
  lcd.createChar(0, custom0);
  lcd.createChar(1, custom1);
  lcd.createChar(2, custom2);  
  lcd.createChar(3, custom3);  
  lcd.createChar(4, custom4);  
  lcd.createChar(5, custom5);  
  lcd.createChar(6, custom6);  
  lcd.createChar(7, morsecharblank);  //Default blank - off
}

void updateDisplay(void)
{
  char buf[17];
  int invol;
  
/* Sequence goes something like:
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|v|F|N|N|A| |NR   | |C P U| |W  |
|o|i|B|o|G| | | | | | | | | |P  |
|l|l|l|t|C| | | | | | | | | |M  |
|u|t|a|c| | | | | | | | | | | | |
|m|e|n|h| | | | | | | | | | | | |
|e|r|k| | | | | | | | | | | | | |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

CPU is updated in the main loop.
*/

  //Input volume (peak) bar graph - 5 'special' chars, from 0x00-0x04
  //Bump by '10%', as we'd like to show 90+% as a full whack clipping
  invol = (char)(((input_peak_acc+0.5)/10.0)*4.0);
  if (invol > 4) invol = 4;

  //Special case - we cannot write character '0x00' using a string write!
  //So, substitute here for a non-zero placeholder, and write it singly at the
  // end if we need....
  if (invol == 0 ) buf[0] = ' ';
  else buf[0] = invol;

  if (nr_mode != NR_MODE_COMPLETE_BYPASS ) {
    //Filter mode
    switch (current_filter_mode) {
      case 0:   //passthrough (no filter)
        buf[1] = 0x7E;  //right arrow
        break;
        
      case 1:   //SSB
        buf[1] = 'S';
        break;
        
      case 2:   //CW
        buf[1] = 'C';
        break;
        
      case 3:   //AM
        buf[1] = 'A';
        break;
        
      case 4:   //FM
        buf[1] = 'F';
        break;
  
      default:
        buf[1] = '#';
        break;
    }
  
    //Noise blanker
    if (nb_enabled) buf[2] = 0x05;
    else buf[2] = '-';
  
    //Auto notch filter
    if (xanr_notch) buf[3] = 0x06;
    else buf[3] = '-';
  
    //AGC
    switch(agc_mode) {
  
      case AGC_MODE_OFF:
        buf[4] = '-';
        break;
        
      case AGC_MODE_TRACK:
        buf[4] = 'T';
        break;
        
      case AGC_MODE_SG5K:
        buf[4] = '5';
        break;
        
      default:
        buf[4] = '#';
        break;
    }
  
    //Spacer
    buf[5] = ' ';
  } else {
    //Full bypass mode!
    buf[1] = '#';
    buf[2] = '#';
    buf[3] = '#';
    buf[4] = '#';
    buf[5] = '#';
  }

  //Noise reduction
  switch(nr_mode) {

  case NR_MODE_COMPLETE_BYPASS:   //Complete audio processing bypass
    buf[6] = 'B';
    buf[7] = 'Y';
    buf[8] = 'P';
    break;
  

  case NR_MODE_OFF:   //off
    buf[6] = 'O';
    buf[7] = 'f';
    buf[8] = 'f';
    break;

  case NR_MODE_LMS:   //LMS
    buf[6] = 'L';
    buf[7] = 'M';
    buf[8] = 'S';
    break;

  case NR_MODE_KIM:   //Kim
    buf[6] = 'K';
    buf[7] = 'i';
    buf[8] = 'm';
    break;
    
  case NR_MODE_FNR:   //fnr
    buf[6] = 'f';
    buf[7] = 'n';
    buf[8] = 'r';
    break;
    
  case NR_MODE_FNRA:   //fnrA
    buf[6] = 'f';
    buf[7] = 'n';
    buf[8] = 'A';
    break;
    
  case NR_MODE_SPECTRAL:   //Spectral
    buf[6] = 'S';
    buf[7] = 'p';
    buf[8] = 'c';
    break;
    
  case NR_MODE_LLMS:   //Leaky LMS
    buf[6] = 'L';
    buf[7] = 'L';
    buf[8] = 'M';
    break;
    
  default:   //Unknown
    buf[6] = '#';
    buf[7] = '#';
    buf[8] = '#';
    break;
  }

  //Spacer
  buf[9] = ' ';

  //CPU date lives in buf[10-12] - is filled out (overwritten) in main loop.
  buf[10] = ' ';
  buf[11] = ' ';
  buf[12] = ' ';


  //Decoder speeds and things
  if (decoder_mode == DECODER_MORSE) {
    buf[13] = 0x07; //The special morse tuning char
    sprintf(&buf[14], "%2d", morseWPM() );
  } else
  if (decoder_mode == DECODER_MORSE_K4ICY) {
    buf[13] = 0x07; //The special morse tuning char
    sprintf(&buf[14], "%2d", k4icy_getWPM() );
  } else
  if (decoder_mode == DECODER_MORSE_TF3LJ) {
    buf[13] = 0x07; //The special morse tuning char
    sprintf(&buf[14], "%2d", tf3lj_getWPM() );
  } else {
    //spacer
    buf[13] = ' ';
    buf[14] = '-';
    buf[15] = '-';
  }

  //And finally print it out
  buf[16]='\0';
  lcd.setCursor(0, 1);
  lcd.print(buf);

  //And do we need to write the input volume by hand??
  if (invol == 0 ) {
    lcd.setCursor(0, 1);
    lcd.print((char)invol);
  }
}
