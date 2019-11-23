#pragma once

#pragma comment (lib,"winmm.lib") 

#define NOMINMAX
#include <windows.h>

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

constexpr auto MIXRATE              = 48000;          // in Hz
constexpr auto BLOCK_SIZE           = 0x4000;         // normally 0x4000
constexpr auto BLOCK_COUNT          = 20;             // should be at least 4
#define        BITS_PER_SAMPLE        32              // can't use constexpr here
constexpr auto SAMPLES_PER_BLOCK    = BLOCK_SIZE / 
                    (BITS_PER_SAMPLE >> 3);           // 32 bit = 4 bytes / sample

typedef int MixBufferType;      // for internal mixing, 32 bit will do for now

#if BITS_PER_SAMPLE == 32
typedef std::int32_t DestBufferType;     // DestBufferType must be int for 32 bit mixing
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
    bool            isMuted;
    Instrument*     pInstrument;
    Sample*         pSample;
    unsigned        volume;
    unsigned        panning;

    unsigned        iVolumeEnvelope;
    unsigned        iPanningEnvelope;
    unsigned        iPitchFltrEnvelope;
    bool            keyIsReleased;

    Note            oldNote;
    Note            newNote;
    unsigned        lastNote;
    unsigned        period;
    unsigned        targetPeriod;
    unsigned        instrumentNo;
    unsigned        sampleNo;
    unsigned        sampleOffset;

    /*
    effect memory & index counters:
    */
    bool            patternIsLooping;
    unsigned        patternLoopStart;
    unsigned        patternLoopCounter;
    unsigned        lastArpeggio;
    unsigned        arpeggioCount;
    unsigned        arpeggioNote1;
    unsigned        arpeggioNote2;
    unsigned        vibratoWaveForm;
    unsigned        tremoloWaveForm;
    unsigned        panbrelloWaveForm;
    int             vibratoCount;
    int             tremoloCount;
    int             panbrelloCount;
    unsigned        retrigCount;
    unsigned        delayCount;
    unsigned        portaDestPeriod;
    unsigned        lastPortamentoUp;
    unsigned        lastPortamentoDown;
    unsigned        lastTonePortamento;
    unsigned        lastVibrato;
    unsigned        lastTremolo;
    unsigned        lastVolumeSlide;
    unsigned        lastFinePortamentoUp;
    unsigned        lastFinePortamentoDown;
    unsigned        lastFineVolumeSlideUp;
    unsigned        lastFineVolumeSlideDown;
    unsigned        lastGlobalVolumeSlide;
    unsigned        lastPanningSlide;
    unsigned        lastBpmSLide;
    unsigned        lastSampleOffset;
    unsigned        lastMultiNoteRetrig;
    unsigned        lastExtendedEffect;
    unsigned        lastTremor;
    unsigned        lastExtraFinePortamentoUp;
    unsigned        lastExtraFinePortamentoDown;
    unsigned        mixerChannelNr;
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
    unsigned        masterChannel;     // 0..63
    unsigned        age;
    bool            isActive;          // if not, mixer skips it
    bool            isMaster;          // false if it is a virtual channel
    Sample*         sample;
    unsigned        sampleOffset;      // samples can be up to 4 GB in theory
    unsigned        sampleOffsetFrac;

    unsigned        sampleIncrement;
    unsigned        leftVolume;
    unsigned        rightVolume;
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
    WAVEFORMATEX    waveFormatEx;

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
