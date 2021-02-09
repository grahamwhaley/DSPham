//*********************************************************************************
//**
//** Project.........: Read Hand Sent Morse Code (tolerant of considerable jitter)
//**
//** Copyright (c) 2016  Loftur E. Jonasson  (tf3lj [at] arrl [dot] net)
//**
//** This program is free software: you can redistribute it and/or modify
//** it under the terms of the GNU General Public License as published by
//** the Free Software Foundation, either version 3 of the License, or
//** (at your option) any later version.
//**
//** This program is distributed in the hope that it will be useful,
//** but WITHOUT ANY WARRANTY; without even the implied warranty of
//** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//** GNU General Public License for more details.
//**
//** The GNU General Public License is available at
//** http://www.gnu.org/licenses/
//**
//** Substantive portions of the methodology used here to decode Morse Code are found in:
//**
//** "MACHINE RECOGNITION OF HAND-SENT MORSE CODE USING THE PDP-12 COMPUTER"
//** by Joel Arthur Guenther, Air Force Institute of Technology,
//** Wright-Patterson Air Force Base, Ohio
//** December 1973
//** http://www.dtic.mil/dtic/tr/fulltext/u2/786492.pdf
//**
//** Platform........: Teensy 3.1 / 3.2 and the Teensy Audio Shield
//**
//** Initial version.: 0.00, 2016-01-25  Loftur Jonasson, TF3LJ / VE2LJX
//**
//*********************************************************************************

#include <Arduino.h>

#include "global.h"
#include "morseGen.h"

#include "tf3lj.h"

#include "tf3lj_dec.h"

bflags                b;                            // Various Operational state flags
char                  code[DATA_BUFSIZE];           // Decoded dot/dash info in symbol form, e.g. ".-.."                                         
sigbuf                data[DATA_BUFSIZE];           // Buffer containing decoded dot/dash and time information
double                pulse_avg;                    // CW timing variables - pulse_avg is a composite value
double                dot_avg, dash_avg;            // Dot and Dash Space averages             
double                symspace_avg, cwspace_avg;    // Intra symbol Space and Character-Word Space
int32_t               w_space;                      // Last word space time
int32_t               last_outcount= 0;             // sig_outcount for previous character, used for Error Correction func
int32_t               cur_outcount = 0;             // Basically same as sig_outcount, for Error Correction functionality

void InitializationFunc(void);
bool DataRecognitionFunc(void);
void CodeGenFunc(void);
uint8_t CharacterIdFunc(void);
void PrintCharFunc(uint8_t c);
void WordSpaceFunc(uint8_t c);
bool ErrorCorrectionFunc(void);

void tf3lj_dec_init() {
  b.initialized = FALSE;
}

//------------------------------------------------------------------
//
// CW Decode manages all the decode Functions.
// It establishes dot/dash/space periods through the Initialization
// function, and when initialized (or if excessive time when not fully
// initialized), then it runs DataRecognition, CodeGen and CharacterId
// functions to decode any incoming data.  If not successful decode
// then ErrorCorrection is attempted, and if that fails, then
// Initialization is re-performed.
//
//------------------------------------------------------------------
void CW_Decode(void)
{
  uint8_t decoded;                                  // 0xff if char not recognized
  bool    received;                                 // True on a symbol received

  //-----------------------------------
  // Initialize pulse_avg, dot_avg, dash_avg, symspace_avg, cwspace_avg
  if (b.initialized == FALSE) InitializationFunc();

  //-----------------------------------
  // Process the works once initialized - or if timeout
  if ((b.initialized == TRUE) || (cur_time >= 344*TIMEOUT)) // 344 equals one second
  {
    received = DataRecognitionFunc();               // True if new character received
    if (received && (data_len > 0))                 // also make sure it is not a spike
    {
      CodeGenFunc();                                // Generate a dot/dash pattern string
      decoded = CharacterIdFunc();                  // Indentify the Character
      if (decoded < 0xfe)                           // 0xfe = spike suppression, 0xff = error
      {
        PrintCharFunc(decoded);                     // Print to LCD and Serial (USB)
        WordSpaceFunc(decoded);                     // Print Word Space to LCD and Serial when required
      }
      else if (decoded == 0xff)                     // Attempt Error Correction
      {
        // Debug
        //Serial.println();
        //Serial.println(code);
        // If Error Correction function cannot resolve, then reinitialize speed
        if(!ErrorCorrectionFunc()) b.initialized = FALSE;
      }
    }
  }
}


//------------------------------------------------------------------
//
// Initialization Function (non-blocking-style)
// Determine Pulse, Dash, Dot and initial
// Character-Word time averages
//
// Input is the circular buffer sig[], including in and out counters
// Output is variables containing dot dash and space averages
//
//------------------------------------------------------------------
void InitializationFunc(void)
{
  static int16_t startpos, progress;                             // Progress counter, size = SIG_BUFSIZE
  static bool    initializing;                                   // Bool for first time init of progress counter
  int16_t        processed;                                      // Number of states that have been processed
  double         t;                                              // We do timing calculations in floating point
                                                                 // to gain a little bit of precision when low
                                                                 // sampling rate
  // Set up progress counter at beginning of initialize
  if (initializing == FALSE)
  {
    startpos = sig_outcount;                                     // We start at last processed mark/space
    progress = sig_outcount;
    initializing = TRUE;
    pulse_avg    = 0;                                            // Reset CW timing variables to 0
    dot_avg      = 0;
    dash_avg     = 0;
    symspace_avg = 0;
    cwspace_avg  = 0;
    w_space      = 0;
 }

  // Determine number of states waiting to be processed
  if (progress < startpos) processed = SIG_BUFSIZE - startpos + progress;
  else processed = progress - startpos;
    
  if (processed >= 98)
  {
    b.initialized = TRUE;                                        // Indicate we're done and return
    initializing = FALSE;                                        // Allow for correct setup of progress if
                                                                 // InitializaitonFunc is invoked a second time
  }
  if (progress != sig_incount)                                   // Do we have a new state?
  {
    t = sig[progress].time;
    
    if (sig[progress].state)                                     // Is it a pulse?
    {
      if (processed > 32)                                        // More than 32, getting stable
      {
        if (t > pulse_avg)
        {
          dash_avg = dash_avg + (t - dash_avg)/4.0;              // (e.q. 4.5)
        }
        else
        {
          dot_avg = dot_avg + (t - dot_avg)/4.0;                 // (e.q. 4.4)
        }
      }
      else                                                       // Less than 32, still quite unstable
      {
        if (t > pulse_avg)
        {
          dash_avg = (t + dash_avg)/2.0;                         // (e.q. 4.2)
        }
        else
        {
          dot_avg = (t + dot_avg)/2.0;                           // (e.q. 4.1)
        }
      }
      pulse_avg = (dot_avg/4 + dash_avg)/2.0;                    // Update pulse_avg (e.q. 4.3)
    }
    else          // Not a pulse - determine character_word space avg
    {
      if (processed > 32)
      {
        if (t > pulse_avg)                                       // Symbol space?
        {
          cwspace_avg = cwspace_avg + (t - cwspace_avg)/4.0;     // (e.q. 4.8)
        }
        else
        {
          symspace_avg = symspace_avg +  (t - symspace_avg)/4.0; // New EQ, to assist calculating Rate
        }
      }
    }         
    progress++;                                                  // Increment progress counter
    if (progress == SIG_BUFSIZE) progress = 0;
  }
}


//------------------------------------------------------------------
//
// Spike Cancel function
//
// Optionally selectable in CWReceive.h, used by Data Recognition
// function to identify and ignore spikes of short duration.
//
//------------------------------------------------------------------
#if SPIKECANCEL || SHORTCANCEL
double spikeCancel(double t)
{
  static bool spike;

  #if SPIKECANCEL                               // Squash spikes/transients of short duration
  if (t <= SPIKECANCEL)
  #elif SHORTCANCEL                             // Squash spikes shorter than 1/3rd dot duration
  if ((3*t<dot_avg) && (b.initialized==TRUE))   // Only do this if we are not initializing dot/dash periods
  #endif
  {
    spike = TRUE;
    sig_outcount++;                             // If short, then do nothing
    if (sig_outcount == SIG_BUFSIZE) sig_outcount = 0;
    return 0;
  }
  else if (spike == TRUE)                       // Check if last state was a short Spike or Drop
  {
    spike = FALSE;
    // Add time of last three states together.
    t = t + sig[(sig_outcount-1<0)?SIG_BUFSIZE-1:sig_outcount-1].time
          + sig[(sig_outcount-2<0)?SIG_BUFSIZE-(2-sig_outcount):sig_outcount-2].time;
  }
  return t;
}
#endif


//------------------------------------------------------------------
//
// Data Recognition Function (non-blocking-style)
// Decode dots, dashes and spaces and group together
// into a character.
//
// Input is the circular buffer sig[], including in and out counters
// Variables containing dot, dash and space averages are maintained, and
// output is a data[] buffer containing decoded dot/dash information, a
// data_len variable containing length of incoming character data.
// The function returns TRUE when a complete new character has been decoded
// In addition, b.wspace flag indicates whether long (word) space after char
//
//------------------------------------------------------------------
bool DataRecognitionFunc(void)
{
  double        t;                                 // Temporary time
  double        x = 0;                             // Temp comparison value
  bool          new_char = FALSE;                  // Return value
  static bool   processed;
  
  //-----------------------------------
  // Do we have a new state to process?
  if (sig_outcount != sig_incount)
  {
    b.timeout = FALSE;                              // Mainly used by Error Correction Function
    t = sig[sig_outcount].time;                     // Get time of the new state
    
    //-----------------------------------
    // Is it a Mark (keydown)?
    if (sig[sig_outcount].state)
    {
      #if SPIKECANCEL || SHORTCANCEL                // Squash spikes/transients
      double temp = spikeCancel(t);
      if (temp == 0) return FALSE;                  // It was a transient
      else t = temp;                                // If last was a transient, then t = last 3 t added together
      #endif
      
      processed = FALSE;                            // Indicate that incoming character is not processed
      sig_outcount++;                               // Update process counter
      if ((sig_outcount == SIG_BUFSIZE)) sig_outcount = 0;  // Wraparound output index
 
      x = pulse_avg - t;                            // Determine if Dot or Dash (e.q. 4.10)
      
      //-----------------------------------
      // Is it a dot?      
      if (x >= 0)                                   // It is a Dot
      {
        b.dash = FALSE;                             // Clear Dash flag
        data[data_len].state = 0;                   // Store as Dot
        dot_avg = dot_avg + (t - dot_avg)/8.0;      // Update dot_avg (e.q. 4.6)
      }
      //-----------------------------------
      // Is it a Dash?
      else
      {
        b.dash = TRUE;                              // Set Dash flag
        data[data_len].state = 1;                   // Store as Dash
        if (t <= 5*dash_avg)                        // Store time if not stuck key
        {
          dash_avg = dash_avg + (t - dash_avg)/8.0; // Update dash_avg (e.q. 4.7)
        }      
      }
      data[data_len].time =  (uint32_t) t;          // Store associated time
      data_len++;                                   // Increment by one dot/dash
      pulse_avg = (dot_avg/4 + dash_avg)/2.0;       // Update pulse_avg (e.q. 4.3)
    }
    
    //-----------------------------------
    // Is it a Space?
    else
    {
      #if SPIKECANCEL || SHORTCANCEL                // Squash spikes/transients
      double temp = spikeCancel(t);
      if (temp == 0) return FALSE;                  // It was a transient
      else t = temp;                                // If last was a transient, then t = last 3 t added together
      #endif
      
      sig_outcount++;                               // And update process counter
      if ((sig_outcount == SIG_BUFSIZE)) sig_outcount = 0;  // Wraparound output index
      
      // We expect a bit shorter space if the last character was a dash
      //
      else if (b.dash == TRUE)                      // Last character was a dash
      {
        b.dash = false;
        x = t - (pulse_avg - ((uint32_t) data[data_len-1].time - pulse_avg)/4.0);  // (e.q. 4.12, corrected)  
        if (x < 0)                                  // Return on symbol space - not a full char yet
        {
          symspace_avg = symspace_avg + (t - symspace_avg)/8.0; // New EQ, to assist calculating Rat
          return FALSE;
        }
        else if (t <= 10*dash_avg)                  // Current space is not a timeout 
        {
          x = t - (cwspace_avg - ((uint32_t) data[data_len-1].time - pulse_avg)/4.0);// (e.q. 4.14)
          if (x >= 0)                               // It is a Word space
          {
            w_space = t;
            b.wspace = TRUE;
          }
        }
      }
      else                                          // Last character was a dot
      {
        x = t - pulse_avg;                          // (e.q. 4.11)
        if (x < 0)                                  // Return on symbol space - not a full char yet
        {
          symspace_avg = symspace_avg + (t - symspace_avg)/8.0; // New EQ, to assist calculating Rate
          return FALSE;         
        }
        else if (t <= 10*dash_avg)                  // Current space is not a timeout  
        {
          cwspace_avg = cwspace_avg + (t - cwspace_avg)/8.0; // (e.q. 4.9)
          x = t - cwspace_avg;                      // (e.q. 4.13)
          if (x >= 0)                               // It is a Word space
          {
            w_space = t;
            b.wspace = TRUE;
          }
        }
      }
      // Process the character 
      if (processed==FALSE) new_char=TRUE;          // Indicate there is a new char to be processed
    }
  }
 
  //-----------------------------------
  // Long key down or key up
  else if (cur_time > (10*dash_avg))
  {
    // If current state is Key up and Long key up then  Char finalized
    if (!sig[sig_incount].state && !processed)
    {
      processed = TRUE;
      b.wspace = TRUE;
      b.timeout = TRUE;
      new_char = TRUE;                              // Process the character
    }
  }

  if (data_len > DATA_BUFSIZE-2) data_len = DATA_BUFSIZE-2; // We're receiving garble, throw away
  
  if (new_char)                                     // Update circular buffer pointers for Error function
  {
    last_outcount = cur_outcount;
    cur_outcount = sig_outcount;
  }
  return new_char;                                  // TRUE if new character, else FALSE
}


//------------------------------------------------------------------
//
// The Code Generation Function converts the received
// character to a string code[] of dots and dashes
//
//------------------------------------------------------------------
void CodeGenFunc(void)
{
  uint8_t a;
  for (a = 0; a < data_len; a++)
  {
    if (data[a].state) code[a] = '-';
    else code[a] = '.';
  }
  code[a] = 0;                                      // Terminate string
  data_len = 0;                                     // And make ready for a new Char
}


//------------------------------------------------------------------
//
// The Character Identification Function applies dot/dash pattern
// recognition to identify the received character.
//
// The function returns the ASCII code for the character received,
// or 0xff if pattern was not recognized.
//
//------------------------------------------------------------------
uint8_t CharacterIdFunc(void)
 {
  uint8_t out;
 
  // Should never happen - Empty, spike suppression or similar
  if (code[0] == 0) out = 0xfe;
  
  // TODO, there are a number of ways to make this faster,
  // but this doesn't seem to be a bottleneck
  else if (strcmp(code,".-") == 0)    out ='A';
  else if (strcmp(code,"-...") == 0)  out ='B';
  else if (strcmp(code,"-.-.") == 0)  out ='C';
  else if (strcmp(code,"-..") == 0)   out ='D';
  else if (strcmp(code,".") == 0)     out ='E';
  else if (strcmp(code,"..-.") == 0)  out ='F';
  else if (strcmp(code,"--.") == 0)   out ='G';
  else if (strcmp(code,"....") == 0)  out ='H';
  else if (strcmp(code,"..") == 0)    out ='I';
  else if (strcmp(code,".---") == 0)  out ='J';
  else if (strcmp(code,"-.-") == 0)   out ='K';
  else if (strcmp(code,".-..") == 0)  out ='L';
  else if (strcmp(code,"--") == 0)    out ='M';
  else if (strcmp(code,"-.") == 0)    out ='N';
  else if (strcmp(code,"---") == 0)   out ='O';
  else if (strcmp(code,".--.") == 0)  out ='P';
  else if (strcmp(code,"--.-") == 0)  out ='Q';
  else if (strcmp(code,".-.") == 0)   out ='R';
  else if (strcmp(code,"...") == 0)   out ='S';
  else if (strcmp(code,"-") == 0)     out ='T';
  else if (strcmp(code,"..-") == 0)   out ='U';
  else if (strcmp(code,"...-") == 0)  out ='V';
  else if (strcmp(code,".--") == 0)   out ='W';
  else if (strcmp(code,"-..-") == 0)  out ='X';
  else if (strcmp(code,"-.--") == 0)  out ='Y';
  else if (strcmp(code,"--..") == 0)  out ='Z';

  else if (strcmp(code,".----") == 0) out ='1';
  else if (strcmp(code,"..---") == 0) out ='2';
  else if (strcmp(code,"...--") == 0) out ='3';
  else if (strcmp(code,"....-") == 0) out ='4';
  else if (strcmp(code,".....") == 0) out ='5';
  else if (strcmp(code,"-....") == 0) out ='6';
  else if (strcmp(code,"--...") == 0) out ='7';
  else if (strcmp(code,"---..") == 0) out ='8';
  else if (strcmp(code,"----.") == 0) out ='9';
  else if (strcmp(code,"-----") == 0) out ='0';

  else if (strcmp(code,".-.-.-") == 0) out ='.';
  else if (strcmp(code,"--..--") == 0) out =',';
  else if (strcmp(code,"-....-") == 0) out ='-';
  else if (strcmp(code,"-...-") == 0)  out ='=';
  else if (strcmp(code,"..--.-") == 0) out ='_';
  else if (strcmp(code,"-..-.") == 0)  out ='/';
  else if (strcmp(code,".-..-.") == 0) out ='\"';
  else if (strcmp(code,"..--..") == 0) out ='?';
  else if (strcmp(code,"-.-.--") == 0) out ='!';
  else if (strcmp(code,".--.-.") == 0) out ='@';
  else if (strcmp(code,"---...") == 0) out =':';
  else if (strcmp(code,"-.-.-.") == 0) out =';';
  else if (strcmp(code,"-.--.-") == 0) out =')';
  else if (strcmp(code,"...-..-") == 0)out ='$';
 
  // Prosign codes
  else if (strcmp(code,"-.--.") == 0)   out ='(';   // ( and KN - Over to you only
  else if (strcmp(code,".-...") == 0)   out ='&';   // & and AS - Wait
  else if (strcmp(code,".-.-.") == 0)   out ='+';   // + and AR - End of message
  else if (strcmp(code,"-.-.-") == 0)   out ='}';   // KA - Starting signal
  else if (strcmp(code,"...-.") == 0)   out ='~';   // SN - Understood
  else if (strcmp(code,"...-.-") == 0)  out ='>';   // SK . End of contact
  else if (strcmp(code,"-...-.-") == 0) out ='^';   // BK - Break
  else if (strcmp(code,"-.-..-..") == 0)out ='{';   // CL - Closing station
  else if (strcmp(code,"........") == 0)out = 0x7f; // Error / Correction
  else if (strcmp(code,"...---...")== 0)out = '!';  // SOS

  // This is a somewhat random collection of International Characters,
  // including Icelandic, Norwegian/Danish, Swedish...
  #if ICELAND_SYMBOLS
  else if (strcmp(code,".--..") == 0)  out = 0;     // 'Þ'
  else if (strcmp(code,"..--.") == 0)  out = 1;     // 'Ð'
  #endif
  #if ICELAND_SYMBOLS || NOR_DEN_SYMBOLS
  else if (strcmp(code,".-.-") == 0)   out = 2;     // 'Æ' - overlaps with 'Ä'
  #endif
  #if SWEDEN_SYMBOLS
  else if (strcmp(code,".-.-") == 0)   out = 6;     // 'Ä' - overlaps with 'Æ'
  #endif
  #if ICELAND_SYMBOLS || SWEDEN_SYMBOLS
  else if (strcmp(code,"---.") == 0)   out = 3;     // 'Ö'  - overlaps with 'Ø'
  #endif
  #if NOR_DEN_SYMBOLS
  else if (strcmp(code,"---.") == 0)   out = 4;     // 'Ø'  - overlaps with 'Ö'
  #endif
  #if NOR_DEN_SYMBOLS || SWEDEN_SYMBOLS
  else if (strcmp(code,".--.-") == 0)  out = 5;     // 'Å'
  #endif
  #if REST_SYMBOLS
  else if (strcmp(code,"..--") == 0)   out = 7;     // 'Ü'
  else if (strcmp(code,"..-..") == 0)  out = 8;     // 'É'
  else if (strcmp(code,"--.--") == 0)  out = 9;     // 'Ñ'
  else if (strcmp(code,"----") == 0)   out = 10;    // 'Ch'
  #endif
  
  else                                              // No code identified - error correction routine
  {
    out = 0xff;                                     // 0xff selected to indicate ERROR
  }
  return out;
}


//------------------------------------------------------------------
//
// The Print Character Function prints to LCD and Serial (USB)
//
//------------------------------------------------------------------
void PrintCharFunc(uint8_t c)
{    
#if 0
  //--------------------------------------
  // Print Characters to Serial (USB) port
  //
  // International (non ASCII characters) 
  if (c == 0)      Serial.print("Þ");
  else if (c == 1) Serial.print("Ð");
  else if (c == 2) Serial.print("Æ");               // Same as Prosign AA
  else if (c == 3) Serial.print("Ö");               // OE
  else if (c == 4) Serial.print("Ø");               // Same as OE
  else if (c == 5) Serial.print("Å");               // Could also be Á
  else if (c == 6) Serial.print("Ä");               // Same as AE
  else if (c == 7) Serial.print("Ü");
  else if (c == 8) Serial.print("É");               // Could also be Á
  else if (c == 9) Serial.print("Ñ");               // Same as AE
  else if (c == 10) Serial.print("Ch");
  
  // Prosigns
  else if (c == '}') Serial.print("ka");            // KA (starting signal)
  else if (c == '(') Serial.print("kn");            // KN (over to you only)
  else if (c == '&') Serial.print("as");            // AS (wait)
  else if (c == '~') Serial.print("sn");            // SN (understood)
  else if (c == '>') Serial.print("sk");            // SK (end of contact)
  else if (c == '+') Serial.print("ar");            // AR (end of message)
  else if (c == '^') Serial.print("bk");            // BK (break)
  else if (c == '{') Serial.print("cl");            // CL (closing station)
  else if (c == '!') Serial.print("sos");           // SOS
  else if (c == 0x7f)Serial.print("err");           // Symbol for Error

  else if (c == 0xff)Serial.print('#');             // Unresolved Error

  // Normal Characters
  else Serial.print((char) c);                      // Serial print ASCII character

  //--------------------------------------
  // Print Characters to LCD 
  
  //--------------------------------------
  // International UTF-8 encoded (non ASCII) characters
  #if ICELAND_SYMBOLS || NOR_DAN_SYMBOLS || SWEDEN_SYMBOLS || REST_SYMBOLS
  char tmp[3];                                      // Accommodate UTF-8 encoded characters
  if (c == 0) 
  {
    strcpy(tmp,"Þ");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 1)
  {
    strcpy(tmp,"Ð");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 2)
  {
    strcpy(tmp,"Æ");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 3)
  {
    strcpy(tmp,"Ö");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 4)
  {
    strcpy(tmp,"Ø");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 5)
  {
    strcpy(tmp,"Å");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 6)
  {
    strcpy(tmp,"Ä");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 7)
  {
    strcpy(tmp,"Ü");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }

  else if (c == 8)
  {
    strcpy(tmp,"É");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 9)
  {
    strcpy(tmp,"Ñ");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  else if (c == 10)
  {
    strcpy(tmp,"Ch");
    lcdLineScrollPrint(tmp[0]);
    lcdLineScrollPrint(tmp[1]);
  }
  #endif
   
  //--------------------------------------
  // Prosigns
  if (c == '}')
  {
    lcdLineScrollPrint('k');
    lcdLineScrollPrint('a');   
  }
  else if (c == '(')
  {
    lcdLineScrollPrint('k');
    lcdLineScrollPrint('n');   
  }
  else if (c == '&')
  {
    lcdLineScrollPrint('a');
    lcdLineScrollPrint('s');   
  }
  else if (c == '~')
  {
    lcdLineScrollPrint('s');
    lcdLineScrollPrint('n');   
  }
  else if (c == '>')
  {
    lcdLineScrollPrint('s');
    lcdLineScrollPrint('k');   
  }
  else if (c == '+')
  {
    lcdLineScrollPrint('a');
    lcdLineScrollPrint('r');   
  }
  else if (c == '^')
  {
    lcdLineScrollPrint('b');
    lcdLineScrollPrint('k');   
  }
  else if (c == '{')
  {
    lcdLineScrollPrint('c');
    lcdLineScrollPrint('l');   
  }
  else if (c == '!')
  {
    lcdLineScrollPrint('s');
    lcdLineScrollPrint('o');   
    lcdLineScrollPrint('s');   
  }
  else if (c == 0x7f)
  {
    lcdLineScrollPrint('e');
    lcdLineScrollPrint('r');   
    lcdLineScrollPrint('r');   
  }
  
  //--------------------------------------
  // # is our designated ERROR Symbol
  else if (c == 0xff)
  {
    lcdLineScrollPrint('#');
  }
  
  //--------------------------------------
  // Normal Characters
  else lcdLineScrollPrint(c);
#else //if 0
  //The acsii chars that map directly to the 2x16 LCD char set.
  if( (c >= '!') && (c <= '}') ) morsePrint(c);
  // The 'error' char
  if( c == 0xff ) morsePrint(c);
#endif
}


//------------------------------------------------------------------
//
// The Word Space Function takes care of Word Spaces
// to LCD and Serial (USB).  
// Word Space Correction is applied if certain characters, which
// are less likely to be at the end of a word, are received
// The characters tested are applicable to the English language
//
//------------------------------------------------------------------
void WordSpaceFunc(uint8_t c)
{
  int16_t x;                                        // Temp comparison value 

  if (b.wspace == TRUE)                             // Print word space
  {
    b.wspace = FALSE;
    
    // Word space correction routine - longer space required if certain characters    
    if ((c=='I')||(c=='J')||(c=='Q')||(c=='U')||(c=='V')||(c=='Z'))
    {
      x = (cwspace_avg + pulse_avg) - w_space;      // (e.q. 4.15)
      if (x < 0)
      {
        Serial.print(' ');
        morsePrint(' ');
        //lcdLineScrollPrint(' ');
      }
    }
    
    else
    {
      Serial.print(' ');
      morsePrint(' ');
      //lcdLineScrollPrint(' ');
    }    
  }
}


//------------------------------------------------------------------
//
// Error Correction Function has three parts
// 1) Exits with Error if character is too long (DATA_BUFSIZE-2)
// 2) If a dot duration is determined to be less than half expected,
// then this dot is eliminated by adding it and the two spaces on 
// either side to for a new space duration, then new code is generated
// for pattern parsing.
// 3) If not 2) then separate two run-on characters caused by
// a short character space - Extend the char space and reprocess
//
// If not able to resolve anything, then return FALSE
// Return TRUE if something was resolved.
//
//------------------------------------------------------------------
bool ErrorCorrectionFunc(void)
{
  int32_t  pduration;                 // Short pulse durationa and location
  int32_t  plocation;
  int32_t  sduration;                 // Long symbol space duration and location
  int32_t  slocation;
  int32_t  temp_outcount;
  uint8_t  decoded[]  = {0xff,0xff};
  bool     result     = FALSE;        // Result of Error resolution - FALSE if nothing resolved

  uint64_t freezetime = millis();     // Guard against misbehaviour of DataRecognitionFunc()
  
  if (data_len >= DATA_BUFSIZE-2)     // Too long char received
  {
    PrintCharFunc(0xff);              // Print Error to LCD and Serial (USB)
    WordSpaceFunc(0xff);              // Print Word Space to LCD and Serial when required
    // Debug
    Serial.println();
    Serial.println("{{long_char}}");  
  }

  else
  {
    b.wspace = FALSE;
    //-----------------------------------------------------
    // Find the location of pulse with shortest duration
    // and the location of symbol space of longest duration
    temp_outcount = last_outcount;    // Grab a copy of endpos for last successful decode
    slocation     = last_outcount;
    plocation     = last_outcount;
    pduration     = 32767;            // Very high number to decrement for min pulse duration
    sduration     = 0;                // and a zero to increment for max symbol space duration
    while (temp_outcount != cur_outcount)
    {
      //-----------------------------------------------------
      // Find shortest pulse duration. Only test key-down states     
      if (sig[temp_outcount].state)
      {
        #if SPIKECANCEL               // Squash spikes/transients of short duration
        if ((sig[temp_outcount].time < pduration) && (sig[temp_outcount].time > SPIKECANCEL))
        #elif SHORTCANCEL             // Squash spikes shorter than 1/3rd dot duration
        if ((sig[temp_outcount].time < pduration) &&  ((3*sig[temp_outcount].time)>dot_avg))
        #else
        if (sig[temp_outcount].time < pduration) 
        #endif
        {
          pduration = sig[temp_outcount].time;
          plocation = temp_outcount;
        }
      }
      
      //-----------------------------------------------------
      // Find longest symbol space duration. Do not test first state
      // or last state and only test key-up states
      if ((temp_outcount != last_outcount) &&
          (temp_outcount != (cur_outcount-1)) &&
          (!sig[temp_outcount].state))   
      {
        if (sig[temp_outcount].time > sduration)
        {
          sduration = sig[temp_outcount].time;
          slocation = temp_outcount;
        }
      }
      temp_outcount++;
      if (temp_outcount == SIG_BUFSIZE) temp_outcount = 0;
    }

    //-----------------------------------------------------
    // Take corrective action by dropping shortest pulse
    // if shorter than half of dot_avg
    // This can result in one or more valid characters - or Error
    if ((pduration < dot_avg/2) && (plocation != temp_outcount))
    { 
      // Add up duration of short pulse and the two spaces on either side,
      // as space at pulse location + 1
      sig[((plocation+1==SIG_BUFSIZE) ? 0:plocation+1)].time =
        sig[((plocation-1<0) ? SIG_BUFSIZE-1:plocation-1)].time + 
        sig[plocation].time +
        sig[((plocation+1==SIG_BUFSIZE) ? 0:plocation+1)].time;

      // Shift the preceding data forward accordingly
      temp_outcount = ((plocation-2)<0) ? SIG_BUFSIZE+(plocation-2):plocation-2;
      while (temp_outcount != last_outcount)      
      {
        sig[(((temp_outcount+2)>=SIG_BUFSIZE) ? (temp_outcount+2)-SIG_BUFSIZE:temp_outcount+2)].time =
          sig[temp_outcount].time;
        sig[(((temp_outcount+2)>=SIG_BUFSIZE) ? (temp_outcount+2)-SIG_BUFSIZE:temp_outcount+2)].state = 
          sig[temp_outcount].state;
        temp_outcount--;
        if (temp_outcount < 0) temp_outcount = SIG_BUFSIZE-1;      
      }
      // And finally shift the startup pointer similarly
      sig_outcount = ((last_outcount+2)>=SIG_BUFSIZE) ? (last_outcount+2)-SIG_BUFSIZE:last_outcount+2;
      //
      // Now we reprocess
      //
      // Pull out a character, using the adjusted sig[] buffer
      //while(!DataRecognitionFunc());  // Process character delimited by character or word space
      while((!DataRecognitionFunc()) && (millis() < freezetime+2));
      CodeGenFunc();                  // Generate a dot/dash pattern string
      decoded[0]=CharacterIdFunc();   // Convert dot/dash data into a character
      if (decoded[0] != 0xff)
      {
        PrintCharFunc(decoded[0]);
        result = TRUE;                // Error correction had success.
      }
      else PrintCharFunc(0xff);
    } 

    //-----------------------------------------------------
    // Take corrective action by converting the longest symbol space to character space
    // This will result in two valid characters - or Error
    else
    { 
      // Split char in two by adjusting time of longest sym space to a char space
      sig[slocation].time = ((cwspace_avg-1)>=1?cwspace_avg-1:1); // Make sure it is always larger than 0
      sig_outcount = last_outcount;   // Set circ buffer reference to the start of previous failed decode
      //
      // Now we reprocess          
      //
      // Debug - If timing is out of whack because of noise, with rate
      // showing at >99 WPM, then DataRecognitionFunc() occasionally fails.
      // Not found out why, but millis() is used to guards against it.

      // Process first character delimited by character or word space
      while((!DataRecognitionFunc()) && (millis() < freezetime+2));
      CodeGenFunc();                  // Generate a dot/dash pattern string
      decoded[0]=CharacterIdFunc();   // Convert dot/dash pattern into a character
      // Process second character delimited by character or word space
      while((!DataRecognitionFunc()) && (millis() < freezetime+2));
      CodeGenFunc();                  // Generate a dot/dash pattern string
      decoded[1]=CharacterIdFunc();   // Convert dot/dash pattern into a character
      
      if ((decoded[0] != 0xff) && (decoded[1] != 0xff)) // If successful error resolution
      {
        PrintCharFunc(decoded[0]);
        PrintCharFunc(decoded[1]);    
        result = TRUE;                // Error correction had success.
      }
      else PrintCharFunc(0xff);
    } 
  }
  return result;
}

uint32_t tf3lj_getWPM(void) {
  uint32_t spd;
  float32_t spdcalc;
  
  //-----------------------------------
  // Word time. Formula based on the word "PARIS"
  spdcalc = 10.0*dot_avg + 4.0*dash_avg + 9.0*symspace_avg + 5.0*cwspace_avg;
  spdcalc = spdcalc*1000.0/344.0;                 // Convert to Milliseconds per Word
  spd = (0.5 + 60000.0 / spdcalc);                // Convert to Words per Minute (WPM)
  return spd;
}
