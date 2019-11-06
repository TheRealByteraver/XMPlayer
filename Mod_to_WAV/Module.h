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
#include "instrument.h"
#include "virtualfile.h"

// forward declarations for linker:
class Sample;
class Instrument;

class Module {
public:
    Module();
    Module( std::string& fileName ) : Module() { loadFile( fileName ); }

    std::string     getFileName()         const { return fileName_;             }
    void            setFileName( std::string& fileName ) { fileName_ = fileName; }
    int             loadFile( std::string &fileName )
                    { setFileName( fileName ); return loadFile(); }
    bool            isLoaded()            const { return isLoaded_;             }
    bool            getVerboseMode()      const { return showDebugInfo_;        }
    void            enableDebugMode()           { showDebugInfo_ = true;        }
    void            disableDebugMode()          { showDebugInfo_ = false;       }
    bool            useLinearFrequencies()const { return useLinearFrequencies_; }
    bool            isCustomRepeat()      const { return isCustomRepeat_;       }
    unsigned        getTrackerType()      const { return trackerType_;          }
    unsigned        getMinPeriod()        const { return minPeriod_;            }
    unsigned        getMaxPeriod()        const { return maxPeriod_;            }
    unsigned        getPanningStyle()     const { return panningStyle_;         }
    unsigned        getnChannels()        const { return nChannels_;            }
    unsigned        getnInstruments()     const { return nInstruments_;         }
    unsigned        getnSamples()         const { return nSamples_;             }
    unsigned        getnPatterns()        const { return nPatterns_;            }
    unsigned        getDefaultTempo()     const { return defaultTempo_;         }
    unsigned        getDefaultBpm()       const { return defaultBpm_;           }
    unsigned        getSongLength()       const { return songLength_;           }
    unsigned        getSongRestartPosition()const { return songRestartPosition_; }
    std::string     getSongTitle()        const { return songTitle_;            }

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

    Sample&         getSample( unsigned sample )
    {
        assert( sample <= MAX_SAMPLES ); // !!!!
        return (samples_[sample] ? *(samples_[sample]) : *(samples_[0]));
    }
    Instrument&     getInstrument( unsigned instrument )
    { 
        assert( instrument <= MAX_INSTRUMENTS );
        return (instruments_[instrument] ? *(instruments_[instrument]) : *(instruments_[0]));
    }
    Pattern&        getPattern( unsigned pattern )
    { 
        assert( pattern < MAX_PATTERN );
        return (patterns_[pattern] ? *(patterns_[pattern]) : emptyPattern_);
    }

private:
    int             loadFile();

private:
    std::string     fileName_;
    std::string     songTitle_;
    std::string     trackerTag_;
    unsigned        trackerType_ = TRACKER_IT;
    bool            showDebugInfo_ = false;
    bool            isLoaded_ = false;
    bool            useLinearFrequencies_ = true;
    bool            isCustomRepeat_ = false;
    unsigned        minPeriod_ = 14;
    unsigned        maxPeriod_ = 27392;
    unsigned        panningStyle_ = PANNING_STYLE_IT;
    unsigned        nChannels_ = 0;
    unsigned        nInstruments_ = 1;
    unsigned        nSamples_ = 1;
    unsigned        nPatterns_ = 1;
    unsigned        defaultTempo_ = 6;
    unsigned        defaultBpm_ = 125;
    unsigned        songLength_ = 1;
    unsigned        songRestartPosition_ = 0;
    unsigned char   defaultPanPositions_[PLAYER_MAX_CHANNELS];
    unsigned        patternTable_[MAX_PATTERNS];


    std::unique_ptr < Sample >      samples_[MAX_SAMPLES];
    std::unique_ptr < Instrument >  instruments_[MAX_INSTRUMENTS];
    std::unique_ptr < Pattern >     patterns_[MAX_PATTERNS];

    Pattern         emptyPattern_ = 
        Pattern( 
            PLAYER_MAX_CHANNELS,
            64,
            std::vector<Note>( PLAYER_MAX_CHANNELS * 64 ) 
        );
    Instrument      emptyInstrument_ = Instrument( InstrumentHeader() );

    int             loadItFile();
    int             loadXmFile();
    int             loadS3mFile();
    int             loadModFile();

    int             loadItInstrument( 
        VirtualFile& itFile,
        int instrumentNr,
        unsigned createdWTV 
    );
    int             loadItSample( 
        VirtualFile& itFile,
        int sampleNr,
        bool convertToInstrument,
        bool isIt215Compression 
    );
    int             loadItPattern( VirtualFile & itFile,int patternNr );

    int             loadXmInstrument( VirtualFile& xmFile,int instrumentNr );
    int             loadXmSample( VirtualFile & xmFile,int sampleNr,SampleHeader& smpHdr );
    int             loadXmPattern( VirtualFile & xmFile,int patternNr );

    int             loadModPattern( VirtualFile & modFile,int patternNr );

    // for debugging purposes:
    void            playSampleNr( int sampleNr ); 
};
