#pragma once

#include <climits>
#if CHAR_BIT != 8 
This code requires a byte to be 8 bits wide
#endif

#pragma comment (lib,"winmm.lib") 

#define NOMINMAX
#include <windows.h>
#include <mmreg.h>

// the following are only needed if you use the wave_format_extensible:
//#include <ks.h>
//#include <Ksmedia.h>

#include <cstdio>
#include <conio.h>
#include <mmsystem.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <cmath>
#include <vector>
#include <iostream> // for debugging
#include <iomanip>  // for debugging

#include "time.h"
#include "Module.h"
#include "Sample.h"
#include "Pattern.h"
#include "VirtualFile.h" // debug

#define debug_mixer

#define LINEAR_INTERPOLATION  // this constant is not used

const int MIXRATE              = 48000;          // in Hz
const int BLOCK_SIZE           = 0x4000;         // normally 0x4000
const int BLOCK_COUNT          = 2;              // at least 2, or 4 (?) for float mixing
#define   BITS_PER_SAMPLE      32 //  (sizeof( MixBufferType ) / 8) // can't use constexpr here
const int SAMPLES_PER_BLOCK    = BLOCK_SIZE /
                    (BITS_PER_SAMPLE / 8);       // 32 bit = 4 bytes / sample


typedef std::int32_t MixBufferType;      // for internal mixing, 32 bit will do for now

#if BITS_PER_SAMPLE == 32
//typedef std::int32_t DestBufferType;     // DestBufferType must be int for 32 bit mixing
typedef float DestBufferType;     // DestBufferType must be int or float for 32 bit mixing
#elif BITS_PER_SAMPLE == 16
typedef SHORT DestBufferType;   // DestBufferType must be SHORT for 16 bit mixing
#else
Error! output buffer must be 16 bit or 32 bit!
#endif

// these are not used for now:
#define FILTER_SQUARE           0
#define FILTER_LINEAR           1
#define FILTER_CUBIC            2
#define FILTER                  FILTER_CUBIC   

#define MIXER_MAX_CHANNELS      256     // maximum of total mixed channels 
#define VOLUME_RAMP_SPEED       44      // 1 ms at mixrate of 44kHz, max. = 127
#define VOLUME_RAMPING_UP       1
#define VOLUME_RAMPING_DOWN     -1


class Channel {
public:
    Instrument*     pInstrument;
    Sample*         pSample;
    Note            oldNote;  
    Note            newNote; // oldNote + NewNote = 12 bytes

    bool            isMuted;
    unsigned char   volume;
    unsigned char   panning;
    bool            keyIsReleased;

    unsigned short  iVolumeEnvelope;
    unsigned short  iPanningEnvelope;

    unsigned short  iPitchFltrEnvelope;
    unsigned short  period;

    unsigned short  targetPeriod;
    unsigned char   instrumentNr;
    unsigned char   lastNote;           

    unsigned        sampleOffset;

    // effect memory & index counters:
    bool            patternIsLooping;
    unsigned char   patternLoopStart;
    unsigned char   patternLoopCounter;
    unsigned char   lastArpeggio;

    unsigned char   arpeggioCount;
    unsigned char   arpeggioNote1;
    unsigned char   arpeggioNote2;
    unsigned char   vibratoWaveForm;

    unsigned char   tremoloWaveForm;
    unsigned char   panbrelloWaveForm;
    signed char     vibratoCount;
    signed char     tremoloCount;

    signed char     panbrelloCount;
    unsigned char   retrigCount;
    unsigned char   delayCount;
    unsigned char   lastPortamentoUp;

    unsigned short  portaDestPeriod;
    unsigned char   lastPortamentoDown;
    unsigned char   lastTonePortamento;

    unsigned char   lastVibrato;
    unsigned char   lastTremolo;
    unsigned char   lastVolumeSlide;
    unsigned char   lastFinePortamentoUp;

    unsigned char   lastFinePortamentoDown;
    unsigned char   lastFineVolumeSlideUp;
    unsigned char   lastFineVolumeSlideDown;
    unsigned char   lastGlobalVolumeSlide;

    unsigned char   lastPanningSlide;
    unsigned char   lastBpmSLide;
    unsigned char   lastSampleOffset;
    unsigned char   lastMultiNoteRetrig;

    unsigned char   lastExtendedEffect;
    unsigned char   lastTremor;
    unsigned char   lastExtraFinePortamentoUp;
    unsigned char   lastExtraFinePortamentoDown;

    unsigned short  mixerChannelNr;
    unsigned short  alignDummy;
public:
    void            init()
    {
        memset( this,0,sizeof( Channel ) );
    }
    Channel()
    {
        init();
    }
};

class MixerChannel {
public:
    unsigned        age;
    Sample*         sample;

    unsigned char   masterChannel;     // 0..63
    bool            isActive;          // if not, mixer skips it
    bool            isMaster;          // false if it is a virtual channel
    unsigned        sampleOffset;      // samples can be up to 4 GB in theory
    unsigned        sampleOffsetFrac;

    unsigned        sampleIncrement;
    unsigned        leftVolume;        // must be 32 bit!
    unsigned        rightVolume;       // must be 32 bit!

    bool            isPlayingBackwards;
    signed char     volumeRampDelta;   // if -1 -> ramping down 
    unsigned char   volumeRampCounter; // no ramp if zero
    signed char     volumeRampStart;   // VOLUME_RAMP_SPEED if ramping down, 0 if ramping up
public:
    void            init()
    {
        memset( this,0,sizeof( MixerChannel ) );
    }
    MixerChannel()
    {
        init();
    }
};

class Mixer {
public: // debug
    bool            isInitialised;
    bool            st300FastVolSlides_;
    bool            st3StyleEffectMemory_;
    bool            itStyleEffects_;
    bool            ft2StyleEffects_;
    bool            pt35StyleEffects_;
    Module*         module;
    std::unique_ptr < MixBufferType[] > mixBuffer;
    Channel         channels[PLAYER_MAX_CHANNELS];
    MixerChannel    mixerChannels[MIXER_MAX_CHANNELS]; // not a pointer, too slow
    unsigned        nrActiveMixChannels;
    unsigned        mixIndex;
    unsigned        mixCount;
    unsigned        callBpm;
    unsigned        gain;
    unsigned        saturation;
    unsigned        nrChannels;
    unsigned        globalPanning_;
    unsigned        globalVolume_;
    int             balance_;     // range: -100...0...+100
    unsigned        tempo;
    unsigned        bpm;
    unsigned        tick;
    unsigned        patternDelay_;
    unsigned        patternRow;
    unsigned        iPatternTable;
    unsigned        patternLoopStartRow_;
    bool            patternLoopFlag_;

    Pattern* pattern;
    const Note* iNote;

    static CRITICAL_SECTION     waveCriticalSection;
    static WAVEHDR*             waveBlocks;
    static volatile int         waveFreeBlockCount;
    static int                  waveCurrentBlock;

    HWAVEOUT        hWaveOut;

    WAVEFORMATEXTENSIBLE    waveFormatExtensible;
    WAVEFORMATEX            waveFormatEx;

public: // debug
    DestBufferType* waveBuffers[BLOCK_COUNT];
private: // debug

    unsigned        noteToPeriod( unsigned note,int finetune );
    unsigned        periodToFrequency( unsigned period );
    int             setMixerVolume( unsigned fromChannel );
    int             setFrequency( unsigned fromChannel,unsigned frequency );
    int             playSample( unsigned fromChannel,Sample* sample,unsigned offset,bool direction );
    int             stopChannelPrimary( unsigned fromChannel );
    int             setVolumes();
    int             updateNotes();
    int             updateEffects();
    int             updateImmediateEffects();
    int             updateEnvelopes();
    int             updateBpm();
    int             setBpm()
    {
        callBpm = (MIXRATE * 5) / (bpm << 1);
        return 0;
    }
public:
    Mixer();
    ~Mixer();
    static void CALLBACK waveOutProc(
        HWAVEOUT hWaveOut,
        UINT uMsg,
        DWORD *dwInstance,
        DWORD dwParam1,
        DWORD dwParam2 );
    void            startReplay();
    int             stopReplay();
    void            updateWaveBuffers();
    int             initialize( Module* m );

    int             doMixBuffer( DestBufferType* buffer );
    int             doMixSixteenbitStereo( unsigned nSamples );
    int             setGlobalPanning( unsigned panning ); // range: 0..255 (extreme stereo... extreme reversed stereo)
    int             setGlobalBalance( int balance );      // range: -100...0...+100
    void            resetSong();
};
