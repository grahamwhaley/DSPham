// SPDX-License-Identifier: GNU General Public License v3.0 or later

#include "rgb_lcd.h"

extern rgb_lcd lcd;
extern void lcd_setup();
extern void updateDisplay(void);

extern uint8_t morsechar0[8];
extern uint8_t morsechar1[8];
extern uint8_t morsechar2[8];
extern uint8_t morsechar3[8];
extern uint8_t morsechar4[8];

extern uint8_t lcd_colourR, lcd_colourG, lcd_colourB;
extern void lcd_setcolour(void);
