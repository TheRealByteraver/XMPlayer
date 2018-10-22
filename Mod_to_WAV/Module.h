#ifndef MODULE_H
#define MODULE_H

// More general constants: 
#define PAL_CALC                            7093789.2   // these values are
#define NTSC_CALC                           7159090.5   // not used 
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
#define INTERPOLATION_SPACER                1
#define MAX_EFFECT_COLUMS                   2
#define MAXIMUM_NOTES                       (8 * 12)
#define PANNING_FULL_LEFT                   0
#define PANNING_CENTER                      128
#define PANNING_FULL_RIGHT                  255
#define PANNING_MAX_STEPS                   256  // must be power of two
#define PANNING_SHIFT                       8    // divide by 256 <=> SHR 8
#define FORWARD                             false
#define BACKWARD                            true

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
#define SET_FILTER                          0x0
#define FINE_PORTAMENTO_UP                  0x1 
#define FINE_PORTAMENTO_DOWN                0x2
#define SET_GLISSANDO_CONTROL               0x3
#define SET_VIBRATO_CONTROL                 0x4
#define SET_FINETUNE                        0x5
#define SET_PATTERN_LOOP                    0x6
#define SET_TREMOLO_CONTROL                 0x7
#define NOTE_RETRIG                         0x9
#define FINE_VOLUME_SLIDE_UP                0xA
#define FINE_VOLUME_SLIDE_DOWN              0xB
#define NOTE_CUT                            0xC
#define NOTE_DELAY                          0xD
#define PATTERN_DELAY                       0xE // end of extended effects
#define FUNK_REPEAT                         0xF 
#define SET_TEMPO_BPM                       0xF
#define SET_GLOBAL_VOLUME                   0x10
#define GLOBAL_VOLUME_SLIDE                 0x11
#define SET_ENVELOPE_POSITION               0x15
#define PANNING_SLIDE                       0x19
#define MULTI_NOTE_RETRIG                   0x1B
#define TREMOR                              0x1D
#define EXTRA_FINE_PORTAMENTO               0x21
#define EXTRA_FINE_PORTAMENTO_UP            0x1   // 0x21 in XM file spec
#define EXTRA_FINE_PORTAMENTO_DOWN          0x2   // X1 & X2
#define KEY_OFF                             97


const unsigned periods[MAXIMUM_NOTES] = {
   4*1712,4*1616,4*1524,4*1440,4*1356,4*1280,4*1208,4*1140,4*1076,4*1016, 4*960, 4*906,
   2*1712,2*1616,2*1524,2*1440,2*1356,2*1280,2*1208,2*1140,2*1076,2*1016, 2*960, 2*906,
     1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906,
      856,   808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453,
      428,   404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226,
      214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
      107,   101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56,
       53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28};


const unsigned amigaPeriodTable[] = {
    907, 900, 894, 887, 881, 875, 868, 862, 856, 850, 844, 838, 
    832, 826, 820, 814, 808, 802, 796, 791, 785, 779, 774, 768, 
    762, 757, 752, 746, 741, 736, 730, 725, 720, 715, 709, 704, 
    699, 694, 689, 684, 678, 675, 670, 665, 660, 655, 651, 646,
    640, 636, 632, 628, 623, 619, 614, 610, 604, 601, 597, 592, 
    588, 584, 580, 575, 570, 567, 563, 559, 555, 551, 547, 543, 
    538, 535, 532, 528, 524, 520, 516, 513, 508, 505, 502, 498, 
    494, 491, 487, 484, 480, 477, 474, 470, 467, 463, 460, 457};

// =============================================================================
// These structures represent exactly the layout of a MOD file:
// the pragma directive prevents the compiler from enlarging the struct with 
// dummy bytes for performance purposes

typedef unsigned __int16 AMIGAWORD;
typedef signed   __int16 SHORT;

// These type definitions are needed by the MOD loader & replay routines:
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
    Note            getNote (unsigned n)        { return data_[n];          }
    Note             *getRow (unsigned row)
    { return (data_ ? (data_ + nChannels_ * row) : 0); }
    void            Initialise(unsigned nChannels, unsigned nRows, Note *data)
                    { nChannels_ = nChannels; nRows_ = nRows; data_ = data; }
};

class SampleHeader {
public:
                    SampleHeader () { memset(this, 0, sizeof(SampleHeader)); }
    char            *name;
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
    int             dataType;           // 8 or 16 bit, compressed, etc
    SHORT           *data;              // only 16 bit samples allowed                    
};

class Sample {
public:
                    Sample () { memset(this, 0, sizeof(Sample)); }
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
    SHORT           *getData ()         { return data_ + INTERPOLATION_SPACER; }
private:
    char            *name_;
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
    SHORT           *data_;             // only 16 bit samples allowed
};

class EnvelopePoint {
public:
    unsigned        x;
    unsigned        y;
};


class InstrumentHeader {
public:
    char            *name;
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
                    { memset(this, 0, sizeof(Instrument)); }
                    ~Instrument ();
    void            load(const InstrumentHeader &instrumentHeader);
    const char      *getName ()                     { return name_;  }
    char            *getName (char *name);
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
    char            *name_;
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
                    Module (const char *fileName);
                    ~Module();
//    char            *getFileName (const char *fileName);
    void            setFileName (const char *fileName);
    int             loadFile ();
    int             loadFile (char *fileName)   
                    { setFileName(fileName); return loadFile();                 }
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
    unsigned        getPatternTable (unsigned i)
                    { return ((i < MAX_PATTERNS) ? patternTable_[i] : 0);   }
    Instrument      *getInstrument(unsigned instrument) 
       { return ((instrument < MAX_INSTRUMENTS) ? instruments_[instrument] : 0); }
    Pattern         *getPattern(unsigned pattern) 
       { return ((pattern < MAX_PATTERNS) ? patterns_[pattern] : 0); }

private:
    char            *fileName_;
    char            *songTitle_;
    char            *trackerTag_;
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
    unsigned        patternTable_[MAX_PATTERNS];
    Instrument      *instruments_[MAX_INSTRUMENTS];
    Pattern         *patterns_[MAX_PATTERNS];

    int             loadModFile ();
    int             loadXmFile ();
};

#endif // MODULE_H
