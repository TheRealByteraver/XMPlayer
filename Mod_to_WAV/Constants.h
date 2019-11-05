#pragma once

// color constants for functions that show debug info
#define FOREGROUND_BLACK        0
#define FOREGROUND_CYAN         (FOREGROUND_BLUE        | FOREGROUND_GREEN)              
#define FOREGROUND_MAGENTA      (FOREGROUND_BLUE        | FOREGROUND_RED)                
#define FOREGROUND_BROWN        (FOREGROUND_GREEN       | FOREGROUND_RED)               
#define FOREGROUND_LIGHTGRAY    (FOREGROUND_BLUE        | FOREGROUND_GREEN | FOREGROUND_RED)
#define FOREGROUND_DARKGRAY     (FOREGROUND_BLACK       | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTBLUE    (FOREGROUND_BLUE        | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTGREEN   (FOREGROUND_GREEN       | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTCYAN    (FOREGROUND_CYAN        | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTRED     (FOREGROUND_RED         | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTMAGENTA (FOREGROUND_MAGENTA     | FOREGROUND_INTENSITY)
#define FOREGROUND_YELLOW       (FOREGROUND_BROWN       | FOREGROUND_INTENSITY)
#define FOREGROUND_WHITE        (FOREGROUND_LIGHTGRAY   | FOREGROUND_INTENSITY)

#define BACKGROUND_BLACK        0
#define BACKGROUND_CYAN         (BACKGROUND_BLUE        | BACKGROUND_GREEN)              
#define BACKGROUND_MAGENTA      (BACKGROUND_BLUE        | BACKGROUND_RED)                
#define BACKGROUND_BROWN        (BACKGROUND_GREEN       | BACKGROUND_RED)               
#define BACKGROUND_LIGHTGRAY    (BACKGROUND_BLUE        | BACKGROUND_GREEN | BACKGROUND_RED)
#define BACKGROUND_DARKGRAY     (BACKGROUND_BLACK       | BACKGROUND_INTENSITY)
#define BACKGROUND_LIGHTBLUE    (BACKGROUND_BLUE        | BACKGROUND_INTENSITY)
#define BACKGROUND_LIGHTGREEN   (BACKGROUND_GREEN       | BACKGROUND_INTENSITY)
#define BACKGROUND_LIGHTCYAN    (BACKGROUND_CYAN        | BACKGROUND_INTENSITY)
#define BACKGROUND_LIGHTRED     (BACKGROUND_RED         | BACKGROUND_INTENSITY)
#define BACKGROUND_LIGHTMAGENTA (BACKGROUND_MAGENTA     | BACKGROUND_INTENSITY)
#define BACKGROUND_YELLOW       (BACKGROUND_BROWN       | BACKGROUND_INTENSITY)
#define BACKGROUND_WHITE        (BACKGROUND_LIGHTGRAY   | BACKGROUND_INTENSITY)

// More general constants: 
#define PAL_CALC                            7093789.2   // these values are
#define NTSC_CALC                           7159090.5   // not used 
#define NTSC_C4_SPEED                       8363.42
#define PAL_C4_SPEED                        8287.14

#define PANNING_STYLE_MOD                   1   // LRRL etc
#define PANNING_STYLE_XM                    2   // ALL CENTER
#define PANNING_STYLE_S3M                   3   // LRLR etc
#define PANNING_STYLE_IT                    4
#define MARKER_PATTERN                      254 // S3M/IT compatibility
#define END_OF_SONG_MARKER                  255 // S3M/IT end of song marker

#define MAX_VOLUME                          64
#define MAX_SAMPLENAME_LENGTH               22
#define MAX_INSTRUMENTNAME_LENGTH           (MAX_SAMPLENAME_LENGTH + 2)
#define MAX_PATTERNS                        256
#define MAX_INSTRUMENTS                     256
#define MAX_SAMPLES                         (128 * 16) // FT2
#define SAMPLEDATA_EXTENSION                256 // add 256 samples to the data for noise reduction
#define SAMPLEDATA_TYPE_UNKNOWN             0
#define SAMPLEDATA_SIGNED_8BIT              1
#define SAMPLEDATA_SIGNED_16BIT             2
#define INTERPOLATION_SPACER                2
#define MAX_EFFECT_COLUMNS                  2
#define MAXIMUM_NOTES                       (11 * 12)
#define PLAYER_MAX_CHANNELS                 32
#define PANNING_FULL_LEFT                   0
#define PANNING_MAX_STEPS                   256  // must be a power of two
#define PANNING_SHIFT                       8   // divide by 256 <=> SHR 8
#define PANNING_CENTER                      (PANNING_MAX_STEPS / 2)
#define PANNING_FULL_RIGHT                  (PANNING_MAX_STEPS - 1)

#define FORWARD                             false
#define BACKWARD                            true

// differentiate trackers, for S3M compatibility mainly
#define TRACKER_PROTRACKER                  1
#define TRACKER_ST300                       2
#define TRACKER_ST321                       3
#define TRACKER_FT2                         4
#define TRACKER_IT                          5

// New Note Action constants
#define NNA_NOTE_CUT                        0
#define NNA_NOTE_CONTINUE                   1
#define NNA_NOTE_OFF                        2
#define NNA_NOTE_FADE                       3

// effect nrs:
#define NO_EFFECT                           0x0 // ARPEGGIO is remapped to 0x25
#define PORTAMENTO_UP                       0x1
#define PORTAMENTO_DOWN                     0x2
#define TONE_PORTAMENTO                     0x3
#define VIBRATO                             0x4
#define TONE_PORTAMENTO_AND_VOLUME_SLIDE    0x5
#define VIBRATO_AND_VOLUME_SLIDE            0x6
#define TREMOLO                             0x7
#define SET_FINE_PANNING                    0x8
#define SET_SAMPLE_OFFSET                   0x9
#define VOLUME_SLIDE                        0xA
#define POSITION_JUMP                       0xB
#define SET_VOLUME                          0xC
#define PATTERN_BREAK                       0xD
#define EXTENDED_EFFECTS                    0xE // extended effects
#define SET_FILTER                          0x0 // XM effect E0
#define FINE_PORTAMENTO_UP                  0x1 // XM effect E1
#define FINE_PORTAMENTO_DOWN                0x2 // XM effect E2
#define SET_GLISSANDO_CONTROL               0x3 // XM effect E3
#define SET_VIBRATO_CONTROL                 0x4 // XM effect E4
#define SET_FINETUNE                        0x5 // XM effect E5
#define SET_PATTERN_LOOP                    0x6 // XM effect E6
#define SET_TREMOLO_CONTROL                 0x7 // XM effect E7
#define SET_ROUGH_PANNING                   0x8 // XM effect E8, not used in player
#define NOTE_RETRIG                         0x9 // XM effect E9
#define FINE_VOLUME_SLIDE_UP                0xA // XM effect EA
#define FINE_VOLUME_SLIDE_DOWN              0xB // XM effect EB
#define NOTE_CUT                            0xC // XM effect EC
#define NOTE_DELAY                          0xD // XM effect ED
#define PATTERN_DELAY                       0xE // XM effect EE
#define INVERT_LOOP                         0xF // XM effect EF, end of XM extended effects
#define S3M_SET_GLISSANDO_CONTROL           0x1 // S3M effect S1
#define S3M_SET_FINETUNE                    0x2 // S3M effect S2
#define S3M_SET_VIBRATO_CONTROL             0x3 // S3M effect S3
#define S3M_SET_TREMOLO_CONTROL             0x4 // S3M effect S4
#define S3M_SET_PANBRELLO_CONTROL           0x5 // S3M effect S5
#define S3M_FINE_PATTERN_DELAY              0x6 // S3M effect S6
#define S3M_SET_ROUGH_PANNING               0x8 // S3M effect S8
#define S3M_SOUND_CONTROL                   0x9 // S3M effect S9
#define S3M_SET_HIGH_SAMPLE_OFFSET          0xA // S3M effect SA
#define S3M_SET_PATTERN_LOOP                0xB // S3M effect SB
#define S3M_NOTE_CUT                        0xC // S3M effect SC
#define S3M_NOTE_DELAY                      0xD // S3M effect SD
#define S3M_PATTERN_DELAY                   0xE // S3M effect SE, end of S3M extended effects
#define SET_TEMPO                           0xF 
#define SET_GLOBAL_VOLUME                   0x10// XM effect G
#define GLOBAL_VOLUME_SLIDE                 0x11// XM effect H
#define SET_ENVELOPE_POSITION               0x15// XM effect L
#define PANNING_SLIDE                       0x19// XM effect P
#define MULTI_NOTE_RETRIG                   0x1B// XM effect R
#define TREMOR                              0x1D// XM effect T
#define EXTRA_FINE_PORTAMENTO               0x21// XM effect X
#define EXTRA_FINE_PORTAMENTO_UP            0x1 // XM effect X1
#define EXTRA_FINE_PORTAMENTO_DOWN          0x2 // XM effect X2
#define PANBRELLO                           0x22// S3M effect Y 

// internal remapped effects for the player
#define SET_BPM                             0x24// after effect "Z" for XM safety
#define ARPEGGIO                            0x25
#define FINE_VIBRATO                        0x26// S3M fine vibrato
#define SET_VIBRATO_SPEED                   0x27// XM Volc command
#define KEY_OFF                             255 //(12 * 11 + 1) // 11 octaves
#define KEY_NOTE_CUT                        254
#define KEY_NOTE_FADE                       253

// for debugging only:
const char * const noteStrings[2 + MAXIMUM_NOTES] = {
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

#define VIBRATO_SINEWAVE                    0
#define VIBRATO_RAMPDOWN                    1
#define VIBRATO_SQUAREWAVE                  2
#define VIBRATO_RANDOM                      3
#define VIBRATO_NO_RETRIG_FLAG              4

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

typedef unsigned __int16 AMIGAWORD;
typedef signed   __int16 SHORT;

