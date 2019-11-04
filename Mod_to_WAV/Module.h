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

#pragma once

#include <bitset> // for debugging
#include <memory>
#include <cassert>
#include <vector>
#include <iterator>

#include "constants.h"
#include "pattern.h"
#include "sample.h"
#include "virtualfile.h"

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
        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
            sampleForNote[i].note = i;
            sampleForNote[i].sampleNr = 0;
        }
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
    NoteSampleMap   sampleForNote[MAXIMUM_NOTES];
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
    Instrument()
    {
        name_.clear();
        nSamples_ = 1;

        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
            sampleForNote_[i].note = i;
            sampleForNote_[i].sampleNr = 0;
        }

        for ( int i = 0; i < 12; i++ ) { // only 12 envelope points?
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
    unsigned        getNoteForNote( unsigned n )
    { 
        assert ( n < MAXIMUM_NOTES );  // has no effect
        if ( n >= MAXIMUM_NOTES ) return 0;
        return sampleForNote_[n].note;
    }
    unsigned        getSampleForNote( unsigned n )
    {
        assert( n < MAXIMUM_NOTES );  // has no effect
        if ( n >= MAXIMUM_NOTES ) return 0;
        return sampleForNote_[n].sampleNr;
    }

    EnvelopePoint   getVolumeEnvelope( unsigned p ) { return volumeEnvelope_[p];  }
    unsigned        getnVolumePoints()              { return nVolumePoints_;      }
    unsigned        getVolumeSustain()              { return volumeSustain_;      }
    unsigned        getVolumeLoopStart()            { return volumeLoopStart_;    }
    unsigned        getVolumeLoopEnd()              { return volumeLoopEnd_;      }
    unsigned        getVolumeType()                 { return volumeType_;         }
    unsigned        getVolumeFadeOut()              { return volumeFadeOut_;      }
    EnvelopePoint   getPanningEnvelope( unsigned p ){ return panningEnvelope_[p]; }
    unsigned        getnPanningPoints()             { return nPanningPoints_;     }
    unsigned        getPanningSustain()             { return panningSustain_;     }
    unsigned        getPanningLoopStart()           { return panningLoopStart_;   }
    unsigned        getPanningLoopEnd()             { return panningLoopEnd_;     }
    unsigned        getPanningType()                { return panningType_;        }
    unsigned        getVibratoType()                { return vibratoType_;        }
    unsigned        getVibratoSweep()               { return vibratoSweep_;       }
    unsigned        getVibratoDepth()               { return vibratoDepth_;       }
    unsigned        getVibratoRate()                { return vibratoRate_;        }

private:
    std::string     name_;
    unsigned        nSamples_;
    //unsigned        sampleForNote_[MAXIMUM_NOTES];
    NoteSampleMap   sampleForNote_[MAXIMUM_NOTES];
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

// forward declarations for linker:
class Sample;

class Module {
public:
    Module();
    Module( std::string &fileName ) : Module() 
    { 
        loadFile( fileName ); 
    }
    ~Module();
    std::string     getFileName()                { return fileName_;             }
    void            setFileName( std::string &fileName ) { fileName_ = fileName; }
    int             loadFile();
    int             loadFile( std::string &fileName )
                    { setFileName( fileName ); return loadFile(); }
    bool            isLoaded()                  { return isLoaded_;             }
    bool            getVerboseMode()            { return showDebugInfo_;        }
    void            enableDebugMode()           { showDebugInfo_ = true;        }
    void            disableDebugMode()          { showDebugInfo_ = false;       }
    bool            useLinearFrequencies()      { return useLinearFrequencies_; }
    bool            isCustomRepeat()            { return isCustomRepeat_;       }
    unsigned        getTrackerType()            { return trackerType_;          }
    unsigned        getMinPeriod()              { return minPeriod_;            }
    unsigned        getMaxPeriod()              { return maxPeriod_;            }
    unsigned        getPanningStyle()           { return panningStyle_;         }
    unsigned        getnChannels()              { return nChannels_;            }
    unsigned        getnInstruments()           { return nInstruments_;         }
    unsigned        getnSamples()               { return nSamples_;             }
    unsigned        getnPatterns()              { return nPatterns_;            }
    unsigned        getDefaultTempo()           { return defaultTempo_;         }
    unsigned        getDefaultBpm()             { return defaultBpm_;           }
    unsigned        getSongLength()             { return songLength_;           }
    unsigned        getSongRestartPosition()    { return songRestartPosition_;  }
    std::string     getSongTitle()              { return songTitle_;            }

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
        assert( sample <= 99 /*MAX_SAMPLES*/ ); // !!!!
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
    bool            showDebugInfo_;
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
    Pattern         emptyPattern_ = 
        Pattern( 
            PLAYER_MAX_CHANNELS,
            64,
            std::vector<Note>( PLAYER_MAX_CHANNELS * 64 ) 
        );
    Sample          emptySample_;
    Instrument      emptyInstrument_;

    int             loadItFile();
    int             loadXmFile();
    int             loadS3mFile();
    int             loadModFile();

    int             loadItInstrument( VirtualFile& itFile,int instrumentNr,unsigned createdWTV );
    int             loadItSample( VirtualFile& itFile,int sampleNr,bool convertToInstrument,bool isIt215Compression );
    int             loadItPattern( VirtualFile & itFile,int patternNr );

    int             loadXmInstrument( VirtualFile& xmFile,int instrumentNr );
    int             loadXmSample( VirtualFile & xmFile,int sampleNr,SampleHeader& sampleHeader );
    int             loadXmPattern( VirtualFile & xmFile,int patternNr );

    int             loadModPattern( VirtualFile & modFile,int patternNr );

    void            playSampleNr( int sampleNr ); // for debugging purposes
};

//#endif // MODULE_H
