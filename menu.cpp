// Example take from https://github.com/neu-rah/ArduinoMenu/
// Original license LGPL 2.1+

#include <menu.h>
#include <menuIO/groveRGBLCDOut.h>
#include <menuIO/chainStream.h>
#include <menuIO/clickEncoderIn.h>

#include <arm_math.h>

#include "global.h"
#include "LMS_NR.h"
#include "nr_kim.h"
#include "ik8yfw.h"
#include "morseGen.h"
#include "dynamicFilters.h"
#include "dspfilter.h"
#include "lcd.h"
#include "settings.h"

using namespace Menu;

extern rgb_lcd lcd;

#define encA 3
#define encB 2
#define encBtn 4
ClickEncoder clickEncoder(encA,encB,encBtn,2);
ClickEncoderStream encStream(clickEncoder,1);

#define LEDPIN LED_BUILTIN
#define MAX_DEPTH 3   /// wth does this mean???


// NR_alpha affects both Kim and spectral NR
void updateNRAlpha() {
  NR_onemalpha = (1.0 - NR_alpha);
}

void updateNRBeta() {
  NR_onemtwobeta = (1.0 - (2.0 * NR_beta));
}

void updateKim() {
  NR_beta = 0.25;
  updateNRBeta();
  set_nr_mode(NR_MODE_KIM);
}

void updateSpectral() {
  NR_beta = 0.85;
  updateNRBeta();
  set_nr_mode(NR_MODE_SPECTRAL);
}

//FIXME - we may need to tweak some of the NR settings when we change mode.
//Hook the funcs here to make that happen.
//FIXME - we should really use/call set_nr_mode to make the real change, as it
// will then call the right init routine at the right time - but, we have hacked
// in for Kim and Spectral above - an interim solution.
CHOOSE(nr_mode,NRmodeMenu,"NR Mode",doNothing,noEvent,noStyle
  ,VALUE("Off",NR_MODE_OFF,doNothing,noEvent)
  ,VALUE("LMS",NR_MODE_LMS,doNothing,noEvent)
  ,VALUE("Kim",NR_MODE_KIM,updateKim,enterEvent | exitEvent | updateEvent)
  ,VALUE("fnr",NR_MODE_FNR,doNothing,noEvent)
  ,VALUE("fnrA",NR_MODE_FNRA,doNothing,noEvent)
  ,VALUE("Spectral",NR_MODE_SPECTRAL,updateSpectral,enterEvent | exitEvent | updateEvent)
  ,VALUE("LLMS",NR_MODE_LLMS,doNothing,enterEvent | exitEvent | updateEvent)
);

MENU(NRTweaksMenu, "NR tweaks", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,FIELD(LMS_nr_strength,"LMS strength","",LMS_MIN_STRENGTH,LMS_MAX_STRENGTH,1,0,Init_LMS_NR,enterEvent | exitEvent | updateEvent,noStyle)
  ,FIELD(NR_KIM_K,"Kim str","",KIM_NR_KIM_K_MIN,KIM_NR_KIM_K_MAX,0.025,0.0,doNothing,noEvent,noStyle)
  ,FIELD(NR_PSI,"Kim PSI","",KIM_NR_PSI_MIN,KIM_NR_PSI_MAX,0.1,0.0,doNothing,noEvent,noStyle)
  ,FIELD(NR_alpha,"Kim alpha","",KIM_NR_ALPHA_MIN,KIM_NR_ALPHA_MAX,0.01,0.0,updateNRAlpha,enterEvent | exitEvent | updateEvent,noStyle)
  ,FIELD(NR_beta,"Kim beta","",KIM_NR_BETA_MIN,KIM_NR_BETA_MAX,0.01,0.0,updateNRBeta,enterEvent | exitEvent | updateEvent,noStyle)
  ,FIELD(fnr_level,"FNR level","",FNR_LEVEL_MIN,FNR_LEVEL_MAX,1,0,doNothing,noEvent,noStyle)
  ,FIELD(fnra_level,"FNRA level","",FNRA_LEVEL_MIN,FNRA_LEVEL_MAX,1,0,doNothing,noEvent,noStyle)
  ,EXIT("<Back")
);

CHOOSE(xanr_notch,NotchMenu,"Notch Mode",doNothing,noEvent,noStyle
  ,VALUE("Off",0,doNothing,noEvent)
  ,VALUE("On",1,doNothing,noEvent)
);

CHOOSE(nb_enabled,NBMenu,"Blnkr Mode",doNothing,noEvent,noStyle
  ,VALUE("Off",0,doNothing,noEvent)
  ,VALUE("On",1,doNothing,noEvent)
);

MENU(NRMenu, "NR menu", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,SUBMENU(NRmodeMenu)
  ,SUBMENU(NRTweaksMenu)
  ,SUBMENU(NotchMenu)
  ,SUBMENU(NBMenu)
  ,EXIT("<Back")
);

CHOOSE(decoder_mode,DecoderModeMenu,"Dcdr Mode",doNothing,noEvent,noStyle
  ,VALUE("Off",0,doNothing,noEvent)
  ,VALUE("Morse",DECODER_MORSE,doNothing,noEvent)
  ,VALUE("K4ICY",DECODER_MORSE_K4ICY,doNothing,noEvent)
  ,VALUE("TF3LJ",DECODER_MORSE_TF3LJ,doNothing,noEvent)
);

MENU(DecoderTweaksMenu, "Dcdr tweak", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,FIELD(morse_frequency,"CW Freq","",1,1000,10,1,morseInit,enterEvent | exitEvent | updateEvent,noStyle)
  ,FIELD(morse_threshold,"CW Tsh","",0,1,0.01,0.0,morseInit,enterEvent | exitEvent | updateEvent,noStyle)
  ,FIELD(morse_cycles,"CW cyc","",1,100,1.0,0.0,morseInit,enterEvent | exitEvent | updateEvent,noStyle)
  ,EXIT("<Back")
);

MENU(DecoderMenu, "Decoder menu", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,SUBMENU(DecoderModeMenu)
  ,SUBMENU(DecoderTweaksMenu)
  ,EXIT("<Back")
);

int current_filter_mode = 0;
double filter_freqhi;
double filter_freqlo;

void updateFilter() {
  int ncoeff = filterList[current_filter_mode].coeff;
  float32_t centref = (filterList[current_filter_mode].freqLow + filterList[current_filter_mode].freqHigh) / 2.0;
  const float32_t maxgain = 2.0;
  float32_t gain;
  
  audioFilter(fir_active1,
    ncoeff,
    filterList[current_filter_mode].filterType,
    filterList[current_filter_mode].window,
    filterList[current_filter_mode].freqLow,
    filterList[current_filter_mode].freqHigh );

  gain = getFilterGain(fir_active1, ncoeff, centref, SAMPLE_RATE/(uint32_t)DF);

  //For CW filters at least, we get a tiny gain - requiring a multiplication factor of
  // ~167 in some cases for instance - and this seems to make the FIR filter then just generate
  // hiss and noise, even when fed no signal.
  // Thus, try limiting the max 'gain' to something 'sensible'. Well, OK, I'd like it so we never
  // need to apply any gain to the FIR coefficients in the first place, but that is not what we seem
  // to get from the FIR dynamic calculators.
  if (1.0/gain > maxgain) gain = 1.0/maxgain;

  //And scale it so we try not to be at 100% for a pure signal, to try and avoid
  // any potential clipping (unlikely it is that we will ever end up in that situation).
  normaliseCoeffs(fir_active1, ncoeff, 0.9/gain);

  firfilter.begin(fir_active1, ncoeff);

  filter_freqlo = filterList[current_filter_mode].freqLow;
  filter_freqhi = filterList[current_filter_mode].freqHigh;
}

void updateFilterVars() {
    filterList[current_filter_mode].freqLow = filter_freqlo;
    filterList[current_filter_mode].freqHigh = filter_freqhi;

    updateFilter();
}

//Keep these in sync with the table in dspfilter.h
CHOOSE(current_filter_mode,filterModeMenu,"Flt Md",updateFilter,enterEvent,noStyle
  ,VALUE("PThru",0,updateFilter,enterEvent)
  ,VALUE("SSB",1,updateFilter,enterEvent)
  ,VALUE("CW",2,updateFilter,enterEvent)
  ,VALUE("AM",3,updateFilter,enterEvent)
  ,VALUE("FM",4,updateFilter,enterEvent)
);

MENU(filterTweaksMenu, "Filter Tweaks", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,FIELD(filter_freqlo,"Flt Lo","Hz",0,20000,100,1,updateFilterVars,enterEvent | exitEvent | updateEvent,noStyle)
  ,FIELD(filter_freqhi,"Flt Hi","Hz",0,20000,100,1,updateFilterVars,enterEvent | exitEvent | updateEvent,noStyle)
  ,EXIT("<Back")
);

MENU(FilterMenu, "Filter menu", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,SUBMENU(filterModeMenu)
  ,SUBMENU(filterTweaksMenu)
  ,EXIT("<Back")
);

void audio_mute() {
      sgtl5000_1.muteHeadphone();
      sgtl5000_1.muteLineout();
}

void audio_unmute() {
      sgtl5000_1.unmuteHeadphone();
      sgtl5000_1.unmuteLineout();
}

void updateAGC() {
  if (agc_mode == AGC_MODE_SG5K) {
    audio_mute();
    sgtl5000_1.autoVolumeEnable();
    //FIXME - this should be a more global decision if we start to use the
    // built in filters or equalisers for instance
    sgtl5000_1.audioPostProcessorEnable();
    sgtl5000_1.autoVolumeControl(
      agc_sg5k_maxGain,
      agc_sg5k_response,
      agc_sg5k_hardLimit,
      agc_sg5k_threshold,
      agc_sg5k_attack,
      agc_sg5k_decay );
    audio_unmute();
  } else {
    audio_mute();
    sgtl5000_1.autoVolumeDisable();    
    //FIXME - this should be a more global decision if we start to use the
    // built in filters or equalisers for instance
    sgtl5000_1.audioProcessorDisable();    
    audio_unmute();
  }
}

//Keep these in sync with the table in global.h
CHOOSE(agc_mode,AGCChoose,"AGC Typ",doNothing,noEvent,noStyle
  ,VALUE("Off",AGC_MODE_OFF,updateAGC,enterEvent | exitEvent | updateEvent)
  ,VALUE("Track",AGC_MODE_TRACK,updateAGC,enterEvent | exitEvent | updateEvent)
  ,VALUE("SG5K",AGC_MODE_SG5K,updateAGC,enterEvent | exitEvent | updateEvent)
);

MENU(AGCMenu, "AGC Menu", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,SUBMENU(AGCChoose)
  ,EXIT("<Back")
);

void set_volume(eventMask e) {
  switch(e) {
    //live update as it is adjusted by the user.
    case updateEvent:
      sgtl5000_1.volume(global_volume);
      break;
    //Save to eeprom once user had made a choice.
    case exitEvent:
      sgtl5000_1.volume(global_volume);
      save_volume();
      break;
  }
}

MENU(VolumeMenu, "Volume", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,FIELD(global_volume,"HdVol","",0.0,1.0,0.05,0.01,set_volume,enterEvent | exitEvent | updateEvent,noStyle)
  ,EXIT("<Back")
);

int active_settings_slot = 0;
//Only allow nums and lower case to reduce scrolling/choice
char* constMEM alphaNum MEMMODE="0123456789abcdefghijklmnopqrstuvwxyz";
char* constMEM alphaNumMask[] MEMMODE={alphaNum};
char active_slot_name[]="0123";   //Name of the currently selected editing slot

void menu_save_settings() {
  saveSetting(active_settings_slot);
}

void menu_save_settings_name() {
  setSettingsName(active_settings_slot, active_slot_name);
}

void load_slot_name() {
  getSettingsName(active_settings_slot, active_slot_name);
}

char* constMEM hexNum MEMMODE="0123456789ABCDEF";
char* constMEM hexNumMask[] MEMMODE={hexNum};
char screen_colour_hex[]="404040";   //hex representation of the screen colour in RGB

void menu_set_colour(eventMask e) {
  long int l;
  uint8_t r, g, b;
  char str[3], *ep;

  if (e != enterEvent ) {
    str[2] = '\0';
    str[0] = screen_colour_hex[0];
    str[1] = screen_colour_hex[1];
    l = strtol(str, &ep, 16);
    r = (uint8_t)l;
  
    str[0] = screen_colour_hex[2];
    str[1] = screen_colour_hex[3];
    l = strtol(str, &ep, 16);
    g = (uint8_t)l;
  
    str[0] = screen_colour_hex[4];
    str[1] = screen_colour_hex[5];
    l = strtol(str, &ep, 16);
    b = (uint8_t)l;
  
    lcd_colourR = r;
    lcd_colourG = g;
    lcd_colourB = b;
  }
  
  switch(e) {
    //Update event doesn't get driven for EDIT fields it seems :-(
    case enterEvent:
      sprintf(screen_colour_hex, "%02x%02x%02x", lcd_colourR, lcd_colourG, lcd_colourB);
      break;
    //case updateEvent:
      //lcd_setcolour();
      //break;
    //Save to eeprom once user had made a choice.
    case exitEvent:
      save_colour();
      lcd_setcolour();
      break;
  }
}

void slot_reset(void) {
  factory_reset_slot(active_settings_slot);
}

MENU(SettingsMenu, "Settings", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,FIELD(active_settings_slot,"Edit Slot","",0,MAX_EE_SLOTS-1,1,0,load_slot_name,enterEvent | exitEvent | updateEvent,noStyle)  //Set slot to edit
  ,EDIT("Slot Name",active_slot_name,alphaNumMask,menu_save_settings_name,exitEvent,noStyle) //edit slot name
  ,OP("Save Slot",menu_save_settings,enterEvent) //save data to selected slot
  ,OP("Reset slot",slot_reset,enterEvent) //rewrite one slot back to defaults
  ,OP("Factory reset",factory_reset,enterEvent) //rewrite the whole eeprom back to defaults
  ,EDIT("ScrnRGB",screen_colour_hex,hexNumMask,menu_set_colour,exitEvent | updateEvent | enterEvent,noStyle) //edit screen colour
  ,OP("Ver: " VERSION_STRING,doNothing,noEvent)
  ,EXIT("<Back")
);

MENU(mainMenu, "Top menu", Menu::doNothing, Menu::noEvent, Menu::wrapStyle
  ,SUBMENU(VolumeMenu)
  ,SUBMENU(NRMenu)
  ,SUBMENU(DecoderMenu)
  ,SUBMENU(FilterMenu)
  ,SUBMENU(AGCMenu)
  ,SUBMENU(SettingsMenu)
  ,EXIT("<Back")
);

MENU_INPUTS(in,&encStream);

MENU_OUTPUTS(out,MAX_DEPTH
  ,GROVERGBLCD_OUT(lcd, {0, 0, 16, 2})
  ,NONE//must have 2 items at least
);

NAVROOT(nav,mainMenu,MAX_DEPTH,in,out);

void menu_setup(void) {
  //FIXME - a bit of a hack - but we need to load the AGC defaults somewhere.
  updateAGC();  // yes, maybe we need a separate agc.cpp etc.
  pinMode(encBtn,INPUT_PULLUP);
  getSettingsName(active_settings_slot, active_slot_name);  //Set name for default edit slot
  nav.useUpdateEvent=true;
  nav.idleOn();  //do not display at startup
}

bool blink(int timeOn,int timeOff) {return millis()%(unsigned long)(timeOn+timeOff)<(unsigned long)timeOn;}

// Returns true if menu is active.
bool menu_poll(void) {
  clickEncoder.service();
  nav.poll();
  return nav.sleepTask == NULL;
}
