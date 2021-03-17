# DSPham

  A DSP audio processor aimed at radio amateurs

![Front panel](./photos/Front.JPG)

This project is a DSP audio processor, aimed at the radio amateurs. It's key features include:

- Multiple noise reduction algorithms
- Multiple morse (CW) decoders
- Auto-notch (tone) removal
- Noise blanker (impulse) removal
- Configurable bandpass filtering

It has audio input via line-in and USB, and audio output via line-out and headphone socket.

I built it as I had an SDR based transceiver with no built in noise reduction, and wanted to see
what was possible to improve that situation. In the spirit of amateur radio, and because my background
is in embedded programming, I didn't want to purchase a ready made unit, so I went about building and
coding up my own.

As an added bonus, the CW decoders tend to be better than the one built into my transceiver, and auto
detect key speed.

An expanded list of [Features can be found below](#feature-list).

## Honorable mentions

Here I should say, this project is 'built on the shoulders of giants'. All the technically challenging
hard math innards of the code has been derived and extracted from other projects. My task was merely
researching, collating and knitting together all the relevant parts into one functional unit with the
features I was looking for. Without the work of the below mentioned, and many others, this project
would have been orders of magnitudes more difficult, and have taken significantly longer to put
together.

Honorable mentions then to:

  - The authors and contributors to the [Teensy-ConvolutionSDR][1] project, along with the closely
    related [UHSDR][2] and [wdsp][3] projects, from which much of the noise reduction and other
    code has been derived.
  - GI1MIC and the [$19 DSP Filter][4] project, for inspiration, showing the use of a Teensy, and
    for Morse decoder and filter code.
  - [PJRC][5], [Paul Stoffregen][6], and the Teensy community for creating a fantastically usable
    powerful Arduino-like board, along with documentation, examples and excellent supporting libraries.
    Highly recommended.
  - [K4ICY][7] and [TF3LJ][8] for alternative Morse Decoders.

and many others.

## Versions

A [history of versions](#version-history) can be found at the end of this document.

## Hardware

The basic hardware is built around a [Teensy 4.0][9] board along with its matching
[audio daughtercard][10]. In addition to that there is a 2x16 screen, a single LED, a rotary click
encoder and a switch to enter reprogramming mode. That's it. Pretty simple.

![Internal view](./photos/Internals.JPG)

The heavy lifting is done by the Teensy and its audio card. I used an I2C based [Grove RGB LCD][13]
merely as that is what I had to hand. If I'd had a standard HD44780 style 2x16 display to hand then
I probably would have driven that in 4bit mode instead. See below for more on
[hardware choices](#hardware-choices).

![Circuit diagram](./diagrams/DSPham_white.png)

> *Note:* In the circuit diagram, the LCD type shown is incorrect - only because the package I used
  to draw the diagram did not have the Grove RGB LCD I2C in its library. You get the idea - there
  are only two data wires going to the LCD (along with the two power connections).
  It should also be noted that, technically, the Grove RGB LCD is a 5V part, and the Teensy is
  a 3V3 part, so having the Teensy driving the display is probably not guaranteed to work. It works
  for me, but if you are purchasing anew you may wish to try and source a 3V3 tolerante display.
  As long as the display is supported by the ["Arduino Menu"][17] system, and you can find enough
  pins to wire it to the Teensy (there are a number spare I believe), then it should be easy to
  modify the initialisation code to handle the new display. Do ask me if you need some help.

> *Note:* The LED resistor value is calculated to limit the current drawn from the Teensy 4.0,
  *depending on your chosen LED*. You may wish to re-calculate this value to suit your build,
  or possibly even better add some sort of LED driver/transitor switch circuit to further reduce
  the current draw from the Teensy. The Teensy 4.0 current draw, *total* for all pins, is
  [specified as 10mA](https://www.pjrc.com/teensy/techspecs.html). Be careful.

> *Note:* The [rotary encoder I use - a KY-040][23] came on a module (pcb) that contained
  some pullups. They are now depicted on the schematic. Hopefully if you have a different or
  bare module this will help with the wiring up.
  if you get the *DT* and *CLK* pins wired around the wrong way it will just make the rotary
  encoder 'work backwards', e.g., go anticlockwise with respect to the menu system.
  You can either fix that by swapping the wiring over, or modifying the software to tell it
  the pins are the other way around.

> **Warning:** If you are going to power the Teensy from an external power supply (such as the 7805
  I used), *and* you still want to be able to use or program over the USB port, then you **must**
  snip a track on the Teensy board to separate the VIN and VUSB - see the
  [Teensy 4.0 pinout diagram][20] for details. If you do not make the modification I believe you
  are probably in danger of **frying** something.

### Hardware choices

I built my unit with the [Grove RGB LCD][13] and a [KY-040 rotary encoder][23]. Those were just
what I had to hand or could get cheaply and easily - you may of course have other hardware to
hand you wish to use. You will 'simply' have to modify the software setup to cater for the changes.

For the LCD, that will involve modifying the [menu library][16] code found at the bottom of the
[`menu.cpp`](./menu.cpp) file, such as:

```cpp
MENU_OUTPUTS(out,MAX_DEPTH
  ,GROVERGBLCD_OUT(lcd, {0, 0, 16, 2})
  ,NONE//must have 2 items at least
);
```

See the [menu library web pages][16] for more details.

For the encoder, hopefully it will work like most other rotary shaft encoders, and will 'just work',
as long as you wire it up to the correct pins. But if not, you may need to modify the encoder and
button code, both near the top of the [`menu.cpp`](./menu.cpp) code:

```cpp
#define encA 3
#define encB 2
#define encBtn 4
ClickEncoder clickEncoder(encA,encB,encBtn,2);
ClickEncoderStream encStream(clickEncoder,1);
```

and near the top of the [`DSPham.ino`](./DSPham.ino) file:

```cpp
Encoder enc1(3, 2);
```

You can both modify which pins the encoder is wired to, which way around the pins are wired
(which effects clock/anti-clockwise rotation wrt to the menu). You may also need to change the
number of clicks per detent for your chosen encoder - that can be changed with the `stepsPerNotch`
parameter to the `ClickEncoder` invocation. For reference, my encoder seems to have 30 detents
per 360 degrees, and 2 steps per click (60 steps for a 360 degree rotation).

## Software architecture

The software is built using the Aruduino IDE along with the [Teensyduino][12] addon.

The software is architected around the flow of the excellent [PJRC Teensy audio library][14], and
its associated GUI design tool.


![Flow diagram](./diagrams/architecture.png)

A key feature, and a powerful feature of the audio library, is that inbetween the two data queues we
can, and do, apply other processing to the data flow, such as using the [ARM CMSIS][11] functions or
open coding algorithms (as is done for some of the noise reduction code).

From Version 1.1 the audio path is run with a decimation of 4, bringing the path down to 11kHz sample
rate. Although this saves us processing overhead, the main goal was to 'focus' the noise reduction
algorithms to process just the audio spectrum (0-5.5kHz), rather than trying to process the whole
CD quality spectrum (0-22kHz), if we'd not decimated the native 44kHz sample rate.

### Display

The display has two 'modes'. The default mode is to show the current status/setup, and if a decoder
is active, then to display the output of the decoder.

The top line of the display shows either the name of the current settings slot, or the output of the
active decoder.

The bottom line shows status information. The following table summarises the bottom line:


```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|S|F|N|N|A| |NR   | |C P U|C|W  |
|i|i|B|o|G| | | | | | | | |W|P  |
|g|l|l|t|C| | | | | | | | |T|M  |
|n|t|a|c| | | | | | | | | |u| | |
|a|e|n|h| | | | | | | | | |n| | |
|l|r|k| | | | | | | | | | |e| | |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

the LED flashes in sync with CW tone detection.

### Menu system

The menu system is entered by clicking the rotary shaft encoder, and navigated with rotates and
clicks of the encoder. It is exited by using the `Back` menu items within the menu itself. The
menu is hierachical. Exiting the top level menu takes you back to the status display.

The following top level menus are available:

- Volume
- NR menu
- Decoder menu
- Filter menu
- AGC menu
- Settings menu

### Settings

The unit stores 11 'slots' of settings in the eeprom. The first six of these slots are initialised
with some defaults:

- 0: FM - Wide band FM filter, notch and nb on, spectral NR.
- 1: AM - Wide band AM filter, notch and nb on, spectral NR.
- 2: OFF - Everything off (no processing).
- 3: SG5K - Everything off apart from hardware SG5k AGC.
- 4: SSB - Medium band SSB filter, notch and nb on, spectral NR.
- 5: CW - Narrow band CW filter, notch off, nb on. No NR. [K4ICY][7] cw decoder.

All eeprom slots can be edited, modified and saved. The defaults can be restored if necessary,
on either a slot by slot or full system basis.

One interesting feature of the 'settings' menu, due to the limited number of inputs, is you need
to select the slot you wish to name/reset/save before you perform the action. Thus, first set the
slot number you wish to edit, and then make your edits/saves/resets etc.

## Feature list

The following features are in the current code:

  - A selection of noise reduction algorithms, including:
    - Least Means Square and Leaky Least Means Square
    - Exponential smoothing moving filter
    - Average smoothing moving filter
    - Spectral noise reduction, with a choice of algorithms
  - Noise blanker
  - Auto-notch filter (tone/whistle removal)
  - Configurable band pass filtering, with user memories and presets for:
    - SSB
    - CW
    - FM
    - AM
  - A selection of CW decoders
    - [GI1MIC/WB7FHC][4] 'morseduino' decoder
    - [K4ICY][7] using geometric mean for pulse categorisation
    - [TF3LJ][8] Bayesian based decoder
  - Hardware AGC or software volume tracking (note, this is *not* a full AGC, yet).
  - Ability to bypass all features (to aid comparison)
  - 2x16 LCD display for status, settings and morse decode
  - LED indicator for morse detection confirmation.
  - line and USB audio in
  - line and headphone out
  - 11 user configurable setting memories, 6 pre-loaded with defaults
  - menu driven configuration via rotary click encoder
  - Quick mode/setting selection through rotary encoder
  - Input level indicator, with input clipping warning.

## Building and Development

The project is built using the Arduino IDE with the [Teensyduino addon][12]. To save everybody
having to build the project from source, Pre-built binaries are provided in the
[GitHub Releases][22], which can be burnt into the Teensy 4.0 using the [Teensy Loader][21].

In order to build the project from sources, you will need a few extra libraries and to configure
your Teensy 4.0 board correctly:

- Set your Teensy 4.0 board up in the Arduino IDE as:
  - Teensy 4.0
  - Serial+MIDI+Audio setup
  - 600MHz
- Add the [Grove RGB LCD library][15] to your Arduino installation
- Add the [Arduino Menu library][16] to your Arduino installation
  - *Note:* You will need a version that includes [my addition for RGB LCD support][17], which
    means you may have to install this one from source. of you use the same LCD that is. I believe
    you need a version that is > v4.21.4 ?
- Add the [Click Encoder library][18]. You may need to install this one from source.

You also need to have *all* the sources, not just the `.ino` file, in a directory named `DSPham`. Note,
if you downloaded a `zip` or `tar.gz` file from GitHub, then it will most likely have placed the
source files in a subdirectory called `DSPham-master` or `DSPham-x.y` - I believe you **must**
rename that directory to be just `DSPham` before you can build it with the Arduino IDE.

That should give you the correct setup and libraries to build and program the project. The most common
mistake I make is to not set up the correct 'USB Type' for the Teensy audio shield support, which tends
to end in errors such as `Audio.h not found` etc.

If you are developing/improving/testing new features, then the ability to feed audio via the USB
port is very useful, allowing you to develop without needing a rig wired up or running, and allowing
you to feed the same audio time and again to make comparisions easier.

## Birdies!

If you do build yourself one of these, then watch out for RF emissions! I initially had a lot of noise
being injected into my system, and had to shield the case, add isolation and ferrites, but *most
importantly* was to add a good set of smoothing and filtering capacitors to the 7805 PSU. Do not skimp
on that, as I learnt.

## Bugs, features, patches etc.

If you do build one of these, and have any questions etc., feel free to ask. Probably the best place
to ask is via a GitHub Issue, as then we get a recorded history to help others in the future.

If you want to contribute code and improvements, feel free! It might be an idea to ask me first though
what I think, just in case that is a feature I've already tried, or am working on, or had already
dismissed. No point duplicating work.

All submissions should come via Github pull requests. Requests, bugs etc. should be reported via
Github issues.

## Version History

This section details the version 'releases', and what changed. Releases can be found on
the [Release page][22].

| Version | Date       | Changes |
| ------- | ---------- | ------- |
| v1.0    | 2021-02-10 | Initial release |
| v1.1    | 2021-02-16 | Normalise FIRs. Decimate audio x4 |
| v1.2    | 2021-03-16 | Fix FIR normalise bug. Add input clip indicator |

## Samples

So you can get some idea of the performance of the unit, I've recorded and uploaded a
couple of samples. The samples were recoded off my Xiegu G90 transciever, from an ~43ft
long wire strung out in a fairly noisy urban environment:

1) The [20m whistle sample](./samples/20m_whistle.mp3) is an SSB conversation, with a constant
tone whistle just inside its upper passband. The sample starts out for 10s in 'setting 2 - OFF',
then moves to 'setting 3 - SG5K' for 10s, which is with the hardware AGC in the DSPham enabled.
Then it moves to 'setting 4 - SSB', which has an SSB bandpass, notch and glitch filters, SG5K AGC
and the Spectral noise reduction all enabled.

2) The [VOLMET sample](./samples/volmet_5.mp3) is a recording of a
[VOLMET weather broadcast](https://en.wikipedia.org/wiki/VOLMET) on 5.450MHz. It conviniently
has some RTTY type interference in the background.
The sample starts out for 10s in 'setting 2 - OFF',
then moves to 'setting 3 - SG5K' for 10s, which is with the hardware AGC in the DSPham enabled.
Then it moves to 'setting 4 - SSB', which has an SSB bandpass, notch and glitch filters, SG5K AGC
and the Spectral noise reduction all enabled.

[1]: https://github.com/DD4WH/Teensy-ConvolutionSDR "Teensy-ConvolutionSDR"
[2]: https://github.com/df8oe/UHSDR "UHSDR"
[3]: https://github.com/NR0V/wdsp "wdsp library"
[4]: https://gi1mic.github.io/ "$19 DSP Filter"
[5]: https://www.pjrc.com/ "PJRC"
[6]: https://github.com/PaulStoffregen/Audio "Paul Stoffregen"
[7]: http://k4icy.com/cw_decoder.html "K4ICY Morce Decoder"
[8]: https://sites.google.com/site/lofturj/cwreceive "TF3LJ bayesian CW"
[9]: https://www.pjrc.com/store/teensy40.html "Teensy 4.0"
[10]: https://www.pjrc.com/store/teensy3_audio.html "Teensy audio adaptor"
[11]: https://developer.arm.com/tools-and-software/embedded/cmsis "ARM CMSIS"
[12]: https://www.pjrc.com/teensy/teensyduino.html "Teensyduino"
[13]: https://wiki.seeedstudio.com/Grove-LCD_RGB_Backlight/ "Grove RGB LCD"
[14]: https://www.pjrc.com/teensy/td_libs_Audio.html "Teensy audio library"
[15]: https://github.com/Seeed-Studio/Grove_LCD_RGB_Backlight "RGB LCD library"
[16]: https://github.com/neu-rah/ArduinoMenu "Arduino Menu"
[17]: https://github.com/neu-rah/ArduinoMenu/pull/331 "RGB LCD menu support PR"
[18]: https://github.com/soligen2010/encoder "ClickEncoder"
[20]: https://www.pjrc.com/store/teensy40.html#pins "Teensy 4.0 pins"
[21]: https://www.pjrc.com/teensy/loader.html "Teensy loader"
[22]: https://github.com/grahamwhaley/DSPham/releases "github releases folder"
[23]: https://www.handsontec.com/dataspecs/module/Rotary%20Encoder.pdf "KY-040 rotary encoder"
