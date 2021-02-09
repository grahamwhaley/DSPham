// SPDX-License-Identifier: GNU General Public License v3.0 or later

//Note, structs in this file have elements ordered to try and reduce structure
//size - packing. Thus, sometimes they look to be in a strange order. Rule of
//thumb is larger items first in the struct - results in less 'gaps'. eeprom
//space is *precious*.
// If we end up with too many gaps, we can 'pack' these and then write load/unload
//functions if we need to.

struct filter_settings {
	uint16_t lowfreq;	//low end of the bandpass
	uint16_t highfreq;	//high end of the bandpass
	uint8_t preset;		//Default system slot to load (ssb, cw etc.)
};

struct nb_settings {
	uint8_t nb_mode;	//on or off
};

struct autonotch_settings {
	float32_t twomu;
	float32_t gamma;
	uint8_t taps;
	uint8_t delay;
	uint8_t autonotch_mode;	//on or off
};

struct agc_settings {
	uint32_t agc_attack;
	uint32_t agc_decay;
	uint8_t agc_mode;	//off or which?
};

struct nr_settings {
	float32_t nr_setting1;	//Generic 'setting'. Changes meaning per nr type
	float32_t nr_setting2;	//Generic 'setting'. Changes meaning per nr type
	uint8_t nr_mode;	//off or which?
};

struct decoder_settings {
	uint8_t decoder_mode;	//off or which?
};

	
struct settings {
  char name[4];
	struct filter_settings filter;
	struct nb_settings nb;
	struct autonotch_settings autonotch;
	struct agc_settings agc;
	struct nr_settings nr;
	struct decoder_settings decoder;
};

struct lcd_colour {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
};

#define EE_FINGERPRINT (0xaa550000 + VERSION_UINT16)

#define MAX_EE_SLOTS 11

struct eedata {
  uint32_t fingerprint;     //Unique identifier for our data
  struct settings slots[MAX_EE_SLOTS];
  struct lcd_colour lcd;
  float32_t volume;
  uint8_t default_mode;		//Default slot to start up with
};

extern void load_next_settings(void);   //Move on to next setting. Sequence is static followed by user, and wrap.
extern void load_previous_settings(void);   //Move down to previous setting.
extern void load_specific_settings(uint8_t slot);  //Load specific slot
extern void getSettingsName(int slot, char *cp);
extern void setSettingsName(int slot, char *cp);
extern void saveSetting(int slot);
extern void init_settings(void);
extern void factory_reset(void);
extern void factory_reset_slot(int slot);
extern void load_volume(void);
extern void save_volume(void);
extern void load_colour(void);
extern void save_colour(void);
extern uint8_t get_default_slot(void);
extern void set_default_slot(uint8_t slot);

//Bit of a hack in - really this could live in its own 'nr.cpp' file, but
// it would be pretty much the only thing in it...
extern void set_nr_mode(uint8_t mode);
