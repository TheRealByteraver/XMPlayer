#pragma once

#include <windows.h> // for the color constants

// color constants for functions that show debug info
constexpr auto FOREGROUND_BLACK        = 0;
constexpr auto FOREGROUND_CYAN         = (FOREGROUND_BLUE        | FOREGROUND_GREEN);              
constexpr auto FOREGROUND_MAGENTA      = (FOREGROUND_BLUE        | FOREGROUND_RED);                
constexpr auto FOREGROUND_BROWN        = (FOREGROUND_GREEN       | FOREGROUND_RED);               
constexpr auto FOREGROUND_LIGHTGRAY    = (FOREGROUND_BLUE        | FOREGROUND_GREEN | FOREGROUND_RED);
constexpr auto FOREGROUND_DARKGRAY     = (FOREGROUND_BLACK       | FOREGROUND_INTENSITY);
constexpr auto FOREGROUND_LIGHTBLUE    = (FOREGROUND_BLUE        | FOREGROUND_INTENSITY);
constexpr auto FOREGROUND_LIGHTGREEN   = (FOREGROUND_GREEN       | FOREGROUND_INTENSITY);
constexpr auto FOREGROUND_LIGHTCYAN    = (FOREGROUND_CYAN        | FOREGROUND_INTENSITY);
constexpr auto FOREGROUND_LIGHTRED     = (FOREGROUND_RED         | FOREGROUND_INTENSITY);
constexpr auto FOREGROUND_LIGHTMAGENTA = (FOREGROUND_MAGENTA     | FOREGROUND_INTENSITY);
constexpr auto FOREGROUND_YELLOW       = (FOREGROUND_BROWN       | FOREGROUND_INTENSITY);
constexpr auto FOREGROUND_WHITE        = (FOREGROUND_LIGHTGRAY   | FOREGROUND_INTENSITY);

constexpr auto BACKGROUND_BLACK        = 0;
constexpr auto BACKGROUND_CYAN         = (BACKGROUND_BLUE        | BACKGROUND_GREEN);              
constexpr auto BACKGROUND_MAGENTA      = (BACKGROUND_BLUE        | BACKGROUND_RED);                
constexpr auto BACKGROUND_BROWN        = (BACKGROUND_GREEN       | BACKGROUND_RED);               
constexpr auto BACKGROUND_LIGHTGRAY    = (BACKGROUND_BLUE        | BACKGROUND_GREEN | BACKGROUND_RED);
constexpr auto BACKGROUND_DARKGRAY     = (BACKGROUND_BLACK       | BACKGROUND_INTENSITY);
constexpr auto BACKGROUND_LIGHTBLUE    = (BACKGROUND_BLUE        | BACKGROUND_INTENSITY);
constexpr auto BACKGROUND_LIGHTGREEN   = (BACKGROUND_GREEN       | BACKGROUND_INTENSITY);
constexpr auto BACKGROUND_LIGHTCYAN    = (BACKGROUND_CYAN        | BACKGROUND_INTENSITY);
constexpr auto BACKGROUND_LIGHTRED     = (BACKGROUND_RED         | BACKGROUND_INTENSITY);
constexpr auto BACKGROUND_LIGHTMAGENTA = (BACKGROUND_MAGENTA     | BACKGROUND_INTENSITY);
constexpr auto BACKGROUND_YELLOW       = (BACKGROUND_BROWN       | BACKGROUND_INTENSITY);
constexpr auto BACKGROUND_WHITE        = (BACKGROUND_LIGHTGRAY   | BACKGROUND_INTENSITY);

// for debugging only:
const char* const noteStrings[] = { // 2 + MAXIMUM_NOTES entries
    "---",
    "C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0",
    "C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1",
    "C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2",
    "C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3",
    "C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4",
    "C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5",
    "C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6",
    "C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7",
    "C-8","C#8","D-8","D#8","E-8","F-8","F#8","G-8","G#8","A-8","A#8","B-8",
    "C-9","C#9","D-9","D#9","E-9","F-9","F#9","G-9","G#9","A-9","A#9","B-9",
    "C-A","C#A","D-A","D#A","E-A","F-A","F#A","G-A","G#A","A-A","A#A","B-A",
    "==="
};

// General constants for the player: 
constexpr auto PAL_CALC         = 7093789.2;    // these values are
constexpr auto NTSC_CALC        = 7159090.5;   // not used 
constexpr auto NTSC_C4_SPEED    = 8363.42;
constexpr auto PAL_C4_SPEED     = 8287.14;

constexpr auto PANNING_STYLE_MOD    = 1;    // LRRL etc
constexpr auto PANNING_STYLE_XM     = 2;    // ALL CENTER
constexpr auto PANNING_STYLE_S3M    = 3;    // LRLR etc
constexpr auto PANNING_STYLE_IT     = 4;
constexpr auto MARKER_PATTERN       = 254;  // S3M/IT compatibility
constexpr auto END_OF_SONG_MARKER   = 255;  // S3M/IT end of song marker

constexpr auto MAX_VOLUME                   = 64;
constexpr auto MAX_SAMPLENAME_LENGTH        = 22;
constexpr auto MAX_INSTRUMENTNAME_LENGTH    = (MAX_SAMPLENAME_LENGTH + 2);
constexpr auto MAX_PATTERNS                 = 256;
constexpr auto MAX_INSTRUMENTS              = 256;
constexpr auto MAX_SAMPLES                  = (128 * 16); // FT2
constexpr auto SAMPLEDATA_EXTENSION         = 256; // add 256 samples to the data for noise reduction
constexpr auto SAMPLEDATA_TYPE_UNKNOWN      = 64;
constexpr auto SAMPLEDATA_UNSIGNED_8BIT     = 0;   // 1st bit set == signed data
constexpr auto SAMPLEDATA_SIGNED_8BIT       = 1;
constexpr auto SAMPLEDATA_UNSIGNED_16BIT    = 2;  // 2nd bit set == 16bit data
constexpr auto SAMPLEDATA_SIGNED_16BIT      = 3;
constexpr auto INTERPOLATION_SPACER         = 2;
constexpr auto MAX_EFFECT_COLUMNS           = 2;
constexpr auto MAXIMUM_NOTES                = (11 * 12);
constexpr auto PLAYER_MAX_CHANNELS          = 32;
constexpr auto PANNING_FULL_LEFT            = 0;
constexpr auto PANNING_MAX_STEPS            = 256; // must be a power of two
constexpr auto PANNING_SHIFT                = 8;   // divide by 256 <=> SHR 8
constexpr auto PANNING_CENTER               = (PANNING_MAX_STEPS / 2);
constexpr auto PANNING_FULL_RIGHT           = (PANNING_MAX_STEPS - 1);

constexpr auto FORWARD = false;
constexpr auto BACKWARD = true;

// differentiate trackers, for S3M compatibility mainly
constexpr auto  TRACKER_PROTRACKER = 1;
constexpr auto  TRACKER_ST300      = 2;
constexpr auto  TRACKER_ST321      = 3;
constexpr auto  TRACKER_FT2        = 4;
constexpr auto  TRACKER_IT         = 5;

// New Note Action constants
constexpr auto  NNA_NOTE_CUT      = 0;
constexpr auto  NNA_NOTE_CONTINUE = 1;
constexpr auto  NNA_NOTE_OFF      = 2;
constexpr auto  NNA_NOTE_FADE     = 3;
                            
// effect nrs:
constexpr auto  NO_EFFECT                        = 0x0; // ARPEGGIO is remapped to 0x25
constexpr auto  PORTAMENTO_UP                    = 0x1;
constexpr auto  PORTAMENTO_DOWN                  = 0x2;
constexpr auto  TONE_PORTAMENTO                  = 0x3;
constexpr auto  VIBRATO                          = 0x4;
constexpr auto  TONE_PORTAMENTO_AND_VOLUME_SLIDE = 0x5;
constexpr auto  VIBRATO_AND_VOLUME_SLIDE         = 0x6;
constexpr auto  TREMOLO                          = 0x7;
constexpr auto  SET_FINE_PANNING                 = 0x8;
constexpr auto  SET_SAMPLE_OFFSET                = 0x9;
constexpr auto  VOLUME_SLIDE                     = 0xA;
constexpr auto  POSITION_JUMP                    = 0xB;
constexpr auto  SET_VOLUME                       = 0xC;
constexpr auto  PATTERN_BREAK                    = 0xD;
constexpr auto  EXTENDED_EFFECTS                 = 0xE; // extended effects
constexpr auto  SET_FILTER                       = 0x0; // XM effect E0
constexpr auto  FINE_PORTAMENTO_UP               = 0x1; // XM effect E1
constexpr auto  FINE_PORTAMENTO_DOWN             = 0x2; // XM effect E2
constexpr auto  SET_GLISSANDO_CONTROL            = 0x3; // XM effect E3
constexpr auto  SET_VIBRATO_CONTROL              = 0x4; // XM effect E4
constexpr auto  SET_FINETUNE                     = 0x5; // XM effect E5
constexpr auto  SET_PATTERN_LOOP                 = 0x6; // XM effect E6
constexpr auto  SET_TREMOLO_CONTROL              = 0x7; // XM effect E7
constexpr auto  SET_ROUGH_PANNING                = 0x8; // XM effect E8, not used in player
constexpr auto  NOTE_RETRIG                      = 0x9; // XM effect E9
constexpr auto  FINE_VOLUME_SLIDE_UP             = 0xA; // XM effect EA
constexpr auto  FINE_VOLUME_SLIDE_DOWN           = 0xB; // XM effect EB
constexpr auto  NOTE_CUT                         = 0xC; // XM effect EC
constexpr auto  NOTE_DELAY                       = 0xD; // XM effect ED
constexpr auto  PATTERN_DELAY                    = 0xE; // XM effect EE
constexpr auto  INVERT_LOOP                      = 0xF; // XM effect EF, end of XM extended effects
constexpr auto  S3M_SET_GLISSANDO_CONTROL        = 0x1; // S3M effect S1
constexpr auto  S3M_SET_FINETUNE                 = 0x2; // S3M effect S2
constexpr auto  S3M_SET_VIBRATO_CONTROL          = 0x3; // S3M effect S3
constexpr auto  S3M_SET_TREMOLO_CONTROL          = 0x4; // S3M effect S4
constexpr auto  S3M_SET_PANBRELLO_CONTROL        = 0x5; // S3M effect S5
constexpr auto  S3M_FINE_PATTERN_DELAY           = 0x6; // S3M effect S6
constexpr auto  S3M_SET_ROUGH_PANNING            = 0x8; // S3M effect S8
constexpr auto  S3M_SOUND_CONTROL                = 0x9; // S3M effect S9
constexpr auto  S3M_SET_HIGH_SAMPLE_OFFSET       = 0xA; // S3M effect SA
constexpr auto  S3M_SET_PATTERN_LOOP             = 0xB; // S3M effect SB
constexpr auto  S3M_NOTE_CUT                     = 0xC; // S3M effect SC
constexpr auto  S3M_NOTE_DELAY                   = 0xD; // S3M effect SD
constexpr auto  S3M_PATTERN_DELAY                = 0xE; // S3M effect SE, end of S3M extended effects
constexpr auto  SET_TEMPO                        = 0xF; 
constexpr auto  SET_GLOBAL_VOLUME                = 0x10;// XM effect G
constexpr auto  GLOBAL_VOLUME_SLIDE              = 0x11;// XM effect H
constexpr auto  SET_ENVELOPE_POSITION            = 0x15;// XM effect L
constexpr auto  PANNING_SLIDE                    = 0x19;// XM effect P
constexpr auto  MULTI_NOTE_RETRIG                = 0x1B;// XM effect R
constexpr auto  TREMOR                           = 0x1D;// XM effect T
constexpr auto  EXTRA_FINE_PORTAMENTO            = 0x21;// XM effect X
constexpr auto  EXTRA_FINE_PORTAMENTO_UP         = 0x1; // XM effect X1
constexpr auto  EXTRA_FINE_PORTAMENTO_DOWN       = 0x2; // XM effect X2
constexpr auto  PANBRELLO                        = 0x22;// S3M effect Y 

// internal remapped effects for the player
constexpr auto SET_BPM                          = 0x24; // after effect "Z" for XM safety
constexpr auto ARPEGGIO                         = 0x25;
constexpr auto FINE_VIBRATO                     = 0x26; // S3M fine vibrato
constexpr auto SET_VIBRATO_SPEED                = 0x27; // XM Volc command
constexpr auto KEY_OFF                          = 255;  //(12 * 11 + 1); // 11 octaves
constexpr auto KEY_NOTE_CUT                     = 254;
constexpr auto KEY_NOTE_FADE                    = 253;

// different types of vibrato
constexpr auto VIBRATO_SINEWAVE         = 0;
constexpr auto VIBRATO_RAMPDOWN         = 1;
constexpr auto VIBRATO_SQUAREWAVE       = 2;
constexpr auto VIBRATO_RANDOM           = 3;
constexpr auto VIBRATO_NO_RETRIG_FLAG   = 4;

/*
    From MPT's test suite:
    Arpeggio behavior is very weird with more than 16 ticks per row. This comes
    from the fact that Fasttracker 2 uses a LUT for computing the arpeggio note
    (instead of doing something like tick%3 or similar). The LUT only has 16
    entries, so when there are more than 16 ticks, it reads beyond array
    boundaries. The vibrato table happens to be stored right after arpeggio
    table. The tables look like this in memory:

    ArpTab: 0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0
    VibTab: 0,24,49,74,97,120,141,161,180,197,...

    All values except for the first in the vibrato table are greater than 1, so
    they trigger the third arpeggio note. Keep in mind that Fasttracker 2
    counts downwards, so the table has to be read from back to front, i.e. at
    16 ticks per row, the 16th entry in the LUT is the first to be read. This
    is also the reason why Arpeggio is played “backwards” in Fasttracker 2.
*/
const int FT2_ArpeggioTable[] = {
    0,1,2,
    0,1,2,
    0,1,2,
    0,1,2,
    0,1,2,
    0,           // 16 values, now intrude into sine table:
    0,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2 // 32 values in total
};
/*
    Vibrato / Tremelo / Panbrello curve tables & constants:
*/
const int sineTable[] = {
    0  ,24 ,49 ,74 ,97 ,120,141,161,
    180,197,212,224,235,244,250,253,
    255,253,250,244,235,224,212,197,
    180,161,141,120,97 ,74 ,49 ,24 ,
    0
};

/*
const unsigned periods[MAXIMUM_NOTES] = {
   4*1712,4*1616,4*1524,4*1440,4*1356,4*1280,4*1208,4*1140,4*1076,4*1016, 4*960, 4*906,
   2*1712,2*1616,2*1524,2*1440,2*1356,2*1280,2*1208,2*1140,2*1076,2*1016, 2*960, 2*906,
     1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906,
      856,   808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453,
      428,   404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226,
      214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
      107,   101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56,
       53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28};
*/

const unsigned periods[] = {
    54784,51712,48768,46080,43392,40960,38656,36480,34432,32512,30720,28992,
    27392,25856,24384,23040,21696,20480,19328,18240,17216,16256,15360,14496,
    13696,12928,12192,11520,10848,10240,9664 ,9120 ,8608 ,8128 ,7680 ,7248 ,
    6848 ,6464 ,6096 ,5760 ,5424 ,5120 ,4832 ,4560 ,4304 ,4064 ,3840 ,3624 ,
    3424 ,3232 ,3048 ,2880 ,2712 ,2560 ,2416 ,2280 ,2152 ,2032 ,1920 ,1812 ,
    1712 ,1616 ,1524 ,1440 ,1356 ,1280 ,1208 ,1140 ,1076 ,1016 ,960  ,906  ,
    856  ,808  ,762  ,720  ,678  ,640  ,604  ,570  ,538  ,508  ,480  ,453  ,
    428  ,404  ,381  ,360  ,339  ,320  ,302  ,285  ,269  ,254  ,240  ,226  ,
    214  ,202  ,190  ,180  ,170  ,160  ,151  ,143  ,135  ,127  ,120  ,113  ,
    107  ,101  ,95   ,90   ,85   ,80   ,75   ,71   ,67   ,63   ,60   ,56   ,
    53   ,50   ,47   ,45   ,42   ,40   ,37   ,35   ,33   ,31   ,30   ,28   ,
    26   ,25   ,23   ,22   ,21   ,20   ,18   ,17   ,16   ,15   ,15   ,14
};

const unsigned amigaPeriodTable[] = {
    907, 900, 894, 887, 881, 875, 868, 862, 856, 850, 844, 838,
    832, 826, 820, 814, 808, 802, 796, 791, 785, 779, 774, 768,
    762, 757, 752, 746, 741, 736, 730, 725, 720, 715, 709, 704,
    699, 694, 689, 684, 678, 675, 670, 665, 660, 655, 651, 646,
    640, 636, 632, 628, 623, 619, 614, 610, 604, 601, 597, 592,
    588, 584, 580, 575, 570, 567, 563, 559, 555, 551, 547, 543,
    538, 535, 532, 528, 524, 520, 516, 513, 508, 505, 502, 498,
    494, 491, 487, 484, 480, 477, 474, 470, 467, 463, 460, 457
};

// These type definitions come in handy when loading module files
typedef unsigned __int16 AMIGAWORD;
typedef signed   __int16 SHORT;