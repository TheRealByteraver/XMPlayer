/*

Idea's for performance gain (less memory):
    - use flags instead of booleans
    - store sample and instrument names in a separate vector list rather than
      storing it together with the data needed by the mixer
    - store small values in smaller variables (volume in unsigned char etc)
    - 4-byte align them in the struct

Flags for samples:
    - is sample used in song
    - is looping sample
    - is sustain loop used
    - is ping pong loop
    - is ping pong sustain loop

Flags for instruments:
    - New Note Action: cut
    - New Note Action: continue
    - New Note Action: note off
    - New Note Action: note fade
    - Duplicate Check Type: off
    - Duplicate Check Type: note
    - Duplicate Check Type: sample
    - Duplicate Check Type: instrument
    - Duplicate Check Action: cut
    - Duplicate Check Action: note off
    - Duplicate Check Action: note fade

Other idea's:
    - further reduce pointer usage (replace with const reference)
    - don't use new and delete: 
      https://isocpp.org/wiki/faq/freestore-mgmt#double-delete-disaster
    - check if using looplength is easier / worse than loopend internally
    (in the context of samples)
    - compress patterns internally (.IT pattern compression?)

Todo:
    - implement IT & S3M effects
    - verify panning slide egde cases for different trackers


OpenMPT doc errors:

|C-501...O21
|........#02
|........#01

In this example, the hexadecimal sample offset is (21h × 10000h) + (2h × 100h) + 1h = 210201h.
In decimal, it is (33 × 65536) + (2 × 256) + 1 = 2,163,201.


Fxx  Portamento Up
or Fine Portamento Up
or Extra Fine Portamento Up  Global  Increases current note pitch by xx units on every tick of the row except the first. 

EFx finely increases note pitch by only applying x units on the first tick of the row.
EEx extra-finely increases note pitch by applying with 4 times the precision of EFx.

---> FFx & FEx

*/


#ifndef MODULE_H
#define MODULE_H

#include <bitset> // for debugging

#include "assert.h"
#include "virtualfile.h"

// color constants for functions that show debug info
#define FOREGROUND_BLACK        0
#define FOREGROUND_CYAN         (FOREGROUND_BLUE    | FOREGROUND_GREEN)              
#define FOREGROUND_MAGENTA      (FOREGROUND_BLUE    | FOREGROUND_RED)                
#define FOREGROUND_BROWN        (FOREGROUND_GREEN   | FOREGROUND_RED)               
#define FOREGROUND_LIGHTGRAY    (FOREGROUND_BLUE    | FOREGROUND_GREEN | FOREGROUND_RED)
#define FOREGROUND_DARKGRAY     (FOREGROUND_BLACK       | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTBLUE    (FOREGROUND_BLUE        | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTGREEN   (FOREGROUND_GREEN       | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTCYAN    (FOREGROUND_CYAN        | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTRED     (FOREGROUND_RED         | FOREGROUND_INTENSITY)
#define FOREGROUND_LIGHTMAGENTA (FOREGROUND_MAGENTA     | FOREGROUND_INTENSITY)
#define FOREGROUND_YELLOW       (FOREGROUND_BROWN       | FOREGROUND_INTENSITY)
#define FOREGROUND_WHITE        (FOREGROUND_LIGHTGRAY   | FOREGROUND_INTENSITY)

#define BACKGROUND_BLACK        0
#define BACKGROUND_CYAN         (BACKGROUND_BLUE    | BACKGROUND_GREEN)              
#define BACKGROUND_MAGENTA      (BACKGROUND_BLUE    | BACKGROUND_RED)                
#define BACKGROUND_BROWN        (BACKGROUND_GREEN   | BACKGROUND_RED)               
#define BACKGROUND_LIGHTGRAY    (BACKGROUND_BLUE    | BACKGROUND_GREEN | BACKGROUND_RED)
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
#define SAMPLEDATA_TYPE_UNKNOWN             0
#define SAMPLEDATA_SIGNED_8BIT              1
#define SAMPLEDATA_SIGNED_16BIT             2
#define INTERPOLATION_SPACER                2
#define MAX_EFFECT_COLUMNS                  2
#define MAXIMUM_NOTES                       (11 * 12)
#define PLAYER_MAX_CHANNELS                 32
#define PANNING_FULL_LEFT                   0
#define PANNING_CENTER                      128
#define PANNING_FULL_RIGHT                  255
#define PANNING_MAX_STEPS                   256  // must be power of two
#define PANNING_SHIFT                       8    // divide by 256 <=> SHR 8
#define FORWARD                             false
#define BACKWARD                            true

// differentiate trackers, for S3M compatibility mainly
#define TRACKER_PROTRACKER                  1
#define TRACKER_ST300                       2
#define TRACKER_ST321                       3
#define TRACKER_FT2                         4
#define TRACKER_IT                          5

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

// These type definitions are needed by the replay routines:
class Effect {
public:
    unsigned        effect;
    unsigned        argument;
};

class Note {
public:
    unsigned        note;
    unsigned        instrument;
    Effect          effects[MAX_EFFECT_COLUMNS];
};

class Pattern {
    unsigned        nChannels_;
    unsigned        nRows_;
    Note            *data_;
public:
    Pattern ()                  
    { 
        nChannels_ = 0;
        nRows_ = 0;
        data_ = nullptr;
    }
    Pattern( unsigned nChannels,unsigned nRows ) :
        nChannels_ ( nChannels ),
        nRows_ ( nRows )
    {
        data_ = new Note[nChannels * nRows];
        memset( data_,0,nChannels * nRows * sizeof( Note ) );
    }
    ~Pattern () { delete data_; }
    void            initialise( unsigned nChannels,unsigned nRows,Note *data )
    {
        nChannels_ = nChannels;
        nRows_ = nRows;
        data_ = data;
        memset( data,0,nChannels * nRows * sizeof( Note ) );
    }
    unsigned        getnRows ()                 { return nRows_;            }
    Note            getNote( unsigned n )       { return data_[n];          }
    /*
        - returns a pointer to the beginning of the pattern if row exceeds the
          maximum nr of rows in this particular pattern.
        - returns nullptr if no pattern data is present.
    */
    Note            *getRow ( unsigned row ) 
    { 
        if ( !data_ ) return nullptr;
        if ( row > nRows_ ) row = 0;
        return data_ + nChannels_ * row; 
    }
};

class SampleHeader {
public:
    SampleHeader()
    {
        name.clear();
        length = 0;
        repeatOffset = 0;
        repeatLength = 0;
        sustainRepeatStart = 0;
        sustainRepeatEnd = 0;
        isRepeatSample = false;
        isSustainedSample = false;
        isPingpongSample = false;
        isSustainedPingpongSample = false;
        isUsed = false;
        globalVolume = 64;
        volume = 64;
        relativeNote = 0;
        panning = PANNING_CENTER;
        finetune = 0;
        vibratoSpeed = 0;     // 0..64
        vibratoDepth = 0;     // 0..64
        vibratoWaveForm = 0;  // 0 = sine,1 = ramp down,2 = square,3 = random
        vibratoRate = 0;      // 0..64
        dataType = SAMPLEDATA_TYPE_UNKNOWN;
        data = nullptr;
    }
    std::string     name;
    unsigned        length;
    unsigned        repeatOffset;
    unsigned        repeatLength;
    bool            isRepeatSample;
    bool            isPingpongSample;
    bool            isSustainedSample;
    bool            isSustainedPingpongSample;
    bool            isUsed;           // if the sample is used in the song
    int             globalVolume;
    int             volume;
    int             relativeNote;
    unsigned        panning;
    int             finetune;
    int             dataType;         // 8 or 16 bit, compressed, etc
    unsigned        sustainRepeatStart;
    unsigned        sustainRepeatEnd;
    unsigned        vibratoSpeed;     // 0..64
    unsigned        vibratoDepth;     // 0..64
    unsigned        vibratoWaveForm;  // 0 = sine,1 = ramp down,2 = square,3 = random
    unsigned        vibratoRate;      // 0..64
    SHORT           *data;            // only 16 bit samples allowed                    
};

class Sample {
public:
    Sample()
    {
        name_.clear();
        length_ = 0;
        repeatOffset_ = 0;
        repeatLength_ = 0;
        isRepeatSample_ = false;
        isPingpongSample_ = false;
        isSustainedSample_ = false;
        isPingpongSustainedSample_ = false;
        isUsed_ = false;
        volume_ = false;
        relativeNote_ = 0;
        panning_ = PANNING_CENTER;
        finetune_ = 0;
        data_ = nullptr;
    }
    ~Sample();
    bool            load ( const SampleHeader &sampleHeader );
    std::string     getName ()          { return name_;             }
    unsigned        getLength ()        { return length_;           }
    unsigned        getRepeatOffset ()  { return repeatOffset_;     }
    unsigned        getRepeatLength ()  { return repeatLength_;     }
    unsigned        getRepeatEnd ()     { return repeatEnd_;        }
    bool            isRepeatSample ()   { return isRepeatSample_;   }
    bool            isPingpongSample () { return isPingpongSample_; }
    bool            isUsed ()           { return isUsed_;           }
    int             getVolume ()        { return volume_;           }
    int             getRelativeNote ()  { return relativeNote_;     }
    unsigned        getPanning ()       { return panning_;          }
    int             getFinetune ()      { return finetune_;         }
    SHORT           *getData ()         { return data_ + INTERPOLATION_SPACER; }
private:
    std::string     name_;
    unsigned        length_;
    unsigned        repeatOffset_;
    unsigned        repeatEnd_;
    unsigned        repeatLength_;
    unsigned        sustainRepeatStart_;
    unsigned        sustainRepeatEnd_;
    bool            isRepeatSample_;
    bool            isPingpongSample_;
    bool            isSustainedSample_;
    bool            isPingpongSustainedSample_;
    bool            isUsed_;            // if the sample is used in the song
    int             globalVolume_;
    int             volume_;
    int             relativeNote_;
    unsigned        panning_;
    int             finetune_;
    SHORT           *data_;             // only 16 bit samples allowed
};

class EnvelopePoint {
public:
    unsigned        x;
    unsigned        y;
};

struct NoteSampleMap {
    unsigned        note;
    unsigned        sampleNr;
};

class InstrumentHeader {
public:
    InstrumentHeader()  
    { 
        name.clear();
        nSamples = 0;
        for( int i = 0; i < MAXIMUM_NOTES; i++ )
            sampleForNote[i] = 0;
        for ( int i = 0; i < 12; i++ )  // only 12 envelope points?
        {
            volumeEnvelope[i].x = 0;
            volumeEnvelope[i].y = 0;
            panningEnvelope[i].x = 0;
            panningEnvelope[i].y = 0;
        }
        nVolumePoints = 0;
        volumeSustain = 0;
        volumeLoopStart = 0;
        volumeLoopEnd = 0;
        volumeType = 0;
        volumeFadeOut = 0;
                            
        nPanningPoints = 0;
        panningSustain = 0;
        panningLoopStart = 0;
        panningLoopEnd = 0;
        panningType = 0;
        vibratoType = 0;
        vibratoSweep = 0;
        vibratoDepth = 0;
        vibratoRate = 0;
    }
    std::string     name;
    unsigned        nSamples;
    unsigned        sampleForNote[MAXIMUM_NOTES];
    EnvelopePoint   volumeEnvelope[12];
    unsigned        nVolumePoints;
    unsigned        volumeSustain;
    unsigned        volumeLoopStart;
    unsigned        volumeLoopEnd;
    unsigned        volumeType;
    unsigned        volumeFadeOut;
    EnvelopePoint   panningEnvelope[12];
    unsigned        nPanningPoints;
    unsigned        panningSustain;
    unsigned        panningLoopStart;
    unsigned        panningLoopEnd;
    unsigned        panningType;
    unsigned        vibratoType;
    unsigned        vibratoSweep;
    unsigned        vibratoDepth;
    unsigned        vibratoRate;
};

class Instrument {
public:
    Instrument ()
    { 
        name_.clear();
        nSamples_ = 1;
        for ( int i = 0; i < MAXIMUM_NOTES; i++ )
            sampleForNote_[i] = 0;
        for ( int i = 0; i < 12; i++ )  // only 12 envelope points?
        {
            volumeEnvelope_[i].x = 0;
            volumeEnvelope_[i].y = 0;
            panningEnvelope_[i].x = 0;
            panningEnvelope_[i].y = 0;
        }
        nVolumePoints_ = 0;
        volumeSustain_ = 0;
        volumeLoopStart_ = 0;
        volumeLoopEnd_ = 0;
        volumeType_ = 0;
        volumeFadeOut_ = 0;
        nPanningPoints_ = 0;
        panningSustain_ = 0;
        panningLoopStart_ = 0;
        panningLoopEnd_ = 0;
        panningType_ = 0;
        vibratoType_ = 0;
        vibratoSweep_ = 0;
        vibratoDepth_ = 0;
        vibratoRate_ = 0;
    }
                    ~Instrument ();
    void            load( const InstrumentHeader &instrumentHeader );
    std::string     getName() { return name_; }
    unsigned        getnSamples ()                  { return nSamples_;           }
    unsigned        getSampleForNote( unsigned n )
    { 
        assert ( n < MAXIMUM_NOTES );  // has no effect
        if ( n >= MAXIMUM_NOTES ) return 0;
        return sampleForNote_[n];   
    }
    EnvelopePoint   getVolumeEnvelope ( unsigned p ){ return volumeEnvelope_[p];  }
    unsigned        getnVolumePoints ()             { return nVolumePoints_;      }
    unsigned        getVolumeSustain ()             { return volumeSustain_;      }
    unsigned        getVolumeLoopStart ()           { return volumeLoopStart_;    }
    unsigned        getVolumeLoopEnd ()             { return volumeLoopEnd_;      }
    unsigned        getVolumeType ()                { return volumeType_;         }
    unsigned        getVolumeFadeOut ()             { return volumeFadeOut_;      }
    EnvelopePoint   getPanningEnvelope (unsigned p) { return panningEnvelope_[p]; }
    unsigned        getnPanningPoints ()            { return nPanningPoints_;     }
    unsigned        getPanningSustain ()            { return panningSustain_;     }
    unsigned        getPanningLoopStart ()          { return panningLoopStart_;   }
    unsigned        getPanningLoopEnd ()            { return panningLoopEnd_;     }
    unsigned        getPanningType ()               { return panningType_;        }
    unsigned        getVibratoType ()               { return vibratoType_;        }
    unsigned        getVibratoSweep ()              { return vibratoSweep_;       }
    unsigned        getVibratoDepth ()              { return vibratoDepth_;       }
    unsigned        getVibratoRate ()               { return vibratoRate_;        }

private:
    std::string     name_;
    unsigned        nSamples_;
    unsigned        sampleForNote_[MAXIMUM_NOTES];
    //NoteSampleMap   sampleForNote_[MAXIMUM_NOTES];
    EnvelopePoint   volumeEnvelope_[12];
    unsigned        nVolumePoints_;
    unsigned        volumeSustain_;
    unsigned        volumeLoopStart_;
    unsigned        volumeLoopEnd_;
    unsigned        volumeType_;
    unsigned        volumeFadeOut_;
    EnvelopePoint   panningEnvelope_[12];
    unsigned        nPanningPoints_;
    unsigned        panningSustain_;
    unsigned        panningLoopStart_;
    unsigned        panningLoopEnd_;
    unsigned        panningType_;
    unsigned        vibratoType_;
    unsigned        vibratoSweep_;
    unsigned        vibratoDepth_;
    unsigned        vibratoRate_;
};

class Module {
public:
    Module();
    Module( std::string &fileName ) : Module() 
    { 
        loadFile( fileName ); 
    }
    ~Module();
    std::string     getFileName ()               { return fileName_;             }
    void            setFileName( std::string &fileName ) { fileName_ = fileName; }
    int             loadFile ();
    int             loadFile( std::string &fileName )
                    { setFileName( fileName ); return loadFile(); }
    bool            isLoaded ()                 { return isLoaded_;             }
    bool            useLinearFrequencies ()     { return useLinearFrequencies_; }
    bool            isCustomRepeat ()           { return isCustomRepeat_;       }
    unsigned        getTrackerType ()           { return trackerType_;          }
    unsigned        getMinPeriod ()             { return minPeriod_;            }
    unsigned        getMaxPeriod ()             { return maxPeriod_;            }
    unsigned        getPanningStyle()           { return panningStyle_;         }
    unsigned        getnChannels ()             { return nChannels_;            }
    unsigned        getnInstruments ()          { return nInstruments_;         }
    unsigned        getnSamples ()              { return nSamples_;             }
    unsigned        getnPatterns ()             { return nPatterns_;            }
    unsigned        getDefaultTempo ()          { return defaultTempo_;         }
    unsigned        getDefaultBpm ()            { return defaultBpm_;           }
    unsigned        getSongLength ()            { return songLength_;           }
    unsigned        getSongRestartPosition ()   { return songRestartPosition_;  }
    std::string     getSongTitle ()             { return songTitle_;            }

    unsigned        getDefaultPanPosition( unsigned i ) 
    { 
        assert( i < nChannels_ );
        return defaultPanPositions_[i];
    }
    unsigned        getPatternTable( unsigned i )
    { 
        assert( i < MAX_PATTERNS );
        return patternTable_[i];   
    }
    Sample          *getSample( unsigned sample )
    {
        assert( sample <= 64 /*MAX_SAMPLES*/ );
        return samples_[sample] ? samples_[sample] : &emptySample_;
    }
    Instrument      *getInstrument( unsigned instrument )  
    { 
        assert( instrument <= MAX_INSTRUMENTS );
        return instruments_[instrument] ? instruments_[instrument] : &emptyInstrument_;
    }
    Pattern         *getPattern( unsigned pattern ) 
    { 
        assert( pattern < MAX_PATTERN );
        return patterns_[pattern] ? patterns_[pattern] : &emptyPattern_;
    }

private:
    std::string     fileName_;
    std::string     songTitle_;
    std::string     trackerTag_;
    unsigned        trackerType_;
    bool            isLoaded_;
    bool            useLinearFrequencies_;
    bool            isCustomRepeat_;
    unsigned        minPeriod_;
    unsigned        maxPeriod_;
    unsigned        panningStyle_;
    unsigned        nChannels_;
    unsigned        nInstruments_;
    unsigned        nSamples_;
    unsigned        nPatterns_;
    unsigned        defaultTempo_;
    unsigned        defaultBpm_;
    unsigned        songLength_;
    unsigned        songRestartPosition_;
    unsigned char   defaultPanPositions_[PLAYER_MAX_CHANNELS];
    unsigned        patternTable_[MAX_PATTERNS];
    Sample          *samples_[MAX_SAMPLES];
    Instrument      *instruments_[MAX_INSTRUMENTS];
    Pattern         *patterns_[MAX_PATTERNS];
    Pattern         emptyPattern_;
    Sample          emptySample_;
    Instrument      emptyInstrument_;

    int             loadModFile ();
    int             loadS3mFile ();
    int             loadXmFile ();

    int             loadItInstrument( VirtualFile& itFile,int instrumentNr,unsigned createdWTV );
    int             loadItSample( VirtualFile& itFile,int sampleNr,bool convertToInstrument,bool isIt215Compression );
    int             loadItPattern( VirtualFile & itFile,int patternNr );
    int             loadItFile ();
};

#endif // MODULE_H
