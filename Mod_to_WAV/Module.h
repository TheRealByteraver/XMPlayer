#ifndef MODULE_H
#define MODULE_H

#include "assert.h"

// More general constants: 
#define PAL_CALC                            7093789.2   // these values are
#define NTSC_CALC                           7159090.5   // not used 
#define NTSC_C4_SPEED                       8363.42
#define PAL_C4_SPEED                        8287.14
#define PANNING_STYLE_MOD                   1   // LRRL etc
#define PANNING_STYLE_XM                    2   // ALL CENTER
#define PANNING_STYLE_S3M                   3   // LRLR etc
#define MAX_VOLUME                          64
#define MAX_SAMPLENAME_LENGTH               22
#define MAX_INSTRUMENTNAME_LENGTH           (MAX_SAMPLENAME_LENGTH + 2)
#define MAX_PATTERNS                        256
#define MAX_INSTRUMENTS                     256
#define MAX_SAMPLES                         16   // max.samples / instrument
#define SIGNED_EIGHT_BIT_SAMPLE             1
#define SIGNED_SIXTEEN_BIT_SAMPLE           2
#define INTERPOLATION_SPACER                2
#define MAX_EFFECT_COLUMS                   2
#define MAXIMUM_NOTES                       (11 * 12)
#define PLAYER_MAX_CHANNELS                 32
#define PANNING_FULL_LEFT                   0
#define PANNING_CENTER                      128
#define PANNING_FULL_RIGHT                  255
#define PANNING_MAX_STEPS                   256  // must be power of two
#define PANNING_SHIFT                       8    // divide by 256 <=> SHR 8
#define FORWARD                             false
#define BACKWARD                            true

// differentiate trackers, fro S3M compatibility mainly
#define TRACKER_PROTRACKER                  1
#define TRACKER_ST300                       2
#define TRACKER_ST321                       3
#define TRACKER_FT2                         4

// effect nrs:
#define ARPEGGIO                            0x0
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
#define FUNK_REPEAT                         0xF // XM effect EF, end of extended effects
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

// internal remapped effects for the player
#define SET_BPM                             0x24  // after effect "Z" for XM safety
#define FINE_VIBRATO                        0x26  // S3M fine vibrato
//#define S3M_VOLUME_SLIDE                    0x27  // S3M volume slide
#define KEY_OFF                             (12 * 11 + 1) // 12 octaves

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
    unsigned        period;
    Effect          effects[MAX_EFFECT_COLUMS];
};

class Pattern {
    unsigned        nChannels_;
    unsigned        nRows_;
    Note            *data_;
public:
                    Pattern ()                  
                    { memset(this, 0, sizeof(Pattern)); }
                    ~Pattern ()                 { delete data_;             }
    unsigned        getnRows ()                 { return nRows_;            }
    Note            getNote( unsigned n )       { return data_[n];          }
    Note             *getRow (unsigned row)
    { return (data_ ? (data_ + nChannels_ * row) : nullptr); }
    void            Initialise(unsigned nChannels, unsigned nRows, Note *data)
                    { 
                        nChannels_ = nChannels; 
                        nRows_ = nRows; 
                        data_ = data; 
                        memset( data,0,nChannels * nRows * sizeof( Note ));
                    }
};

class SampleHeader {
public:
                    SampleHeader () 
                    { 
                        memset(this, 0, sizeof(SampleHeader)); 
                        //c4Speed = (unsigned)NTSC_C4_SPEED;
                    }
    //char            *name;
    std::string     name;
    unsigned        length;
    unsigned        repeatOffset;
    unsigned        repeatLength;
    bool            isRepeatSample;
    bool            isPingpongSample;
    bool            isUsed;               // if the sample is used in the song
    int             volume;
    int             relativeNote;
    unsigned        panning;
    int             finetune;
    //unsigned        c4Speed;
    int             dataType;           // 8 or 16 bit, compressed, etc
    SHORT           *data;              // only 16 bit samples allowed                    
};

class Sample {
public:
                    Sample () 
                    { 
                        memset(this, 0, sizeof(Sample)); 
                        //c4Speed_ = (unsigned)NTSC_C4_SPEED;
                    }
                    ~Sample ();
    bool            load (const SampleHeader &sampleHeader);
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
    //unsigned        getC4Speed()        { return c4Speed_;          }
    SHORT           *getData ()         { return data_ + INTERPOLATION_SPACER; }
private:
    std::string     name_;
    //char            *name_;
    unsigned        length_;
    unsigned        repeatOffset_;
    unsigned        repeatEnd_;
    unsigned        repeatLength_;
    bool            isRepeatSample_;
    bool            isPingpongSample_;
    bool            isUsed_;              // if the sample is used in the song
    int             volume_;
    int             relativeNote_;
    unsigned        panning_;
    int             finetune_;
    //unsigned        c4Speed_;
    SHORT           *data_;             // only 16 bit samples allowed
};

class EnvelopePoint {
public:
    unsigned        x;
    unsigned        y;
};

class InstrumentHeader {
public:
    //char            *name;
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
    Sample          *samples[MAX_SAMPLES];
    InstrumentHeader() { memset(this, 0, sizeof(InstrumentHeader)); }
};

class Instrument {
public:
                    Instrument ()
                    { memset(this, 0, sizeof(Instrument)); }    // still ok w/ std::string?
                    ~Instrument ();
    void            load(const InstrumentHeader &instrumentHeader);
    //const char      *getName ()                     { return name_;  }
    std::string     getName() { return name_; }
    //char            *getName (char *name);
    unsigned        getnSamples ()                  { return nSamples_;           }
    unsigned        getSampleForNote(unsigned n)    { return sampleForNote_[n];   }
    EnvelopePoint   getVolumeEnvelope (unsigned p)  { return volumeEnvelope_[p];  }
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
    Sample          *getSample (unsigned sample)    
                    { return ((sample < MAX_SAMPLES) ? samples_[sample] : 0);     }
private:
    std::string     name_;
    //char            *name_;
    unsigned        nSamples_;
    unsigned        sampleForNote_[MAXIMUM_NOTES];
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
    Sample          *samples_[MAX_SAMPLES];
};

class Module {
public:
                    Module ()               { memset(this, 0, sizeof(Module)); } 
                    Module( std::string &fileName ) : Module() { loadFile( fileName ); }
                    ~Module();
                    //Module (const char *fileName);
//    char            *getFileName (const char *fileName);
    //void            setFileName (const char *fileName);
    //int             loadFile (char *fileName)   
    std::string     getFileName()               { return fileName_;             }
    void            setFileName( std::string &fileName ) { fileName_ = fileName; }
    int             loadFile ();
    int             loadFile( std::string &fileName )
                    { setFileName( fileName ); return loadFile(); }
    bool            isLoaded ()                 { return isLoaded_;             }
    bool            useLinearFrequencies ()     { return useLinearFrequencies_; }
    bool            isCustomRepeat()            { return isCustomRepeat_;       }
    unsigned        getPanningStyle()           { return panningStyle_;         }
    unsigned        getnChannels ()             { return nChannels_;            }
    unsigned        getnInstruments ()          { return nInstruments_;         }
    unsigned        getnSamples ()              { return nSamples_;             }
    unsigned        getnPatterns ()             { return nPatterns_;            }
    unsigned        getDefaultTempo ()          { return defaultTempo_;         }
    unsigned        getDefaultBpm ()            { return defaultBpm_;           }
    unsigned        getSongLength ()            { return songLength_;           }
    unsigned        getSongRestartPosition ()   { return songRestartPosition_;  }
    std::string     getSongTitle()              { return songTitle_;            }

    unsigned        getDefaultPanPosition( unsigned i ) 
    { 
        assert( i < nChannels_ );
        return defaultPanPositions_[i];
    }
    unsigned        getPatternTable (unsigned i)
                    { return ((i < MAX_PATTERNS) ? patternTable_[i] : 0);   }
    Instrument      *getInstrument(unsigned instrument) 
       { return ((instrument < MAX_INSTRUMENTS) ? instruments_[instrument] : 0); }
    Pattern         *getPattern(unsigned pattern) 
       { return ((pattern < MAX_PATTERNS) ? patterns_[pattern] : 0); }

private:
    std::string     fileName_;
    std::string     songTitle_;
    std::string     trackerTag_;
    unsigned        trackerType_;
    bool            isLoaded_;
    bool            useLinearFrequencies_;
    bool            isCustomRepeat_;
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
    Instrument      *instruments_[MAX_INSTRUMENTS];
    Pattern         *patterns_[MAX_PATTERNS];

    int             loadModFile ();
    int             loadS3mFile ();
    int             loadXmFile ();
};

#endif // MODULE_H
