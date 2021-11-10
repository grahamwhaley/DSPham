// SPDX-License-Identifier: GNU General Public License v3.0 or later

#include <Arduino.h>
#include <EEPROM.h>
#include <arm_math.h>

#include "lcd.h"
#include "dspfilter.h"
#include "menu.h"
#include "morseGen.h"
#include "nr_kim.h"
#include "spectral.h"
#include "xanr.h"
#include "LMS_NR.h"
#include "global.h"

#include "settings.h"

void load_user_ee_slot(int slot, struct settings *s);
void getSettingsName(int slot, char *cp);

struct eedata live_settings;

struct eedata factory_default_settings =
{
  EE_FINGERPRINT,
  {
    {
      "FM",    //Name //0
      { //Filter
        0,  //lowfreq
        0, //highfreq
        FILTER_FM     //preset
      },
      { //nb
        1   //on/off
      },
      { //autonotch
        0,    //twomu
        0,    //gamma
        0,    //taps
        0,    //delay
        1     //on/off
      },
      { //agc
        0,    //attack
        0,    //decay
        AGC_MODE_SG5K,    //on/off
      },
      { //nr
        0.0,  //gensetting1
        0.0,  //gensetting2
        NR_MODE_SPECTRAL     //on/off
      },
      { //decoder
        DECODER_OFF     //on/off
      },
    },
    {
      "AM",    //Name //1
      { //Filter
        0,  //lowfreq
        0, //highfreq
        FILTER_AM     //preset
      },
      { //nb
        1   //on/off
      },
      { //autonotch
        0,    //twomu
        0,    //gamma
        0,    //taps
        0,    //delay
        1     //on/off
      },
      { //agc
        0,    //attack
        0,    //decay
        AGC_MODE_SG5K,    //on/off
      },
      { //nr
        0.0,  //gensetting1
        0.0,  //gensetting2
        NR_MODE_SPECTRAL     //on/off
      },
      { //decoder
        DECODER_OFF     //on/off
      },
    },
    {
      "OFF",  //Name  //2
      { //Filter
        0,  //lowfreq
        0, //highfreq
        FILTER_PASSTHRU     //preset
      },
      { //nb
        0   //on/off
      },
      { //autonotch
        0,    //twomu
        0,    //gamma
        0,    //taps
        0,    //delay
        0     //on/off
      },
      { //agc
        0,    //attack
        0,    //decay
        0,    //on/off
      },
      { //nr
        0.0,  //gensetting1
        0.0,  //gensetting2
        NR_MODE_OFF     //on/off
      },
      { //decoder
        DECODER_OFF     //on/off
      },
    },
    { //Just the hardware AGC - everything else off!
      "SG5K",  //Name  //3
      { //Filter
        0,  //lowfreq
        0, //highfreq
        FILTER_PASSTHRU     //preset
      },
      { //nb
        0   //on/off
      },
      { //autonotch
        0,    //twomu
        0,    //gamma
        0,    //taps
        0,    //delay
        0     //on/off
      },
      { //agc
        0,    //attack
        0,    //decay
        AGC_MODE_SG5K,    //on/off
      },
      { //nr
        0.0,  //gensetting1
        0.0,  //gensetting2
        NR_MODE_OFF     //on/off
      },
      { //decoder
        DECODER_OFF     //on/off
      },
    },
    {
      "SSB",    //Name  //4
      { //Filter
        0,  //lowfreq
        0, //highfreq
        FILTER_SSB     //preset
      },
      { //nb
        1   //on/off
      },
      { //autonotch
        0,    //twomu
        0,    //gamma
        0,    //taps
        0,    //delay
        1     //on/off
      },
      { //agc
        0,    //attack
        0,    //decay
        AGC_MODE_SG5K,    //on/off
      },
      { //nr
        0.0,  //gensetting1
        0.0,  //gensetting2
        NR_MODE_SPECTRAL     //on/off
      },
      { //decoder
        DECODER_OFF     //on/off
      },
    },
    {
      "CW",   //Name  //5
      { //Filter
        0,  //lowfreq
        0, //highfreq
        FILTER_CW     //preset
      },
      { //nb
        1   //on/off - NB is OK with CW - only removes very short clicks
      },
      { //autonotch
        0,    //twomu
        0,    //gamma
        0,    //taps
        0,    //delay
        0     //on/off - autonotch can destroy CW - turn off
      },
      { //agc
        0,    //attack
        0,    //decay
        0,    //on/off - FIXME - decide, do we need AGC with CW ?
      },
      { //nr
        0.0,  //gensetting1
        0.0,  //gensetting2
        NR_MODE_OFF     //on/off - no NR for CW
      },
      { //decoder
        DECODER_MORSE_K4ICY     //on/off - K4ICY best balance of useful vs accurate etc.
      },
    },
    {
      "\0x00\0x00\0x00" //name //6
    },
    {
      "\0x00\0x00\0x00" //name //7
    },
    {
      "\0x00\0x00\0x00" //name //8
    },
    {
      "\0x00\0x00\0x00" //name //9
    },
    {
      "\0x00\0x00\0x00" //name //10
    },
  },
  { //lcd colour
    32, 32, 32, //rgb
    50  //brightness
  },
  0.5,  //volume
  4     //Default slot SSB
};

static int current_setting = 4; //SSB. FIXME - load from eedata at init time

static void set_setting(struct settings *s) {
  //Filter
  current_filter_mode = s->filter.preset;
  if (s->filter.lowfreq != 0 )    //FIXME - what if we *do* want to set the freq to 0hz ?
    filterList[current_filter_mode].freqLow = s->filter.lowfreq;
  if (s->filter.highfreq != 0 ) 
    filterList[current_filter_mode].freqHigh = s->filter.highfreq;
  //Generate and set the FIR
  updateFilter();
  
  //nb
  nb_enabled = s->nb.nb_mode;
  
  //autonotch

  if (s->autonotch.twomu != 0 )
    ANR_two_mu = s->autonotch.twomu;
  if (s->autonotch.gamma != 0 )  
    ANR_gamma = s->autonotch.gamma;  
  if (s->autonotch.taps != 0 )
    ANR_taps = s->autonotch.taps;
  if (s->autonotch.delay != 0 )
    ANR_delay = s->autonotch.delay;
    
  xanr_notch = s->autonotch.autonotch_mode;
  
  //agc
  agc_mode = s->agc.agc_mode;
  if (agc_mode == AGC_MODE_SG5K ) {
      agc_sg5k_attack = s->agc.agc_attack;
      agc_sg5k_decay = s->agc.agc_decay;
  }
  updateAGC();
  
  //nr
  set_nr_mode(s->nr.nr_mode);
  
  //decoder
  decoder_mode = s->decoder.decoder_mode;
  morseLed(false);  //And turn the LED off - just in case it was on when we disabled a decoder.
}

static void load_setting(int slot) {
  char buf[17];
  struct settings *s = &live_settings.slots[slot];
  
  memset(buf, ' ', 16);
  buf[16]='\0';
  sprintf(buf, "%02d", slot);
  buf[2] = ' ';

  if ( (s->name[0] == '\0') || (s->name[0] == 0xff) ) { //Empty settings slot
    buf[3] = 'E';  
    buf[4] = 'm';  
    buf[5] = 'p';  
    buf[6] = 't';  
    buf[7] = 'y';  
    lcd.setCursor(0, 0);
    lcd.print(buf);
    return;   //Nothing to load
  } else {
    for( int i=0; i<sizeof(s->name); i++)
      if (s->name[i] != '\0') buf[3+i] = s->name[i];
  }
  
  lcd.setCursor(0, 0);
  lcd.print(buf);

  sgtl5000_1.muteHeadphone();
  sgtl5000_1.muteLineout();
  set_setting(s);
  sgtl5000_1.unmuteHeadphone();
  sgtl5000_1.unmuteLineout();
}

void load_next_settings(void) {
  current_setting++;  
  if (current_setting >= MAX_EE_SLOTS ) current_setting = 0; //Safe, as 0 is always a valid static setting
  load_setting(current_setting);
}

void load_previous_settings(void) {
  current_setting--;  
  if (current_setting < 0 ) current_setting = MAX_EE_SLOTS-1;
  load_setting(current_setting);
}

void load_specific_settings(uint8_t slot) {
  current_setting = slot;  
  if (current_setting >= MAX_EE_SLOTS ) current_setting = 0; //Safe, as 0 is always a valid static setting
  load_setting(current_setting);
}

//Copy the current live settings into a slot, and write it to eeprom
void saveSetting(int slot) {
  struct settings *s = &live_settings.slots[slot];
  int eeoffset;

  //Just blindly copy the whole slot to the eeprom - initialised or not..  
  s->filter.lowfreq = filterList[current_filter_mode].freqLow;
  s->filter.highfreq = filterList[current_filter_mode].freqHigh;
  s->filter.preset = current_filter_mode;
  
  s->nb.nb_mode = nb_enabled;
  
  s->autonotch.twomu = ANR_two_mu;
  s->autonotch.gamma = ANR_gamma;
  s->autonotch.taps = ANR_taps;
  s->autonotch.delay = ANR_delay;
  s->autonotch.autonotch_mode = xanr_notch;
  
  s->agc.agc_mode = agc_mode;

  if( agc_mode == AGC_MODE_SG5K ) {
    s->agc.agc_attack = agc_sg5k_attack;
    s->agc.agc_decay = agc_sg5k_decay;
  }
  
  s->nr.nr_setting1 = 0;   //FIXME - is dependant on the mode! We probably want a union for these?
  s->nr.nr_setting2 = 0;
  s->nr.nr_mode = nr_mode;
  
  s->decoder.decoder_mode = decoder_mode;

  //The name field should already be filled out correctly from either loading the defaults
  //or from a previous call to setSettingsName()

  eeoffset = offsetof(eedata, slots);
  eeoffset += sizeof(struct settings) * slot; //user data at front of eeprom. Slots indexed from 0

  morseLed(true);
  for (int i=0; i<sizeof(struct settings); i++ ) {
    EEPROM.write(eeoffset+i, ((uint8_t *)s)[i]);
  }
  //Keep the led on a little as a 'did the write' indicator
  delay(250);
  morseLed(false);
}

void getSettingsName(int slot, char *cp) {
  char *p = live_settings.slots[slot].name;
    
  for (int i=0; i<SETTING_NAME_LENGTH; i++) cp[i] = p[i];
}

//Set the name in the live settings, but do not write it to the eeprom
// that is done with the global 'save slot' call.
void setSettingsName(int slot, char *cp) {
  char *p = live_settings.slots[slot].name;

  //Set the name in the live data
  for (int i=0; i<SETTING_NAME_LENGTH; i++) p[i] = cp[i];
}

void init_settings(void) {
  uint32_t fingerprint;

  for(int i=0; i<sizeof(fingerprint); i++ )
  {
    ((char *)&fingerprint)[i] = EEPROM.read(i);
  }

  if (fingerprint != EE_FINGERPRINT) {
    live_settings = factory_default_settings;
  } else {
    for(int i=0; i<sizeof(live_settings); i++ )
    {
      ((char *)&live_settings)[i] = EEPROM.read(i);
    }
  }
}

void load_volume(void) {
  int eeoffset = offsetof(eedata, volume);

  sgtl5000_1.muteHeadphone();
  sgtl5000_1.muteLineout();
  for( int i=0; i<sizeof(global_volume); i++ ) {
    ((char *)&global_volume)[i] = EEPROM.read(eeoffset+i);
  }
  sgtl5000_1.volume(global_volume);
  sgtl5000_1.unmuteHeadphone();
  sgtl5000_1.unmuteLineout();  
}

void save_volume(void) {
  int eeoffset = offsetof(eedata, volume);

  for( int i=0; i<sizeof(global_volume); i++ ) {
    EEPROM.write(eeoffset+i, ((char *)&global_volume)[i]);
  }
}

void load_colour(void) {
  int eeoffset = offsetof(eedata, lcd.red);

  for( int i=0; i<sizeof(lcd_colourR); i++ ) {
    ((char *)&lcd_colourR)[i] = EEPROM.read(eeoffset+i);
  }

  eeoffset = offsetof(eedata, lcd.green);
  for( int i=0; i<sizeof(lcd_colourG); i++ ) {
    ((char *)&lcd_colourG)[i] = EEPROM.read(eeoffset+i);
  }

  eeoffset = offsetof(eedata, lcd.blue);
  for( int i=0; i<sizeof(lcd_colourB); i++ ) {
    ((char *)&lcd_colourB)[i] = EEPROM.read(eeoffset+i);
  }

  lcd_setcolour();
}

void save_colour(void) {
  int eeoffset = offsetof(eedata, lcd.red);

  for( int i=0; i<sizeof(lcd_colourR); i++ ) {
    EEPROM.write(eeoffset+i, ((char *)&lcd_colourR)[i]);
  }
  
  eeoffset = offsetof(eedata, lcd.green);
  for( int i=0; i<sizeof(lcd_colourG); i++ ) {
    EEPROM.write(eeoffset+i, ((char *)&lcd_colourG)[i]);
  }
  
  eeoffset = offsetof(eedata, lcd.blue);
  for( int i=0; i<sizeof(lcd_colourB); i++ ) {
    EEPROM.write(eeoffset+i, ((char *)&lcd_colourB)[i]);
  }
}

//Write the defaults to the whole eeprom...
void factory_reset(void) {
  morseLed(true);
  for(int i=0; i<sizeof(factory_default_settings); i++ )
  {
    EEPROM.write(i, ((char *)&factory_default_settings)[i]);
  }

  //Keep the led on a little as a 'did the write' indicator
  delay(250);
  morseLed(false);

  //And now re-load them in...
  init_settings();
}

//Write the defaults to one slot only
void factory_reset_slot(int slot) {
  int eeoffset;
  
  eeoffset = offsetof(eedata, slots);
  eeoffset += sizeof(struct settings) * slot;
  
  morseLed(true);
  for(int i=0; i<sizeof(struct settings); i++ )
  {
    EEPROM.write(eeoffset+i, ((char *)&factory_default_settings.slots[slot])[i]);
  }

  //Keep the led on a little as a 'did the write' indicator
  delay(250);
  morseLed(false);

  live_settings.slots[slot] = factory_default_settings.slots[slot];
  //NOTE - if the slot you are resetting is the slot that is currently 'active', it does
  // **NOT** automatically get re-loaded - but will next time you re-select it.
}

uint8_t get_default_slot(void) {
  return live_settings.default_mode; 
}

void set_default_slot(uint8_t slot) {
  live_settings.default_mode = slot;

  EEPROM.write(offsetof(eedata, default_mode), live_settings.default_mode);
}

void set_nr_mode(uint8_t mode) {

  //If not valid, just leave as is.
  if (mode > NR_MODE_MAX) return;

  nr_mode = mode;

  switch(nr_mode) {
    case NR_MODE_KIM:
      nr_kim_init();
      break;

    case NR_MODE_LMS:
      Init_LMS_NR();
      break;
    
    case NR_MODE_SPECTRAL:
      spectral_noise_reduction_init();
      break;
      
    case NR_MODE_LLMS:
      xanr_init();
      break;

    //All other modes don't need any extra (re-)initialisation
    default:
      break;
  }
}
