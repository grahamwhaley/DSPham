// Taken from http://k4icy.com/cw_decoder.html
//
// original 'ino' file modified to trim out generics and just keep the core decoder
// we can then call the key up/down routines from our tone detector code.
//

/*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 * 
 *    K4ICY CW Decoder
 * 
 *    For use with logic-signal input from key or tone decoder
 * 
 *    by (c) Michael A. Maynard, a.k.a. "K4ICY"     Visit: http://www.k4icy.com/
 *    
 *    Code is Open Source and free to alter and distribute. If copying or using derivitive
 *    of this code, please give proper credit to the original author/programmer.
 *      
 *    Version 1.14r  06/23/20                   Questions: mikek4icy@gmail.com
 *    
 *//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 *    The Morse Code:
 * 
 *    According to the generally agreed upon timing convention, the word 'PARIS', when sent in Morse code (also known as
 *    "CW,") with proper timing equates to 50 units of time, when the total of the duration of all code elements is divided
 *    into 1 minute, this constitutes the Word Per Minute speed.
 *     _ ___ ___ _   _ ___   _ ___ _   _ _   _ _ _        
 *    |_ ___ ___ _   _ ___   _ ___ _   _ _   _ _ _       |
 *    
 *    The millisecond duration of an element unit can be calculated as 60,000 / (WPM * 50) or 1,200 / WPM.
 *    Dots, or 'Dits', which are the shorter pulse bursts, are considered 1 unit while Dashes or 'Dahs', the longer of
 *    the pulse bursts, are considered to be 3 units.  These elements are called 'marks'.  The 'space' or signal-off time
 *    between each intra-character element are also 1 unit in length.  3 units of space are to follow each character and
 *    seven are to follow each word.
 *    
 *    The actual timing performed by hand without electronic assistance will vary as much as the skill and personality
 *    of the operator themselves.  At the least discernible, a 'weighting' ratio, or the relationship between dots and dashes
 *    of 3:2 is passable but harder to copy.  3:1 is preferred but most operators prefer to send and copy a lighter weight,
 *    especially at higher speeds.  The typical range for Morse code hand-sending (via CW) is 10 to 25 WPM and 'radiosports'
 *    contesters prefer 30 to 40 sent electronically.  60 WPM is a great maximum for most homebrew projects where the smallest
 *    'dit' would have a mark duration only 20 ms.  A 'dah' at a slow 5 WPM would be 720 ms.
 *    This will be our typical working range which is fine using the millis() function of the Arduino.
 *    
 *    A Secret Decoder Ring Requires at Least Two Parts:
 *    
 *    A CW decoder requires two parts including both a way to 'hear', or discriminate the signal coming from your receiver's
 *    audio as well as an algorithm or process to decode that signal.  The tone decoder should be able to operate from the
 *    audio port jack of your radio, should be adjustable for gain and frequency response and should be able to narrow in
 *    on your signal apart from others that may be competing.  You'll find the schematic for the tone decoder using the
 *    classic LM567 IC at k4icy.com/cw_decoder/cw_decoder.html  The chosen audio tone will be converted to a matching
 *    logic signal which is then used by the second part, an Arduino Uno, Nano (or other model,) to handle the code
 *    deciphering and display of the result to an LCD panel.
 *    
 *    Timing of a CW Morse code signal, especially that of human-sent code does not rigidly follow the 'PARIS' 3:1 timing
 *    convention which makes it harder to deciphyer with a rigid algorithm.  While many options for software and
 *    microcontroller-based CW decoding do exist, many performing better than most humans, for our purposes as hams
 *    and Arduino enthusiasts, are not really granted the room or processing speed on a basic Arduino (at 32k of flash)
 *    to include the option for higher-level analysis such as using Bayesian classification and histogram modeling.
 *    My approach isn't the most accurate but it's simple enough and should do well with hand-sent code, quickly adjusting
 *    to any speed up to around 70 WPM.
 *        
 *    The decoding algorithm works as follows:
 *    
 *    The duration of each Morse code element sent, whether long or short, relatively speaking, is sampled.  The two are,
 *    of course, not able to be sent at the same time so the best we can do is poll the duration of each pulse in consecutive
 *    order, and if close enough to each other in occurrence will most likely have been sent at the same WPM speed.
 *    When a long pulse follows a short one or vice-versa, it can be assumed that the long pulse is generally more than
 *    2-times the duration of the short.  Since Dashes are generally 3 times the duration of a dot, a difference should be evident.
 *    
 *    When this condition is detected the algorithm can move a reference point in between the the two and use
 *    it to discriminate all future occurrences of any sampled duration, classifying a very likely candidate for either a
 *    dot or a dash for which to decode via a lookup table.
 *    
 *    Two thresholds are established and maintained via a moving average including a geometric mean and an arithmetic mean.
 *    The former is more appropriate for finding the most likely mid-point in between a sampled dot and dash as, in practice,
 *    any variation in duration, especially that being sent by hand, will be in likely propotion to the element's usual duration. 
 *    The latter mean is simply a mid-way average and is better suited for determining the space.
 *    
 *    Programming Considerations:
 *    
 *    The decoding algorithm of this sketch is most likely an attempt to "reinvent the wheel" as one or two other Arduino
 *    sketches I've looked into use the geometric mean, but generally, most only sample the dashes to determin the thresholds.
 *    I assume that working more accurately with both known duration values may tend to yield better results. It's now all down
 *    to the limitations of Arduino process timing, the nuances of printing to an LCD screen, desired features and crafting
 *    a usable electronic input signal the sketch which all add to the complexity.  This decoder has to do well with hand-sent
 *    code as well as keeping up with CW 'speed demons'.  There can certainly be later improvements made to this sketch.
 *    
 *    License:
 *    
 *    The K4ICY CW Decoder sketch is Open Source and free to distribute and implement without permission.    
 *    Full license is also given to alter and generally improve upon this code/sketch - BUT - Michael A. Maynard, a.k.a. "K4ICY",
 *    the author, insists that credit and acknowledgement be given to the same, including the web site reference.
 *    Enjoy!
 *    
 *    Notes:
 *    
 *    1 - Sorry, there is no easy way to tell this sketch arbitrary character dimentions for any LCD panel, the builder will have to go through and
 *        make adjustments to the code.  The current setup is for a 20 x 4 LCD using I2C and I don't believe the decoder timing will handle higher
 *        dimensions but yoy're welcome to experiment.
 *    2 - used this text for testing input using LCWO's Convert Text to CW feature: ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-=,.?"':;!@$
 *    
 *//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>

#include "global.h"
#include "morseGen.h"
#include "k4icy.h"

void morseDecode();

/// SYSTEM VARIABLES ////////////////////
bool toneOutput = true;         //  Allows for control of a sidetone
int debounceFactor = 14;        //  Debouncing of input signal via minimal signal time duration in ms.  Default should be 15 ms and no less than 8
bool keyLine = false;           // This flag denotes acknowledgement of a active input status on the Morse Key Input Pin

/// TIMING VARIABLES ////////////////////
unsigned long timeTrack = millis(); // Will be used as a time base for many events

long thresholdGeometricMean = 139;  //  This with be the Geometric Mean between short and long pulse pairs  sqrt (s * l)
long thresholdArithmeticMean = 160; //  This will be the Arithmetic Mean between short and long pulse pairs  (s + l) / 2

long keyLineNewEvent = 0;       // Record newest key-down timing
long keyLinePriorEvent = 0;     // Prior key-down timing for comparison

long shortEventHistogramList[10];
long longEventHistogramList[10];
long spaceEventHistogramList[10];
int eventHistogramTrack = 0;
int wpmHistogramTrack = 0;

unsigned long keyLineDuration = 0;  // Key-Down duration
unsigned long spaceDuration = 0;  // Key-Up duration
unsigned long oldSpaceDuration = 0; // Historical reference of Key-Up duration
unsigned long spaceDurationReference = 0; // Key-Up start moment

float wordSpaceTiming = 3.0;    // * threshold... usually 7 units typical
unsigned long wordSpaceDuration = 0;  // Key-Up duration
unsigned long wordSpaceDurationReference = 0; // Key-Up start moment

/// DECODE VARIABLES ////////////////////
float compareFactor = 2.0;      //  This factor determines the ratio threshold for comparing 'dahs' to 'dits' - assuming arithmetic mean of 3:1
bool characterStep = false;     // Lock-step one character at a time for decoding
char elementSequence[10];       //  This string will hold the 'dahs' and 'dits' to decode
int elementSequenceTrack = 0;   //  Index into the string
byte decodeChar = 0;            //  Decoded character from code element sequence lookup table
String decodeProSign = "";      //  Decoded/detected multiple-letter procedural signs can be passed via this variable
int decodeProSignLength;        //  This is useful when printing any prosigns
bool wordStep = false;          // Add one blank space to display after word space duration has been met

int oldWPM = 0;
int WPM = 0;                    //  Words Per Minute to display
int WPMHistogramList[20];
int WPMHistogramTrack = 0;

/// System Setup /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void k4icy_setup()
{
  // preset moving averages  (15 wpm default)
  for (int i = 0; i < 10; i++) {
    shortEventHistogramList[i] = 80;
    longEventHistogramList[i] = 240;
    spaceEventHistogramList[i] = 80;
  }
  for (int i = 0; i < 20; i++) {
    WPMHistogramList[i] = 15;
  }
}

////////////////////////////////// Operation //////////////////////////////////////////////////////////////////////////////////////

/// When the Key is Down - or There is a Signal ////////////////////////////////////////
void k4icy_keyDown()
{
  timeTrack = millis();

  if (keyLine == false) {       // do the following only once per key-down event
    keyLineDuration = timeTrack;  // update duration
    wordSpaceDurationReference = timeTrack; // create starting point to measure for word space (typically 7 dits long)
    keyLine = true;
  }
}                               //end of key down

/// When the Key is Up - or There is Not a Signal ///////////////////////////////////
void k4icy_keyUp()
{
  timeTrack = millis();

  ///  Allow for Time to Debounce the Signal //////////////////////////////////////
  if (timeTrack >= (keyLineDuration + debounceFactor) && keyLine) {
    keyLine = false;            // only allow for this section once per detected key-up
    keyLinePriorEvent = keyLineNewEvent;  //  Create history of last signal duration event to compare to new one
    keyLineNewEvent = timeTrack - keyLineDuration;  // Get current event duration

    ///  IF the Current Duration Event Compared to the Previous Event appears to be a Dot / Dash pair [ roughly (>2):1 ] /////////////////////////
    if ((keyLineNewEvent >= keyLinePriorEvent * compareFactor
         && oldSpaceDuration <= keyLinePriorEvent * compareFactor)
        || (keyLinePriorEvent >= keyLineNewEvent * compareFactor
            && oldSpaceDuration <= keyLineNewEvent * compareFactor)) {

      /// Find out which one is the Dot and which is the Dash and roll them into a moving average of each /////////////////////////////////////
      if (keyLineNewEvent >= keyLinePriorEvent) {
        longEventHistogramList[eventHistogramTrack] = keyLineNewEvent;
        shortEventHistogramList[eventHistogramTrack] = keyLinePriorEvent;
      } else {
        longEventHistogramList[eventHistogramTrack] = keyLinePriorEvent;
        shortEventHistogramList[eventHistogramTrack] = keyLineNewEvent;
      }

      /// Keep a moving average detected dot/dash pairs ////////////////////////
      long longEventAverage =
          longEventHistogramList[0] +
          longEventHistogramList[1] +
          longEventHistogramList[2] +
          longEventHistogramList[3] + longEventHistogramList[4];
      longEventAverage +=
          longEventHistogramList[5] +
          longEventHistogramList[6] +
          longEventHistogramList[7] +
          longEventHistogramList[8] + longEventHistogramList[9];
      longEventAverage /= 10;

      long shortEventAverage =
          shortEventHistogramList[0] +
          shortEventHistogramList[1] +
          shortEventHistogramList[2] +
          shortEventHistogramList[3] + shortEventHistogramList[4];
      shortEventAverage +=
          shortEventHistogramList[5] +
          shortEventHistogramList[6] +
          shortEventHistogramList[7] +
          shortEventHistogramList[8] + shortEventHistogramList[9];
      shortEventAverage /= 10;

      /// Find threshold means /////////////////////////
      thresholdGeometricMean = sqrt(shortEventAverage * longEventAverage);
      // thresholdArithmeticMean = (shortEventAverage + longEventAverage) / 2 ;

      /// Keep a moving average of the intra-element space duration
      spaceEventHistogramList[eventHistogramTrack] = oldSpaceDuration;

      long spaceEventAverage =
          spaceEventHistogramList[0] +
          spaceEventHistogramList[1] +
          spaceEventHistogramList[2] +
          spaceEventHistogramList[3] + spaceEventHistogramList[4];
      spaceEventAverage +=
          spaceEventHistogramList[5] +
          spaceEventHistogramList[6] +
          spaceEventHistogramList[7] +
          spaceEventHistogramList[8] + spaceEventHistogramList[9];
      spaceEventAverage /= 10;

      /// Bootstrap threshold values - - - If any are below or above known Dot/Dash pair ranges then move them instantly
      if (thresholdGeometricMean < shortEventHistogramList[eventHistogramTrack]
          || thresholdGeometricMean >
          longEventHistogramList[eventHistogramTrack]) {

        thresholdGeometricMean =
            sqrt(shortEventHistogramList
                 [eventHistogramTrack] *
                 longEventHistogramList[eventHistogramTrack]);

        longEventHistogramList[0] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[1] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[2] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[3] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[4] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[5] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[6] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[7] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[8] = longEventHistogramList[eventHistogramTrack];
        longEventHistogramList[9] = longEventHistogramList[eventHistogramTrack];

        shortEventHistogramList[0] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[1] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[2] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[3] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[4] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[5] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[6] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[7] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[8] =
            shortEventHistogramList[eventHistogramTrack];
        shortEventHistogramList[9] =
            shortEventHistogramList[eventHistogramTrack];

        longEventAverage = longEventHistogramList[eventHistogramTrack];
        shortEventAverage = shortEventHistogramList[eventHistogramTrack];

        WPMHistogramList[wpmHistogramTrack] =
            (6000 /
             (longEventHistogramList
              [eventHistogramTrack] +
              shortEventHistogramList[eventHistogramTrack] + oldSpaceDuration));
        WPMHistogramList[0] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[1] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[2] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[3] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[4] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[5] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[6] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[7] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[8] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[9] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[10] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[11] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[12] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[13] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[14] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[15] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[16] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[17] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[18] = WPMHistogramList[wpmHistogramTrack];
        WPMHistogramList[19] = WPMHistogramList[wpmHistogramTrack];

      }
      /// we will now calculate the WPM... ////////////////////////////////////////////
      oldWPM = WPM;             // used to see when WPM has changed to save on LCD upload/refresh time

      //          WPM = ( 6000 / ( longEventAverage + shortEventAverage + spaceEventAverage ) ) * 1.06 ;  // use this instead for faster WPM result
      ///*         
      WPMHistogramList[wpmHistogramTrack] = (6000 / (longEventAverage + shortEventAverage + spaceEventAverage));  // use this instead if WPM result is too jittery
      //
      //
      WPM = 20 + WPMHistogramList[0] + WPMHistogramList[1] + WPMHistogramList[2] + WPMHistogramList[3] + WPMHistogramList[4]; // 
      WPM += WPMHistogramList[5] + WPMHistogramList[6] + WPMHistogramList[7] + WPMHistogramList[8] + WPMHistogramList[9]; //
      WPM += WPMHistogramList[10] + WPMHistogramList[11] + WPMHistogramList[12] + WPMHistogramList[13] + WPMHistogramList[14];  //
      WPM += WPMHistogramList[15] + WPMHistogramList[16] + WPMHistogramList[17] + WPMHistogramList[18] + WPMHistogramList[19];  //
      WPM /= 19.05;
      //WPM /= 20 ;
      //WPM *= 1.05 ;
      //*/          

      /// set index to build up further moving averages ////////////////
      eventHistogramTrack++;
      if (eventHistogramTrack > 9) {
        eventHistogramTrack = 0;
      }                         // reset index if needed

      wpmHistogramTrack++;
      if (wpmHistogramTrack > 19) {
        wpmHistogramTrack = 0;
      }                         // reset index if needed
    }
    /// Reset space durations - in Key-Up state ///////////
    spaceDurationReference = timeTrack;
    wordSpaceDurationReference = timeTrack;

    /// Classify and add most likely Dots or Dashes to a string for eventual character decoding
    if (elementSequenceTrack + 2 <= sizeof(elementSequence)) {
      if (keyLineNewEvent <= thresholdGeometricMean) {
        elementSequence[elementSequenceTrack] = '.';
        characterStep = true;
        wordStep = true;
      } else {
        elementSequence[elementSequenceTrack] = '-';
        characterStep = true;
        wordStep = true;
      }
      elementSequenceTrack++;
      elementSequence[elementSequenceTrack] = '\0';
    } else {
      //We've run out of sequence buffer - nominally we should thus mark this as a bad character,
      //drop it, and move on... but, for now, just stop filling the buffer and let it
      // 'naturally die'
    }
  }
  /// Keep track of Key-Up space timing
  oldSpaceDuration = spaceDuration;
  spaceDuration = timeTrack - spaceDurationReference;

  /// DECODE collected string of elements  /////////////////////////////////////////

  // check to see if inter-element space duration threshold has been exceeded - then decode
  // it is assumed that the intra-space is longer than a Dot but shorter than a Dash
  //if (spaceDuration >= thresholdArithmeticMean) {    // using the Arithmetic Mean seems more stable
  if (spaceDuration >= thresholdGeometricMean) {  // but using the Geometric Mean seems more accurate
    spaceDurationReference = timeTrack; // Key-Up space reset

    if (characterStep) {
      morseDecode();            // decode element sequence string

      if (decodeChar != 7) {
        morsePrint(decodeChar);
      } else {
        decodeProSignLength = decodeProSign.length(); // get the length of the prosign if used

        if (decodeProSignLength > 0) {
          for (int i = 0; i < decodeProSignLength; i++) { // scan through the prosign string...
            morsePrint(decodeProSign.charAt(i));
          }

          morsePrint(163);
        } else {
          morsePrint(decodeChar);
        }
      }

      decodeChar = 0;
      elementSequence[0] = '\0';  // clear memory of code elements
      elementSequenceTrack = 0;
    }
    characterStep = false;
  }
  /// Keep track of Key-Up timing and see if a word space is required
  wordSpaceDuration = timeTrack - wordSpaceDurationReference;

  if (wordSpaceDuration >= thresholdGeometricMean * wordSpaceTiming) {
    wordSpaceDurationReference = timeTrack; // word space reset

    if (wordStep) {
      morsePrint(' ');
    }

    wordStep = false;
  }
}                               // End Main Loop /////

/// Secret Decoder Ring ///////////////////////////////////
void morseDecode()
{                               //  Here, we check the collected elements against our list to decode for the matching character
  decodeChar = '?';             //Unkown char. (changed from 0x7 special char in original code, as we already use that)
  decodeProSign = "";

  //Serial.println(elementSequence);
  if (strcmp(elementSequence, ".-") == 0) {
    decodeChar = 65;
  }                             // A    Letters
  if (strcmp(elementSequence, "-...") == 0) {
    decodeChar = 66;
  }                             // B
  if (strcmp(elementSequence, "-.-.") == 0) {
    decodeChar = 67;
  }                             // C
  if (strcmp(elementSequence, "-..") == 0) {
    decodeChar = 68;
  }                             // D
  if (strcmp(elementSequence, ".") == 0) {
    decodeChar = 69;
  }                             // E
  if (strcmp(elementSequence, "..-.") == 0) {
    decodeChar = 70;
  }                             // F
  if (strcmp(elementSequence, "--.") == 0) {
    decodeChar = 71;
  }                             // G
  if (strcmp(elementSequence, "....") == 0) {
    decodeChar = 72;
  }                             // H
  if (strcmp(elementSequence, "..") == 0) {
    decodeChar = 73;
  }                             // I
  if (strcmp(elementSequence, ".---") == 0) {
    decodeChar = 74;
  }                             // J
  if (strcmp(elementSequence, "-.-") == 0) {
    decodeChar = 75;
  }                             // K
  if (strcmp(elementSequence, ".-..") == 0) {
    decodeChar = 76;
  }                             // L
  if (strcmp(elementSequence, "--") == 0) {
    decodeChar = 77;
  }                             // M
  if (strcmp(elementSequence, "-.") == 0) {
    decodeChar = 78;
  }                             // N
  if (strcmp(elementSequence, "---") == 0) {
    decodeChar = 79;
  }                             // O
  if (strcmp(elementSequence, ".--.") == 0) {
    decodeChar = 80;
  }                             // P
  if (strcmp(elementSequence, "--.-") == 0) {
    decodeChar = 81;
  }                             // Q
  if (strcmp(elementSequence, ".-.") == 0) {
    decodeChar = 82;
  }                             // R
  if (strcmp(elementSequence, "...") == 0) {
    decodeChar = 83;
  }                             // S
  if (strcmp(elementSequence, "-") == 0) {
    decodeChar = 84;
  }                             // T
  if (strcmp(elementSequence, "..-") == 0) {
    decodeChar = 85;
  }                             // U
  if (strcmp(elementSequence, "...-") == 0) {
    decodeChar = 86;
  }                             // V
  if (strcmp(elementSequence, ".--") == 0) {
    decodeChar = 87;
  }                             // W
  if (strcmp(elementSequence, "-..-") == 0) {
    decodeChar = 88;
  }                             // X
  if (strcmp(elementSequence, "-.--") == 0) {
    decodeChar = 89;
  }                             // Y
  if (strcmp(elementSequence, "--..") == 0) {
    decodeChar = 90;
  }                             // Z

  if (strcmp(elementSequence, ".----") == 0) {
    decodeChar = 49;
  }                             // 1    Numbers
  if (strcmp(elementSequence, "..---") == 0) {
    decodeChar = 50;
  }                             // 2
  if (strcmp(elementSequence, "...--") == 0) {
    decodeChar = 51;
  }                             // 3
  if (strcmp(elementSequence, "....-") == 0) {
    decodeChar = 52;
  }                             // 4
  if (strcmp(elementSequence, ".....") == 0) {
    decodeChar = 53;
  }                             // 5
  if (strcmp(elementSequence, "-....") == 0) {
    decodeChar = 54;
  }                             // 6
  if (strcmp(elementSequence, "--...") == 0) {
    decodeChar = 55;
  }                             // 7
  if (strcmp(elementSequence, "---..") == 0) {
    decodeChar = 56;
  }                             // 8
  if (strcmp(elementSequence, "----.") == 0) {
    decodeChar = 57;
  }                             // 9
  if (strcmp(elementSequence, "-----") == 0) {
    decodeChar = 48;
  }                             // 0

  if (strcmp(elementSequence, "-.-.--") == 0) {
    decodeChar = 33;
  }                             // !    Punctuation
  if (strcmp(elementSequence, "..--.") == 0) {
    decodeChar = 33;
  }                             // !
  if (strcmp(elementSequence, ".-..-.") == 0) {
    decodeChar = 34;
  }                             // "
  if (strcmp(elementSequence, "...-..-") == 0) {
    decodeChar = 36;
  }                             // $
  //if (strcmp(elementSequence,".-...") == 0)   { decodeChar = 38; }  // &
  if (strcmp(elementSequence, ".----.") == 0) {
    decodeChar = 39;
  }                             // '
  //if (strcmp(elementSequence,"-.--.") == 0)   { decodeChar = 40; }  // (
  //if (strcmp(elementSequence,"-.--.-") == 0)  { decodeChar = 41; }  // )
  //if (strcmp(elementSequence,".-.-.") == 0)   { decodeChar = 43; }  // +
  if (strcmp(elementSequence, "--..--") == 0) {
    decodeChar = 44;
  }                             // ,
  if (strcmp(elementSequence, "-....-") == 0) {
    decodeChar = 45;
  }                             // -
  if (strcmp(elementSequence, ".-.-.-") == 0) {
    decodeChar = 46;
  }                             // .
  if (strcmp(elementSequence, "-..-.") == 0) {
    decodeChar = 47;
  }                             // /
  if (strcmp(elementSequence, "---...") == 0) {
    decodeChar = 58;
  }                             // :
  if (strcmp(elementSequence, "-.-.-.") == 0) {
    decodeChar = 59;
  }                             // ;
  //if (strcmp(elementSequence,"-...-") == 0)   { decodeChar = 61; }  // = (Pause, BT)
  if (strcmp(elementSequence, "..--..") == 0) {
    decodeChar = 63;
  }                             // ?
  if (strcmp(elementSequence, ".--.-.") == 0) {
    decodeChar = 64;
  }                             // @
  if (strcmp(elementSequence, "..--.-") == 0) {
    decodeChar = 95;
  }                             // _

  //Graham - sorry, no space for the special chars on our
  //display - we use them for other things already.
  //Just have to replace them with their nearest non-special
  //equivalent...
  if (strcmp(elementSequence, ".-.-") == 0) {
    decodeChar = 'A';
  }                             // Ä    Special
  if (strcmp(elementSequence, ".--.-") == 0) {
    decodeChar = 'A';
  }                             // Á
  //if (strcmp(elementSequence,".-.-") == 0)  { decodeChar = 2; }  // Æ
  if (strcmp(elementSequence, "-.-..") == 0) {
    decodeChar = 'C';
  }                             // Ç
  if (strcmp(elementSequence, "..-..") == 0) {
    decodeChar = 'E';
  }                             // É
  if (strcmp(elementSequence, "---.") == 0) {
    decodeChar = 'O';
  }                             // Ö
  if (strcmp(elementSequence, "..--") == 0) {
    decodeChar = 'U';
  }                             // Ü
  if (strcmp(elementSequence, "--.--") == 0) {
    decodeChar = 'N';
  }                             // Ñ

  //if (strcmp(elementSequence,".-.-") == 0)    { decodeProSign = "AA"; }   // Prosigns   (New Line)
  if (strcmp(elementSequence, ".-.-.") == 0) {
    decodeProSign = "AR";
  }                             //            (End of Message)
  if (strcmp(elementSequence, ".-...") == 0) {
    decodeProSign = "AS";
  }                             //            (Wait)
  if (strcmp(elementSequence, "-...-.-") == 0) {
    decodeProSign = "BK";
  }                             //            (Break)
  if (strcmp(elementSequence, "-...-") == 0) {
    decodeProSign = "BT";
  }                             //            (Pause, New Paragraph)
  if (strcmp(elementSequence, "-.-..-..") == 0) {
    decodeProSign = "CL";
  }                             //            (Clear, Going Off Air)
  if (strcmp(elementSequence, "-.-.-") == 0) {
    decodeProSign = "CT";
  }                             //            (Start Copying)
  if (strcmp(elementSequence, "-.--.") == 0) {
    decodeProSign = "KN";
  }                             //            (Invite Specific Station Only)
  if (strcmp(elementSequence, "...-.-") == 0) {
    decodeProSign = "SK";
  }                             //            (End of Transmission)
  if (strcmp(elementSequence, "...-.") == 0) {
    decodeProSign = "SN";
  }                             //            (Understood)
  if (strcmp(elementSequence, "...---...") == 0) {
    decodeProSign = "SOS";
  }                             //            (SOS)
  if (strcmp(elementSequence, "-.-.--.-") == 0) {
    decodeProSign = "CQ";
  }                             //          (Invitation)
  if (strcmp(elementSequence, "........") == 0) {
    decodeProSign = "err";
  }                             //          (Error)

  if (strcmp(elementSequence, "-..-..-") == 0) {
    decodeProSign = "Speaker";
    toneOutput = !toneOutput;
  }                             //  Control to toggle the speaker
}

int k4icy_getWPM()
{
  return WPM;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
