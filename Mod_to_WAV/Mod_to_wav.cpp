/*

PROTRACKER effect implementation status:

---------------
#   Effect name
---------------
0   Arpeggio                            implemented
1   Slide Up                            implemented
2   Slide Down                          implemented
3   Tone Portamento                     implemented
4   Vibrato                             implemented
5   Tone Portamento + Volume Slide      implemented
6   Vibrato + Volume Slide              implemented
7   Tremolo                             
8   Set Panning Position                implemented
9   Set SampleOffset                    implemented // check protracker past sample jump behaviour
A   VolumeSlide                         implemented
B   Position Jump                       implemented
C   Set Volume                          implemented
D   Pattern Break                       implemented
E   Extended Effects                    see below
F   Set Speed / Tempo                   implemented


And here are the possible extended effects:
---------------------------------
#   Effect name
---------------------------------
E0  Set Filter                          not feasible
E1  FineSlide Up                        implemented
E2  FineSlide Down                      implemented
E3  Glissando Control
E4  Set Vibrato Waveform                implemented
E5  Set FineTune                        implemented
E6  Set/Jump to Loop                    implemented
E7  Set Tremolo Waveform                implemented
E8  NOT USED                            implemented (MTM panning)
E9  Retrig Note                         implemented
EA  Fine VolumeSlide Up                 implemented
EB  Fine VolumeSlide Down               implemented
EC  NoteCut                             implemented // check protracker past speed cut
ED  NoteDelay                           implemented // to check
EE  PatternDelay                        implemented
EF  Invert Loop
---------------------------------

*/




#include <cstdio>
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib,"winmm.lib") 
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

//extern const char *noteStrings[2 + MAXIMUM_NOTES];

/*
bugs:
    - starsmuz.s3m pans full left


Fixed / cleared up:
    - ssi2.s3m is read as 6 chn???  
      --> channel was muted in modplug tracker
    - SSI.S3M: weird deformed sample in 2nd part of the song (after start eastern type music)
      --> portamento memory fixed (hopefully)
    - menutune3.s3m: FF1 effect corrupts sample data (??)
      --> FF1 effect fixed (it did not corrupt sample data)
    - ALGRHYTH.MOD: ghost notes / high pitched notes in 2nd half of song 
      --> portamento bug
    - 2ND_PM.S3M: ghost notes / missing notes towards the end of the song
    - arpeggio counter should not be reset when a new note occurs!
      --> at least not in FT2
    - aryx.s3m: too high notes in pattern data
      --> key off commands, is normal
    - SSI.S3M: ghost notes towards the end
      --> should be ok now

*/

//#define debug_mixer


const char *noteStrings[2 + MAXIMUM_NOTES] = { "---",
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


#define BUFFER_LENGTH_IN_MINUTES   16
#define BENCHMARK_REPEAT_ACTION    1

#define LINEAR_INTERPOLATION  // TEMP

#define BUFFER_SIZE (44100 * 2 * 60 * BUFFER_LENGTH_IN_MINUTES) // 0x4000 // temp bloated buf for easy progging
#define FILTER_SQUARE 0
#define FILTER_LINEAR 1
#define FILTER_CUBIC  2
#define FILTER        FILTER_CUBIC

#define MIXER_MAX_CHANNELS 256
#define WAVE_BUFFER_NO 1 // temp, should be at least 4
#define VOLUME_RAMP_SPEED 41
#define MAX_NOTES_PER_CHANNEL 16
#define MIXRATE 44100

typedef int MixBufferType;

union INT_UNION {
    struct {
        SHORT hi;
        SHORT lo;
    };
    int s;
};

class Channel {
public:
    bool            isMuted;
    Instrument      *pInstrument;
    Sample          *pSample;
    unsigned        volume;
    unsigned        panning;
    unsigned        iPanningEnvelope;
    unsigned        iVolumeEnvelope;
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
    //unsigned        lastNonZeroFXArg;
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

    // active mixer channels for this logical channel:
    unsigned        mixerChannelsTable[MAX_NOTES_PER_CHANNEL]; // 0 means no channel
    unsigned        iPrimary;   // 0..15
public:
    void            init() { memset( this,0,sizeof( Channel ) ); }
    Channel() { init(); }
};

class MixerChannel {
public:
    unsigned        fromChannel;
    unsigned        age;
    bool            isActive;
    bool            isPrimary;
    bool            isFadingOut;
    Sample          *sample;
    unsigned        sampleOffset;      // samples can be up to 4 GB in theory
    unsigned        sampleOffsetFrac;


    unsigned        sampleIncrement;
    unsigned        leftVolume;
    unsigned        rightVolume;
    bool            isVolumeRampActive;
    bool            isPlayingBackwards;
    unsigned        iVolumeRamp;
public:
    void            init() { memset( this,0,sizeof( MixerChannel ) ); }
    MixerChannel() { init(); }
};

class Mixer {
public: // debug
    bool            isInitialised;
    bool            st300FastVolSlides_;
    bool            st3StyleEffectMemory_;
    bool            ft2StyleEffects_;
    bool            pt35StyleEffects_;
    Module          *module;
    MixBufferType   *mixBuffer;
    Channel         channels[PLAYER_MAX_CHANNELS];
    MixerChannel    mixerChannels[MIXER_MAX_CHANNELS]; // not a pointer, too slow
    unsigned        nActiveMixChannels; 
    unsigned        mixIndex;
    unsigned        mixCount;
    unsigned        callBpm;
    unsigned        gain;
    unsigned        saturation;
    unsigned        nChannels;
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
    Pattern         *pattern;
    Note            *iNote;

    unsigned        whichBuffer;
    HWAVEOUT        hWaveOut;
    WAVEFORMATEX    waveFormatEx;
    WAVEHDR         *waveHeaders;

public: // debug
    SHORT           *waveBuffers[WAVE_BUFFER_NO];
private: // debug

    unsigned        noteToPeriod(unsigned note, int finetune);
    unsigned        periodToFrequency(unsigned period /*, unsigned c4Speed */);
//    int             setVolume   (unsigned fromChannel, unsigned volume);  // range: 0..64
//    int             setPanning  (unsigned fromChannel, unsigned panning); // range: 0..255: extr left... extr right
    int             setMixerVolume(unsigned fromChannel);
    int             setFrequency(unsigned fromChannel, unsigned frequency);
    int             playSample  (unsigned fromChannel, Sample *sample, unsigned offset, bool direction);
    int             stopChannelPrimary( unsigned fromChannel );
    int             setVolumes ();
    int             updateImmediateEffects (); // grbml pattern delay grmbl... grrr!
    int             updateEffects ();
    int             updateNotes ();
    int             updateBpm ();
    int             setBpm () 
                    { callBpm = (MIXRATE * 5) / (bpm << 1); return 0; }
public:
    Mixer ();
    ~Mixer();
    int             initialise (Module *m);
    int             doMixBuffer (SHORT *buffer);
    int             doMixSixteenbitStereo(unsigned nSamples);
    //int             startReplay ();
    void            startReplay();
    int             stopReplay ();
    int             setGlobalPanning(unsigned panning); // range: 0..255 (extreme stereo... extreme reversed stereo)
//    int             setGlobalVolume (unsigned volume);  // range: 0..64
    int             setGlobalBalance(int balance); // range: -100...0...+100
    //unsigned        getnActiveChannels;    // make these into functions
    void            resetSong();
};

void Mixer::resetSong()
{
    mixCount = 0; // added for debugging!!!    should NOT be here
    mixIndex = 0;
    for ( unsigned i = 0; i < MIXER_MAX_CHANNELS; i++ ) {
        mixerChannels[i].init();
    }
    for ( unsigned i = 0; i < nChannels; i++ ) {
        channels[i].init();
        channels[i].panning = module->getDefaultPanPosition( i );
        /*
        switch ( module->getPanningStyle() ) {
            case PANNING_STYLE_MOD:
            {
                switch ( i % 4 ) {
                    case 1:
                    case 2: { channels[i]->panning = PANNING_FULL_RIGHT; break; }
                    case 0:
                    case 3: { channels[i]->panning = PANNING_FULL_LEFT;  break; }
                }
                break;
            }
            case PANNING_STYLE_S3M:
            {
                if ( i & 1 )  channels[i]->panning = PANNING_FULL_RIGHT;
                else        channels[i]->panning = PANNING_FULL_LEFT;
                break;
            }
            case PANNING_STYLE_XM:
            default:
            {
                channels[i]->panning = PANNING_CENTER;
                break;
            }
        }
        */
    }
    globalPanning_ = 0x20;  // 0 means extreme LEFT & RIGHT, so no attenuation
    globalVolume_ = 64;
    gain = 64;//128; // max = 256

    tempo = module->getDefaultTempo();
    bpm = module->getDefaultBpm();
    setBpm();

    patternDelay_ = 0;
    patternRow = 0;
    iPatternTable = 0;
    pattern = module->getPattern( module->getPatternTable( iPatternTable ) );
    iNote = pattern->getRow( 0 );
}

Mixer::Mixer () {
    int     bufSize = (sizeof(WAVEHDR) + sizeof(SHORT[BUFFER_SIZE]))
                      * WAVE_BUFFER_NO;
    char    *bufData;
    
    memset(this, 0, sizeof(Mixer));
    mixBuffer = new MixBufferType[BUFFER_SIZE];
    //memset(mixBuffer, 0, sizeof(MixBufferType[BUFFER_SIZE]));

    waveHeaders = (WAVEHDR *)(new char[bufSize]);
    memset(waveHeaders, 0, bufSize);

    // in memory, all headers are side by side,
    // followed by the data buffers - so the
    // audio data will form one continuous block.
    // point to first data buffer:
    bufData = (char *)waveHeaders;
    bufData += sizeof(WAVEHDR) * WAVE_BUFFER_NO;

    for (unsigned i = 0; i < WAVE_BUFFER_NO; i++) {
        waveHeaders[i].dwBufferLength = sizeof(SHORT[BUFFER_SIZE]);
        waveHeaders[i].lpData = (LPSTR) bufData;
        // waveHeaders[i].dwFlags = 0; // is done with memset() earlier
        waveBuffers[i] = (SHORT *) waveHeaders[i].lpData;
        bufData += sizeof(SHORT[BUFFER_SIZE]);
    }
    // prepare the header for the windows WAVE functions
    waveFormatEx.wFormatTag      = WAVE_FORMAT_PCM;
    waveFormatEx.nChannels       = 2;
    waveFormatEx.nSamplesPerSec  = MIXRATE; 
    waveFormatEx.wBitsPerSample  = 16;
    waveFormatEx.nBlockAlign     = waveFormatEx.nChannels * 
                                  (waveFormatEx.wBitsPerSample >> 3);
    waveFormatEx.nAvgBytesPerSec = waveFormatEx.nSamplesPerSec * 
                                   waveFormatEx.nBlockAlign;
    waveFormatEx.cbSize          = 0;
    return;
}

int Mixer::initialise(Module *m) {
    if (!m)              return 0;
    if (!m->isLoaded ()) return 0;
    module = m;
    nChannels = module->getnChannels ();
    switch ( module->getTrackerType() )
    {
        case TRACKER_PROTRACKER:
        {
            break;
        }
        case TRACKER_ST300:
        {
            st300FastVolSlides_ = true;
            st3StyleEffectMemory_ = true;
            break;
        }
        case TRACKER_ST321:
        {
            st3StyleEffectMemory_ = true;
            break;
        }
        case TRACKER_FT2:
        {
            ft2StyleEffects_ = true;
            break;
        }
    }
    /*
    for (unsigned i = 0; i < nChannels; i++) {
        if ( channels[i] == nullptr )
            channels[i] = new Channel;             
    }
    */
    resetSong();
    isInitialised = true;
    return 0;
}

int Mixer::doMixBuffer (SHORT *buffer) {
    memset(mixBuffer, 0, BUFFER_SIZE * sizeof(MixBufferType));
    mixIndex = 0;  
    unsigned x = callBpm - mixCount;
    unsigned y = BUFFER_SIZE / 2;   // stereo
    if (x > y) {
        mixCount += y;
        doMixSixteenbitStereo(y);
    } else {
        doMixSixteenbitStereo(x);
        x = y - x;
        mixCount = 0;
        updateBpm ();
        while (x >= callBpm) {
            doMixSixteenbitStereo(callBpm);
            x -= callBpm;
            updateBpm ();
        }
        if (x) {
            mixCount = x;
            doMixSixteenbitStereo(x);
        }
    }
    // transfer sampled data from ?? bit buffer into 16 bit buffer
    saturation = 0;
    MixBufferType *src = mixBuffer;
    SHORT *dst = buffer;
    for ( unsigned i = 0; i < BUFFER_SIZE; i++ ) {
        MixBufferType tmp = src[i] >> 8;
        if ( tmp < -32768 ) { tmp = -32768; saturation++; }
        if ( tmp >  32767 ) { tmp = 32767; saturation++; }
        dst[i] = (SHORT)tmp;
    }
    std::cout << "\n\nSaturation = " << saturation << "\n"; // DEBUG
    return 0;
}

int Mixer::doMixSixteenbitStereo(unsigned nSamples) {
    nActiveMixChannels = 0;
    for (unsigned i = 0; i < MIXER_MAX_CHANNELS; i++) {
        MixerChannel& mChn = mixerChannels[i];
        if (mChn.isActive) {
            Sample&         sample = *mChn.sample;
            unsigned        mixOffset = mixIndex;
            int             leftGain  = (gain * mChn.leftVolume) >> 12;   
            int             rightGain = (gain * mChn.rightVolume) >> 12;

            // div by zero safety. Probably because of portamento over/under flow
            if ( !mChn.sampleIncrement ) continue; 

            mChn.age++;
            nActiveMixChannels++;
            // quick hack for first tests
            if ( mChn.isFadingOut) {
                mChn.isActive = false;
                //mChn.isVolumeRampActive = true;
            }
            if (!mChn.isVolumeRampActive   //     && (i == 16) 
                ) {                
                MixBufferType *mixBufferPTR = mixBuffer + mixIndex;
                int chnInc = mChn.sampleIncrement;
                for (unsigned j = 0; j < nSamples; ) {
                    // mChn.sampleIncrement is never greater dan 2 ^ 17
                    if ( !mChn.isPlayingBackwards ) {
                        unsigned nrSamplesLeft = sample.getRepeatEnd() - mChn.sampleOffset;
                        if ( nrSamplesLeft > 8191 ) nrSamplesLeft = 8191;

                        //unsigned nrLoops = ((((nrSamplesLeft << 16) + mChn.sampleIncrement - 1)
                        //    - mChn.sampleOffsetFrac) / mChn.sampleIncrement);                        
                        unsigned nrLoops = ((((nrSamplesLeft << 15) + mChn.sampleIncrement - 1)
                            - mChn.sampleOffsetFrac) / mChn.sampleIncrement);                        

                        if ( nrLoops >= nSamples - j ) nrLoops = nSamples - j;                        

                        /*
                        // original rolled code:   // 32 bit
                        SHORT *SampleDataPTR = sample.getData() + mChn.sampleOffset;
                        for ( unsigned j2 = 0; j2 < nrLoops; j2++ ) {
                            int s1 = SampleDataPTR[(mChn.sampleOffsetFrac >> 16)];
                            int s2 = SampleDataPTR[(mChn.sampleOffsetFrac >> 16) + 1];
                            int xd = (mChn.sampleOffsetFrac & 0xFFFF) >> 1;  // time delta
                            int yd = s2 - s1;                                // sample delta
                            s1 += (xd * yd) >> 15;
                            *mixBufferPTR++ += (s1 * leftGain);
                            *mixBufferPTR++ += (s1 * rightGain);
                            mChn.sampleOffsetFrac += mChn.sampleIncrement;
                        }
                        */
                        

                        // ********************
                        // Start of loop unroll
                        // ********************

                        /*
                        // 32 bit frequency index:
                        SHORT *SampleDataPTR = sample.getData() + mChn.sampleOffset;
                        int loopEnd = nrLoops * mChn.sampleIncrement + mChn.sampleOffsetFrac;                        
                        
                        for ( int ofsFrac = mChn.sampleOffsetFrac; 
                            ofsFrac < loopEnd; ofsFrac += chnInc ) {
                            int s2 = *((int *)(SampleDataPTR + (ofsFrac >> 16)));
                            int s1 = (SHORT)s2;
                            s2 >>= 16;
                            s2 -= s1;                          // sample delta 
                            int xd = (ofsFrac & 0xFFFF) >> 1;  // time delta
                            s1 += (xd * s2) >> 15;
                            *mixBufferPTR++ += (s1 * leftGain);
                            *mixBufferPTR++ += (s1 * rightGain);
                        }
                        mChn.sampleOffsetFrac = loopEnd;
                        */

                        /*
                        // lineaire interpolatie:
                        // 31 bit frequency index:
                        SHORT *SampleDataPTR = sample.getData() + mChn.sampleOffset;
                        int loopEnd = nrLoops * mChn.sampleIncrement + mChn.sampleOffsetFrac;

                        for ( int ofsFrac = mChn.sampleOffsetFrac;
                            ofsFrac < loopEnd; ofsFrac += chnInc ) {
                            int s2 = *((int *)(SampleDataPTR + (ofsFrac >> 15)));
                            int s1 = (SHORT)s2;
                            s2 >>= 16;
                            s2 -= s1;                             // sample delta 
                            int xd = (ofsFrac & 0x7FFF);// >> 1;  // time delta
                            s1 += (xd * s2) >> 15;
                            *mixBufferPTR++ += (s1 * leftGain);    // this is faster
                            *mixBufferPTR++ += (s1 * rightGain);
                        }
                        mChn.sampleOffsetFrac = loopEnd; 
                        */ 

                        // cubic interpolation:
                        // 31 bit frequency index:                        
                        SHORT *SampleDataPTR = sample.getData() + mChn.sampleOffset;
                        int loopEnd = nrLoops * mChn.sampleIncrement + mChn.sampleOffsetFrac;
                        for ( int ofsFrac = mChn.sampleOffsetFrac;
                            ofsFrac < loopEnd; ofsFrac += chnInc ) {

                            int idx = ofsFrac >> 15;
                            int p0 = SampleDataPTR[idx - 1];
                            int p1 = SampleDataPTR[idx    ];
                            int p2 = SampleDataPTR[idx + 1];
                            int p3 = SampleDataPTR[idx + 2];
#define FRAC_RES_SHIFT  7
#define SAR             (15 - FRAC_RES_SHIFT)
                            
                            int fract = (ofsFrac & 0x7FFF) >> SAR;
                            int t = p1 - p2;
                            int a = ((t << 1) + t - p0 + p3) >> 1;
                            int b = (p2 << 1) + p0 - (((p1 << 2) + p1 + p3) >> 1);
                            int c = (p2 - p0) >> 1;

                            int f2 = ((
                                ((((((a  * fract) >> FRAC_RES_SHIFT) 
                                    + b) * fract) >> FRAC_RES_SHIFT)
                                    + c) * fract) >> FRAC_RES_SHIFT) 
                                    + p1;

                            //f2 = p1; // disable interpolation for testing
                            
                            *mixBufferPTR++ += (f2 * leftGain);    
                            *mixBufferPTR++ += (f2 * rightGain);
                        }
                        mChn.sampleOffsetFrac = loopEnd;

                        // ********************
                        // End of loop unroll
                        // ********************


                        mixOffset += nrLoops << 1;
                        j += nrLoops;
                        //mChn.sampleOffset += mChn.sampleOffsetFrac >> 16;
                        //mChn.sampleOffsetFrac &= 0xFFFF;
                        mChn.sampleOffset += mChn.sampleOffsetFrac >> 15;
                        mChn.sampleOffsetFrac &= 0x7FFF;
                        if( mChn.sampleOffset >= sample.getRepeatEnd() ) {
                            if ( sample.isRepeatSample() ) {
                                if ( !sample.isPingpongSample() )
                                {
                                    mChn.sampleOffset = sample.getRepeatOffset();  // ?
                                } else {
                                    mChn.sampleOffset = sample.getRepeatEnd() - 1; // ?
                                    mChn.isPlayingBackwards = true;
                                }
                            } else {
                                mChn.isActive = false;
                                mChn.isPrimary = false;
                                // remove reference to this mixer channel in the channels mixer channels table
                                unsigned k;
                                for ( k = 0; k < MAX_NOTES_PER_CHANNEL /* MAX_SAMPLES */; k++ ) {
                                    if ( channels[mChn.fromChannel].mixerChannelsTable[k] == i ) break;
                                }
                                channels[mChn.fromChannel].mixerChannelsTable[k] = 0;
                                j = nSamples; // quit loop, we're done here
                            }
                        }

                    // ********************************************************
                    // ********************************************************
                    // *********** Backwards playing code *********************
                    // ********************************************************
                    // ********************************************************
                    } else { 
                        // max sample length: 2 GB :)
                        int nrSamplesLeft = mChn.sampleOffset - sample.getRepeatOffset();
                        if ( nrSamplesLeft > 8191 ) nrSamplesLeft = 8191;
                        int nrLoops = 
                        //    ((nrSamplesLeft << 16) + (int)mChn.sampleIncrement - 1 
                            ((nrSamplesLeft << 15) + (int)mChn.sampleIncrement - 1
                            - (int)mChn.sampleOffsetFrac) / (int)mChn.sampleIncrement;
                        if ( nrLoops >= (int)(nSamples - j) ) nrLoops = (int)(nSamples - j);
                        if ( nrLoops < 0 ) nrLoops = 0;

                        mChn.sampleOffsetFrac = (int)mChn.sampleOffsetFrac + nrLoops * (int)mChn.sampleIncrement;
                        //int smpDataShift = mChn.sampleOffsetFrac >> 16;
                        int smpDataShift = mChn.sampleOffsetFrac >> 15;
                        if ( (int)mChn.sampleOffset < smpDataShift ) { // for bluishbg2.xm
                            mChn.sampleOffset = 0;
                            //std::cout << "underrun!" << std::endl;
                        } else mChn.sampleOffset -= smpDataShift;
                        SHORT *SampleDataPTR = sample.getData() + mChn.sampleOffset;

                        for ( int j2 = 0; j2 < nrLoops; j2++ ) {
                            //int s1 = SampleDataPTR[     (mChn.sampleOffsetFrac >> 16)];
                            //int s2 = SampleDataPTR[(int)(mChn.sampleOffsetFrac >> 16) - 1];
                            int s1 = SampleDataPTR[(mChn.sampleOffsetFrac >> 15)];
                            int s2 = SampleDataPTR[(int)(mChn.sampleOffsetFrac >> 15) - 1];
                            //int xd = (0x10000 - (mChn.sampleOffsetFrac & 0xFFFF)) >> 1;  // time delta
                            int xd = (0x8000 - (mChn.sampleOffsetFrac & 0x7FFF));       // time delta
                            int yd = s2 - s1;                                            // sample delta
                            s1 += (xd * yd) >> 15;
                            *mixBufferPTR++ += (s1 * leftGain);
                            *mixBufferPTR++ += (s1 * rightGain);
                            mChn.sampleOffsetFrac -= mChn.sampleIncrement;
                        }
                        mixOffset += nrLoops << 1;
                        j += nrLoops;
                       
                        if ( mChn.sampleOffset <= sample.getRepeatOffset() ) {
                            if ( sample.isRepeatSample() )
                            {
                                mChn.sampleOffset = sample.getRepeatOffset();
                                mChn.isPlayingBackwards = false;
                            } else {
                                mChn.isActive = false;
                                mChn.isPrimary = false;
                                // remove reference to this mixer channel in the channels mixer channels table
                                unsigned k;
                                for ( k = 0; k < MAX_NOTES_PER_CHANNEL /* MAX_SAMPLES */; k++ ) {
                                    if ( channels[mChn.fromChannel].mixerChannelsTable[k] == i ) break;
                                }
                                channels[mChn.fromChannel].mixerChannelsTable[k] = 0;
                                j = nSamples; // quit loop, we're done here
                            }
                        }
                    }
                }
            }
        }
    }
    mixIndex += (nSamples << 1); // *2 for stereo
    //std::cout << "# active chn = " << nActiveMixChannels << std::endl;
    return 0;
}

Mixer::~Mixer() {
    //for (unsigned i = 0; i < PLAYER_MAX_CHANNELS; i++) delete channels[i];
    //for (unsigned i = 0; i < MIXER_MAX_CHANNELS; i++) delete mixerChannels[i];
    delete waveHeaders;
    delete mixBuffer;
    return;
}

static CRITICAL_SECTION waveCriticalSection;
static volatile int waveFreeBlockCount;
static void CALLBACK waveOutProc(
                                HWAVEOUT hWaveOut, 
                                UINT uMsg, 
                                DWORD dwInstance, 
                                DWORD dwParam1,
                                DWORD dwParam2 ) {

    // pointer to free block counter
    int *freeBlockCounter = (int *)dwInstance;
    
    // ignore calls that occur due to openining and closing the device.
    if(uMsg != WOM_DONE) return;
    EnterCriticalSection(&waveCriticalSection);
    (*freeBlockCounter)++;
    LeaveCriticalSection(&waveCriticalSection);
}

void Mixer::startReplay() {
        MMRESULT        mmResult;

    mmResult = waveOutOpen( &hWaveOut, 
                            WAVE_MAPPER, 
                            &waveFormatEx, 
                            (DWORD_PTR)waveOutProc, 
                            (DWORD_PTR)&waveFreeBlockCount, 
                            CALLBACK_NULL /*CALLBACK_FUNCTION*/ );

    //doMixBuffer (waveBuffers[0]);   // moved to main program for benchmarking

#ifdef debug_mixer
    if (mmResult != MMSYSERR_NOERROR) { 
        std::cout << "\nError preparing wave mapper header!";
        switch (mmResult) {
            case MMSYSERR_INVALHANDLE : 
                {
                    std::cout << "\nSpecified device handle is invalid.";
                    break;
                }
            case MMSYSERR_NODRIVER    : 
                {
                    std::cout << "\nNo device driver is present.";
                    break;
                }
            case MMSYSERR_NOMEM       : 
                {
                    std::cout << "\nUnable to allocate or lock memory.";
                    break;
                }
            case WAVERR_UNPREPARED    : 
                {
                    std::cout << "\nThe data block pointed to by the pwh parameter hasn't been prepared.";
                    break;
                }
             default:
                {
                    std::cout << "\nOther unknown error " << mmResult;
                }
        }
    }
#endif 
    mmResult = waveOutPrepareHeader(hWaveOut, &(waveHeaders[0]), sizeof(WAVEHDR));
#ifdef debug_mixer
    if (mmResult != MMSYSERR_NOERROR) { 
        std::cout << "\nError preparing wave mapper header!";
        switch (mmResult) {
            case MMSYSERR_INVALHANDLE : 
                {
                    std::cout << "\nSpecified device handle is invalid.";
                    break;
                }
            case MMSYSERR_NODRIVER    : 
                {
                    std::cout << "\nNo device driver is present.";
                    break;
                }
            case MMSYSERR_NOMEM       : 
                {
                    std::cout << "\nUnable to allocate or lock memory.";
                    break;
                }
            case WAVERR_UNPREPARED    : 
                {
                    std::cout << "\nThe data block pointed to by the pwh parameter hasn't been prepared.";
                    break;
                }
             default:
                {
                    std::cout << "\nOther unknown error " << mmResult;
                }
        }
    }
#endif 
    mmResult = waveOutWrite(hWaveOut, &(waveHeaders[0]), sizeof(WAVEHDR));
#ifdef debug_mixer
    if (mmResult != MMSYSERR_NOERROR) { 
        std::cout << "\nError preparing wave mapper header!";
        switch (mmResult) {
            case MMSYSERR_INVALHANDLE : 
                {
                    std::cout << "\nSpecified device handle is invalid.";
                    break;
                }
            case MMSYSERR_NODRIVER    : 
                {
                    std::cout << "\nNo device driver is present.";
                    break;
                }
            case MMSYSERR_NOMEM       : 
                {
                    std::cout << "\nUnable to allocate or lock memory.";
                    break;
                }
            case WAVERR_UNPREPARED    : 
                {
                    std::cout << "\nThe data block pointed to by the pwh parameter hasn't been prepared.";
                    break;
                }
             default:
                {
                    std::cout << "\nOther unknown error " << mmResult;
                }
        }
    }
#endif 
    //return 0;
}

int Mixer::stopReplay () {
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);
    return 0;
}


unsigned Mixer::noteToPeriod(unsigned note, int finetune) {
    if (module->useLinearFrequencies()) {
        return (7680 - ((note - 1) << 6) - (finetune >> 1));  // grmbl... note - 1
    } else {
/*
FUNCTION TBeRoXMModule.GetPeriod(Note,FineTune:INTEGER):INTEGER;
VAR 
  RealNote,RealOctave,FineTuneValue,PeriodIndex,PeriodA,PeriodB:INTEGER;
BEGIN
  IF Note<1 THEN Note:=1;
  IF Note>132 THEN Note:=132;
  IF LinearSlides THEN 
    BEGIN
      RESULT:=(10*12*16*4)-(Note*16*4)-SAR(FineTune,1);
      IF RESULT<1 THEN RESULT:=1;
    END 
  ELSE 
    BEGIN
      RealNote:=(Note MOD 12)*(8);
      RealOctave:=Note DIV 12;
      FineTuneValue:=SAR(FineTune,4);
      PeriodIndex:=RealNote+FineTuneValue+8;
      IF        PeriodIndex<0    THEN PeriodIndex:=0;
      ELSE IF   PeriodIndex>=104 THEN PeriodIndex:=103;
      PeriodA:=XMPeriodTable[PeriodIndex];
      IF FineTune<0 THEN 
        BEGIN
          DEC(FineTuneValue);
          FineTune:=-FineTune;
        END 
      ELSE INC(FineTuneValue);
      PeriodIndex:=RealNote+FineTuneValue+8;
      IF      PeriodIndex<0    THEN PeriodIndex:=0;
      ELSE IF PeriodIndex>=104 THEN PeriodIndex:=103;
      PeriodB:=XMPeriodTable[PeriodIndex];
      FineTuneValue:=FineTune AND $0F;
      PeriodA:=PeriodA*(16-FineTuneValue);
      PeriodB:=PeriodB*FineTuneValue;
      RESULT:=SAR((PeriodA+PeriodB)*2,RealOctave);
//  RESULT:=TRUNC(POW(2,(133-(Note+(FineTune/128)))/12)*13.375);
 END;
END;
*/

/*
        unsigned    n = (((note - 1) % 12) << 3) + ((finetune >> 4) + 8);  // 8 + ...note - 1 ?????        
        //int         frac = finetune & 0xF;
        //frac |= ((finetune < 0) ? 0xFFFFFFF0 : 0);       
        unsigned    period1 = amigaPeriodTable[n    ];
        unsigned    period2 = amigaPeriodTable[n + 1];
        int         frac = (int)(((double)(finetune)) / 16.0
                                - (double)((int)(finetune / 16))) * 16 + 8;
        //int         frac = finetune - ((finetune >> 4) << 4) + 8;
        return (period1 * (16 - frac) + period2 * frac) >> (note / 12);
*/
        //finally. thank god for Benjamin Rousseaux!!!
        return (unsigned)(
            pow(2.0, 
                ( 133.0 - ((double)note + ((double)finetune / 128.0))) / 12.0
            ) * 13.375
        );
    }
}

/*

AMIGA calculations:

period = 13.375 * 2 ^ [ ( 133 - note - finetune / 128 ) / 12 ]

frequency = (8363 * 1712) / period

Ex:
note == 48 => amiga period should be = 1712
calculated period: 1813,8 (previous note's period actually == 1812)

frequency -->  7893,6

Frequency if calculated by:

PAL Value                     NTSC Value
===========                   ============

7093789.2                     7159090.5
----------- = 11745 Hz        ----------- = 11853 Hz
period * 2                    period * 2


PAL  freq * 4 = 7822
NTSC freq * 4 = 7894

*/




unsigned Mixer::periodToFrequency(unsigned period) {
    return module->useLinearFrequencies() ?
        (unsigned)(8363 * pow(2, ((4608.0 - (double)period) / 768.0)))
    :   (period ? ((8363 * 1712) / period ) : 0);
}

int Mixer::setMixerVolume( unsigned fromChannel ) {
    unsigned        gp = globalPanning_;
    bool            invchn = (gp >= PANNING_CENTER);
    unsigned        soften = (invchn ? (PANNING_FULL_RIGHT - gp) : gp);

    //std::cout << " fc " << fromChannel << ",";
    for (int i = 0; i < MAX_NOTES_PER_CHANNEL /* MAX_SAMPLES */; i++) {
        unsigned    mc = channels[fromChannel].mixerChannelsTable[i];
        //assert( mc <= 16 );
        //std::cout << " mc " << mc;
        if ( mc ) {
            MixerChannel& pmc = mixerChannels[mc];
            if ( pmc.isActive ) {
                unsigned        p = channels[fromChannel].panning;
                unsigned        v = channels[fromChannel].volume
                                        * globalVolume_;

                p = soften + ((p * (PANNING_MAX_STEPS - (soften << 1))) >> PANNING_SHIFT);
                if (invchn) p = PANNING_FULL_RIGHT - p;                 
                pmc.leftVolume  = ((PANNING_FULL_RIGHT - p) * v) >> PANNING_SHIFT; 
                pmc.rightVolume = ( p                       * v) >> PANNING_SHIFT; 
                if (balance_ < 0) {
                    pmc.rightVolume *= (100 + balance_);
                    pmc.rightVolume /= 100;
                }
                if (balance_ > 0) {
                    pmc.leftVolume  *= (100 - balance_);
                    pmc.leftVolume /= 100;
                }
            }
        }
        //std::cout << std::endl;
    }
    return 0;
}

/*
int Mixer::setVolume   (unsigned fromChannel, unsigned volume) {  // range: 0..64
    channels[fromChannel]->volume = volume;
    //setMixerVolume(fromChannel);
    return 0;
}

// range: 0..255: extr left... extr right
int Mixer::setPanning  (unsigned fromChannel, unsigned panning) { 
    channels[fromChannel]->panning = panning;
    //setMixerVolume(fromChannel);
    return 0;
}
*/
int Mixer::setFrequency(unsigned fromChannel, unsigned frequency) {
    for (int i = 0; i < MAX_NOTES_PER_CHANNEL /*MAX_SAMPLES */; i++) {
        unsigned    mc = channels[fromChannel].mixerChannelsTable[i];
        if (mc) {
            MixerChannel&    pmc = mixerChannels[mc];
            if (pmc.isPrimary) {
                //double f = ((double)frequency * 65536.0) / (double)MIXRATE;
                double f = ((double)frequency * 32768.0) / (double)MIXRATE;
                pmc.sampleIncrement = (unsigned) f;
                return 0;
            }
        }
    }
    return 0;
}
// range: 0..255 (extreme stereo... extreme reversed stereo)
int Mixer::setGlobalPanning(unsigned panning) { 
    globalPanning_ = panning;
    //for (unsigned i = 0; i < nChannels; i++) setMixerVolume(i);
    return 0;
}
/*
int Mixer::setGlobalVolume (unsigned volume) {  // range: 0..64  // useless fn?
    globalVolume_ = volume;
    //for (unsigned i = 0; i < nChannels; i++) setMixerVolume(i);
    return 0;
}
*/

int Mixer::setGlobalBalance(int balance) { // range: -100...0...+100
    balance_ = balance;
    //for (unsigned i = 0; i < nChannels; i++) setMixerVolume(i);
    return 0;
}

int Mixer::playSample ( unsigned fromChannel, Sample *sample, unsigned sampleOffset, bool direction ) {
    unsigned    oldestSlot;
    unsigned    emptySlot;
    unsigned    age;
    unsigned    newMc;
    unsigned    mcIndex;
    bool        sampleStarted = false;

    //assert( sample != nullptr );
    // stop previous note in same logical Channel    
    for ( unsigned i = 0; i < MAX_NOTES_PER_CHANNEL; i++ ) {
        unsigned j = channels[fromChannel].mixerChannelsTable[i]; 
        if ( j ) {
            if ( mixerChannels[j].isPrimary && (mixerChannels[j].fromChannel == fromChannel) ) {
                mixerChannels[j].isPrimary = false;
                channels[fromChannel].mixerChannelsTable[i] = 0;
                // temp: 
                mixerChannels[j].isFadingOut = true;
                mixerChannels[j].isActive = false;
                break;
            }
        }
    }
    /*
    mixerChannels[channels[fromChannel].iPrimary].isPrimary = false;
    mixerChannels[channels[fromChannel].iPrimary].isActive = false;
    channels[fromChannel].mixerChannelsTable[channels[fromChannel].iPrimary] = 0;
    */
    // find an empty slot in mixer channels table
    for ( emptySlot = 0; emptySlot < MAX_NOTES_PER_CHANNEL; emptySlot++ ) {
        if( !channels[fromChannel].mixerChannelsTable[emptySlot] ) break;
    }
    // None found, remove oldest sample (the longest playing one)
    // and use it's channel for the new sample
    if ( emptySlot >= MAX_NOTES_PER_CHANNEL ) {
        oldestSlot = 0;
        age = 0;
        for ( unsigned i = 0; i < MAX_NOTES_PER_CHANNEL; i++ ) {
            mcIndex = channels[fromChannel].mixerChannelsTable[i];
            if ( mixerChannels[mcIndex].age > age ) {
                age = mixerChannels[mcIndex].age;
                oldestSlot = i;
            }
        }
        emptySlot = oldestSlot;
        newMc = mcIndex;
    } else {
    // find a new channel for mixing
        newMc = 1; // mix channel 0 is never used
        while ( newMc < MIXER_MAX_CHANNELS ) {
            if ( !mixerChannels[newMc].isActive ) break;
            newMc++;
        }
    }

    if ( newMc < MIXER_MAX_CHANNELS ) { // should be unnecessary  (the check)
        mixerChannels[newMc].isActive = true;
        mixerChannels[newMc].isPrimary = true;
        mixerChannels[newMc].isPlayingBackwards = direction;
        mixerChannels[newMc].isFadingOut = false;
        mixerChannels[newMc].age = 0;
        mixerChannels[newMc].fromChannel = fromChannel;
        mixerChannels[newMc].sample = sample;
            // temp hack
        mixerChannels[newMc].isVolumeRampActive = false;
        mixerChannels[newMc].sampleOffset = sampleOffset;
        channels[fromChannel].mixerChannelsTable[emptySlot] = newMc; 
        channels[fromChannel].iPrimary = emptySlot;
    }
    return 0;
}

int Mixer::stopChannelPrimary( unsigned fromChannel ) {
    /*
    unsigned    oldestSlot;
    unsigned    emptySlot;
    unsigned    age;
    unsigned    newMc;
    unsigned    mcIndex;
    bool        sampleStarted = false;
    */
    // stop previous note in same logical Channel
    for ( unsigned i = 0; i < MAX_NOTES_PER_CHANNEL; i++ ) {
        unsigned j = channels[fromChannel].mixerChannelsTable[i];
        if ( j ) {
            if ( mixerChannels[j].isPrimary && (mixerChannels[j].fromChannel == fromChannel) ) {
                mixerChannels[j].isPrimary = false;
                channels[fromChannel].mixerChannelsTable[i] = 0;
                // temp: 
                mixerChannels[j].isFadingOut = true;
                mixerChannels[j].isActive = false;
                break;
            }
        }
    }
    /*
    mixerChannels[channels[fromChannel].iPrimary].isPrimary = false;
    mixerChannels[channels[fromChannel].iPrimary].isActive = false;
    channels[fromChannel].mixerChannelsTable[channels[fromChannel].iPrimary] = 0;
    */
    /*
    // find an empty slot in mixer channels table
    for ( emptySlot = 0; emptySlot < MAX_NOTES_PER_CHANNEL; emptySlot++ ) {
        if ( !channels[fromChannel].mixerChannelsTable[emptySlot] ) break;
    }
    // None found, remove oldest sample (the longest playing one)
    // and use it's channel for the new sample
    if ( emptySlot >= MAX_NOTES_PER_CHANNEL ) {
        oldestSlot = 0;
        age = 0;
        for ( unsigned i = 0; i < MAX_NOTES_PER_CHANNEL; i++ ) {
            mcIndex = channels[fromChannel].mixerChannelsTable[i];
            if ( mixerChannels[mcIndex].age > age ) {
                age = mixerChannels[mcIndex].age;
                oldestSlot = i;
            }
        }
        emptySlot = oldestSlot;
        newMc = mcIndex;
    } else {
        // find a new channel for mixing
        newMc = 1; // mix channel 0 is never used
        while ( newMc < MIXER_MAX_CHANNELS ) {
            if ( !mixerChannels[newMc].isActive ) break;
            newMc++;
        }
    }

    if ( newMc < MIXER_MAX_CHANNELS ) { // should be unnecessary  (the check)
        mixerChannels[newMc].isActive = true;
        mixerChannels[newMc].isPrimary = true;
        mixerChannels[newMc].isPlayingBackwards = direction;
        mixerChannels[newMc].isFadingOut = false;
        mixerChannels[newMc].age = 0;
        mixerChannels[newMc].fromChannel = fromChannel;
        mixerChannels[newMc].sample = sample;
        // temp hack
        mixerChannels[newMc].isVolumeRampActive = false;
        mixerChannels[newMc].sampleOffset = sampleOffset;
        channels[fromChannel].mixerChannelsTable[emptySlot] = newMc;
        channels[fromChannel].iPrimary = emptySlot;
    }
    */
    return 0;
}

int Mixer::updateNotes () {
    bool            patternBreak = false;
    unsigned        patternStartRow = 0;
    int             nextPatternDelta = 1;

    //if ( patternDelay_ ) { patternDelay_--; return 0; }

#ifdef debug_mixer
    
    char    hex[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned p = module->getPatternTable( iPatternTable );
    std::cout << std::setw( 2 ) << patternRow;
    //std::cout << "\n";
    /*
    if (iPatternTable < 10)    std::cout << " ";
    else                       std::cout << (iPatternTable / 10);
    std::cout << (iPatternTable % 10) << ":";
    if (p < 10) std::cout << " ";
    else        std::cout << (p / 10);
    std::cout << (p % 10) << "|";
    */
#endif

    patternLoopFlag_ = false;

    for (unsigned iChannel = 0; iChannel < nChannels; iChannel++) {
        Channel&    channel = channels[iChannel];
        unsigned    note,instrument,sample;
        bool        isNewNote;
        bool        isNewInstrument;
        bool        isValidInstrument;
        bool        isDifferentInstrument;
        bool        isNoteDelayed = false;
        bool        replay = false;
        bool        keyedOff = false;
        unsigned    oldInstrument;      
        int         finetune = 0;

        note       = iNote->note;        
        instrument = iNote->instrument;

        if ( note ) {            
            if (note == KEY_OFF) {
                isNewNote = false;
                keyedOff = true;
            } else {
                if ( note > MAXIMUM_NOTES ) std::cout << "!"; // DEBUG
                channel.lastNote = note;
                isNewNote = true;
                replay = true;
                channel.retrigCount = 0; // to check if that resets the counter, and when
                if( (channel.vibratoWaveForm & VIBRATO_NO_RETRIG_FLAG) == 0 ) 
                    channel.vibratoCount = 0;
                if ( ft2StyleEffects_ ) channel.sampleOffset = 0;  // ??
                //if ( ft2StyleEffects_ ) std::cout << "ft2!";
            }
        } else {
            isNewNote = false;
        }
        /*
            When an illegal instrument is specified together with a note, all 
            players stop playback but:
            - Protracker, Impulse Tracker & MPT start replay if a valid 
              instrument (without a note) is specified afterwards
            - FastTracker 2 and ScreamTracker 3 stay silent when the valid
              instrument (without a note) is specified afterwards

            Bottom line: stopping replay on encountering an illegal instrument
            is the way to go. The new valid instrument is played at the 
            frequency that was specified together with the invalid instrument.
        */
        
        oldInstrument = channel.instrumentNo;
        if ( instrument ) {
            isNewInstrument = true;
            isDifferentInstrument = (oldInstrument != instrument);
            channel.pInstrument = module->getInstrument( instrument /* - 1 */ ); 
            if ( channel.pInstrument ) 
            {
                if ( channel.lastNote ) 
                {
                    sample = channel.pInstrument->getSampleForNote
                        ( channel.lastNote - 1 );
                    // std::cout << std::setw( 4 ) << sample; // DEBUG
                    channel.pSample = 
                        //channel.pInstrument->getSample( sample );
                        module->getSample( sample );

                }
                if ( channel.pSample ) 
                {
                    channel.volume = channel.pSample->getVolume();
                    isValidInstrument = true;
                } else {
                    isValidInstrument = false;
                    replay = false;
                    stopChannelPrimary( iChannel ); // sundance.mod illegal sample
                    std::cout << std::dec
                        << "Sample cut by illegal inst "
                        << std::setw( 2 ) << instrument
                        << " in pattern " 
                        << std::setw( 2 ) << module->getPatternTable( iPatternTable )
                        << ", order " << std::setw( 3 ) << iPatternTable
                        << ", row " << std::setw( 2 ) << patternRow                        
                        << ", channel " << std::setw( 2 ) << iChannel 
                        << std::endl;
                    /*
                    if ( channel.pInstrument )
                    {
                        for ( int i = 0; i < MAXIMUM_NOTES; i++ )
                        {
                            std::cout << channel.pInstrument->getSampleForNote( i )
                                << " ";
                        }
                        std::cout << std::endl;
                    }
                    */
                }
            }
            channel.sampleOffset = 0;
        } else {
            isNewInstrument = false;
            channel.pInstrument = 
                module->getInstrument( oldInstrument /* - 1 */ );
            if ( channel.pInstrument ) {
                if ( channel.lastNote ) {               
                    sample = channel.pInstrument->getSampleForNote
                        ( channel.lastNote - 1 );
                    // std::cout << std::setw( 4 ) << sample; // DEBUG
                    channel.pSample = 
                        //channel.pInstrument->getSample( sample ); 
                        module->getSample( sample );
                }
            }
        }

        if ( isNewInstrument ) channel.instrumentNo = instrument;

        //if ( isNewNote ) replay = true;
        channel.oldNote = channel.newNote;
        channel.newNote = *iNote;

        /*
            Start effect handling
        */
        // check if a portamento effect occured:
        for ( unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMNS; fxloop++ )
        {
            unsigned& effect = iNote->effects[fxloop].effect;
            unsigned& argument = iNote->effects[fxloop].argument;
            if ( (effect == TONE_PORTAMENTO) ||
                (effect == TONE_PORTAMENTO_AND_VOLUME_SLIDE) ) {

                if ( argument && (effect == TONE_PORTAMENTO) )
                    channel.lastTonePortamento = argument;
                if ( channel.pSample ) // && isNewNote ) 
                    channel.portaDestPeriod =
                    noteToPeriod(
                        channel.lastNote + channel.pSample->getRelativeNote(),
                        channel.pSample->getFinetune()
                    );
                else // maybe not necessary
                    channel.portaDestPeriod =
                    noteToPeriod(
                        channel.lastNote + 0,
                        0
                    );
                replay = false;  // temp hack
            }
        }
        // valid sample for replay ? -> replay sample if new note
        if ( replay && channel.pSample && (!isNoteDelayed) ) {
            finetune = channel.pSample->getFinetune();
            channel.period = noteToPeriod(
                note + channel.pSample->getRelativeNote(),
                finetune );
        }

        for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMNS; fxloop++) 
        {
            const unsigned& effect   = iNote->effects[fxloop].effect;
            const unsigned& argument = iNote->effects[fxloop].argument;
            /* 
                ScreamTracker uses very little effect memory. The following 
                commands will take the previous non-zero effect argument as
                their argument if their argument is zero, even if that previous
                effect has its own effect memory or no effect memory (such as
                the set tempo command):

                Dxy: volume slide  
                Exx: portamento down
                Fxx: portamento up
                Ixy: tremor
                Jxy: arpeggio
                Kxy: volume slide + vibrato
                Lxy: volume slide + portamento
                Qxy: retrig + volume change
                Rxy: tremolo
                Sxy: extended effects (weird!)
                
            */
            if ( st3StyleEffectMemory_ && argument &&
                (fxloop == (MAX_EFFECT_COLUMNS - 1)) )
            {
                //channel.lastNonZeroFXArg = argument; 
                channel.lastVolumeSlide     = argument;
                channel.lastPortamentoDown  = argument;
                channel.lastPortamentoUp    = argument;
                channel.lastTremor          = argument;
                channel.lastArpeggio        = argument;
                channel.lastMultiNoteRetrig = argument;
                channel.lastTremolo         = argument;
                channel.lastExtendedEffect  = argument;
            }
            
            if ( (fxloop == (MAX_EFFECT_COLUMNS - 1)) &&
                (effect != VIBRATO) &&
                (effect != FINE_VIBRATO) &&
                (effect != VIBRATO_AND_VOLUME_SLIDE) )
            {
                //channel.vibratoCount = 0; // only on new note
                setFrequency( iChannel,periodToFrequency( channel.period ) );
            }
            if ( channel.oldNote.effects[fxloop].effect == ARPEGGIO ) {
                if( ! 
                    ((channel.newNote.effects[fxloop].effect == ARPEGGIO) &&
                    ft2StyleEffects_) )
                    setFrequency( iChannel,periodToFrequency( channel.period ) );
            }            

            switch ( effect ) {
                case ARPEGGIO : // to be reviewed
                {        
                    unsigned arpeggio = argument;
                    if ( st3StyleEffectMemory_ && (arpeggio == 0) )
                        arpeggio = channel.lastArpeggio; 
                    
                    if ( arpeggio ) {

                        if ( ft2StyleEffects_ ) // not exactly like FT2, quick hack
                            arpeggio = (arpeggio >> 4) | ((arpeggio & 0xF) << 4);

                        channel.arpeggioNote1 = channel.lastNote + (arpeggio >> 4);
                        channel.arpeggioNote2 = channel.lastNote + (arpeggio & 0xF);

                        if ( (channel.oldNote.effects[fxloop].effect == ARPEGGIO) 
                           && ft2StyleEffects_ ) 
                        {                            
                            if ( channel.pSample ) {
                                unsigned arpeggioPeriod;
                                switch ( channel.arpeggioCount ) {
                                    case 0:
                                    {
                                        arpeggioPeriod = noteToPeriod(
                                            channel.lastNote +
                                            channel.pSample->getRelativeNote(),
                                            channel.pSample->getFinetune()
                                        );
                                        break;
                                    }
                                    case 1:
                                    {
                                        arpeggioPeriod = noteToPeriod(
                                            channel.arpeggioNote1 +
                                            channel.pSample->getRelativeNote(),
                                            channel.pSample->getFinetune()
                                        );
                                        break;
                                    }
                                    case 2:
                                    {
                                        arpeggioPeriod = noteToPeriod(
                                            channel.arpeggioNote2 +
                                            channel.pSample->getRelativeNote(),
                                            channel.pSample->getFinetune()
                                        );
                                        break;
                                    }
                                }
                                channel.arpeggioCount++;
                                if ( channel.arpeggioCount >= 3 ) 
                                    channel.arpeggioCount = 0;
                                /*
                                playSample(
                                    iChannel,
                                    channel.pSample,
                                    channel.sampleOffset,
                                    FORWARD );
                                */
                                setFrequency(
                                    iChannel,
                                    periodToFrequency( arpeggioPeriod ) );
                                //replay = false;
                            }

                        }
                        else channel.arpeggioCount = 0;
                    }
                    break;
                }
                case PORTAMENTO_UP :
                {
                    if ( argument ) channel.lastPortamentoUp = argument;
                    if ( st3StyleEffectMemory_ )
                    {
                        unsigned lastPorta = channel.lastPortamentoUp;
                        //argument = channel.lastNonZeroFXArg;
                        unsigned xfx = lastPorta >> 4;
                        unsigned xfxArg = lastPorta & 0xF;
                        Effect& fxRemap = channel.newNote.effects[fxloop];
                        switch ( xfx )
                        {
                            case 0xE: // extra fine slide
                            {                                
                                fxRemap.effect = EXTRA_FINE_PORTAMENTO;
                                fxRemap.argument = (EXTRA_FINE_PORTAMENTO_UP << 4) + xfxArg;
                                channel.lastExtraFinePortamentoUp = xfxArg;                      
                                break;
                            }
                            case 0xF: // fine slide
                            {
                                fxRemap.effect = EXTENDED_EFFECTS;
                                fxRemap.argument = (FINE_PORTAMENTO_UP << 4) + xfxArg;
                                channel.lastFinePortamentoUp = xfxArg; 
                                break;
                            }
                            // default: normal portamento up
                        }
                    }
                    break;
                }
                case PORTAMENTO_DOWN :
                {
                    if ( argument ) channel.lastPortamentoDown = argument;
                    if ( st3StyleEffectMemory_ )
                    {
                        unsigned lastPorta = channel.lastPortamentoDown;
                        unsigned xfx = lastPorta >> 4;
                        unsigned xfxArg = lastPorta & 0xF;
                        Effect& fxRemap = channel.newNote.effects[fxloop];
                        switch ( xfx )
                        {
                            case 0xE: // extra fine slide
                            {
                                fxRemap.effect = EXTRA_FINE_PORTAMENTO;
                                fxRemap.argument = (EXTRA_FINE_PORTAMENTO_DOWN << 4) + xfxArg;
                                channel.lastExtraFinePortamentoDown = xfxArg;
                                break;
                            }
                            case 0xF: // fine slide
                            {
                                fxRemap.effect = EXTENDED_EFFECTS;
                                fxRemap.argument = (FINE_PORTAMENTO_DOWN << 4) + xfxArg;
                                channel.lastFinePortamentoDown = xfxArg;
                                break;
                            }
                            // default: normal portamento down
                        }
                    }
                    break;
                }
                // TONE_PORTAMENTO: // handled in a separate loop
                case SET_VIBRATO_SPEED : // XM volume column command
                case FINE_VIBRATO :
                case VIBRATO :
                {
                    unsigned& lv = channel.lastVibrato;
                    if ( argument & 0xF0 ) 
                            lv = (lv & 0xF) + (argument & 0xF0);
                    if ( argument & 0xF ) 
                            lv = (lv & 0xF0) + (argument & 0xF);

                    if ( ft2StyleEffects_ && (effect != SET_VIBRATO_SPEED) )
                    {
                        // Hxy: vibrato with x speed and y amplitude
                        channel.vibratoCount += channel.lastVibrato >> 4;
                        if ( channel.vibratoCount > 31 ) channel.vibratoCount -= 64;
                        unsigned vibAmp;
                        unsigned tableIdx;
                        if ( channel.vibratoCount < 0 ) tableIdx = -channel.vibratoCount;
                        else                           tableIdx = channel.vibratoCount;
                        switch ( channel.vibratoWaveForm & 0x3 )
                        {
                            case VIBRATO_RANDOM:
                            case VIBRATO_SINEWAVE:
                            {
                                vibAmp = sineTable[tableIdx];
                                break;
                            }
                            case VIBRATO_RAMPDOWN:
                            {
                                tableIdx <<= 3;
                                if ( channel.vibratoCount < 0 ) vibAmp = 255 - tableIdx;
                                else vibAmp = tableIdx;
                                break;
                            }
                            case VIBRATO_SQUAREWAVE:
                            {
                                vibAmp = 255;
                                break;
                            }
                        }
                        vibAmp *= channel.lastVibrato & 0xF;
                        vibAmp >>= 7;
                        if ( effect != FINE_VIBRATO ) vibAmp <<= 2;
                        unsigned period = channel.period;
                        if ( channel.vibratoCount > 0 ) period += vibAmp;
                        else                            period -= vibAmp;
                        setFrequency( iChannel,periodToFrequency( period ) );
                    }
                    break;
                }
                case TREMOLO : 
                {
                    unsigned& lt = channel.lastTremolo;
                    if (argument & 0xF0) 
                            lt = (lt & 0xF) + (argument & 0xF0);
                    if (argument & 0xF) 
                            lt = (lt & 0xF0) + (argument & 0xF);
                    //channel.lastTremolo = lt;
                    break;
                }
                case SET_FINE_PANNING : 
                {
                    channel.panning = argument;                       
                    break;
                }
                case SET_SAMPLE_OFFSET :   
                /*
                    Using the "OK" sample from ST-02

                    00 C-2 01 906		PT23	: kay - ay - ay - okay		<= buggy, not implemented
                    01 --- 00 000		PT315	: kay - kay - kay - okay
                    02 C-2 00 000		MED	    : kay - kay - kay - okay
                    03 --- 00 000		ST3	    : kay - kay - kay - okay
                    04 C-2 00 000		IO10	: kay - kay - kay - okay
                    05 --- 00 000		MTM	    : kay - okay - okay - okay
                    06 C-2 01 000		FT2	    : kay - okay - okay - okay
                    07 --- 00 000		IT	    : kay - okay - okay - okay

                    So it seems that the bug was fixed somewhere between Protracker 2.3d and
                    3.15. The problem is that there's no way to know if the bug emulation is
                    desirable or not when playing M.K. mods!
                */
                {                   
                    if ( channel.pSample ) {
                        if ( argument ) channel.lastSampleOffset = argument;
                        //else argument = channel.lastSampleOffset;
                        unsigned sOfs = channel.lastSampleOffset << 8;

                        /*
                        if ( sOfs < (channel.pSample->getLength())) {
                            channel.sampleOffset = sOfs;
                            if ( isNewNote ) replay = true;
                        } else {
                            replay = false;
                        } 
                        */

                        // to check if compatible with PROTRACKER / XM
                        if ( sOfs < (channel.pSample->getLength()) ) {
                            channel.sampleOffset = sOfs;                            
                        } else {
                            channel.sampleOffset = channel.pSample->getLength() - 1;
                            //channel.sampleOffset = channel.pSample->getRepeatOffset();
                        }
                        if ( isNewNote ) replay = true; // not necessary?
                    }
                    break;
                }
                case VOLUME_SLIDE :
                case TONE_PORTAMENTO_AND_VOLUME_SLIDE :
                case VIBRATO_AND_VOLUME_SLIDE :
                // Dxy, x = increase, y = decrease.
                // In .S3M the volume decrease has priority if both values are 
                // non zero and different from 0xF (which is a fine slide)
                {
                    if ( argument ) channel.lastVolumeSlide = argument;
                    if ( st3StyleEffectMemory_ && st300FastVolSlides_ )
                    { 
                        /*
                            So, apparently, in .S3M:
                            D01 = volume slide down by 1
                            D10 = volume slide up   by 1
                            DF0 = volume slide up   by F
                            D0F = volume slide down by F
                            D82 = volume slide down by 2      (!) (*)
                            DF1 = fine volume slide down by 1
                            D1F = fine volume slide up   by 1
                            DFF = fine volume slide up   by F (!)
                            (*) note that this is different from .mod where
                                volume slide up has priority over volume slide
                                down if both values are specified. Impulse 
                                tracker (.IT) ignores illegal volume slide 
                                altogether.
                        */
                        unsigned& lastSlide = channel.lastVolumeSlide;
                        unsigned slide1 = lastSlide >> 4;
                        unsigned slide2 = lastSlide & 0xF;
                        if ( slide1 & slide2 )
                        {
                            // these are fine slides:
                            if ( (slide1 == 0xF) || (slide2 == 0xF) ) break;
                        }
                        unsigned& v = channel.volume;
                        if ( slide2 ) { // slide down comes first
                            if ( slide2 > v ) v = 0;
                            else             v -= slide2;
                        } else {        // slide up
                            v += slide1;
                            if ( v > MAX_VOLUME ) v = MAX_VOLUME;
                        }
                    }
                    break;
                }
                case POSITION_JUMP :
                // Warning: position jumps takes s3m marker patterns into account
                {
                    /*
                        A Position jump resets the pattern start row set by a
                        pattern break that occured earlier on the same row. If
                        you want Position Jump + Pattern Break, you need
                        to put the position jump left of the pattern break.
                    */
                    /*if ( ft2StyleEffects_ && patternBreak )*/ patternStartRow = 0;
                    patternBreak = true;
                    nextPatternDelta = (int)argument - (int)iPatternTable; 
                    break;
                }
                case SET_VOLUME :
                {
                    channel.volume = argument;
                    break;
                }
                case PATTERN_BREAK :
                {
                    unsigned startRow = (argument >> 4) * 10 + (argument & 0xF);
                    /*
                        ST3 & FT2 can't jump past row 64. Impulse Tracker can,
                        values higher than the nr of rows of the next pattern
                        are converted to zero.
                        ST3 ignores illegal pattern breaks, IT jumps to the 
                        next pattern though.
                    */
                    if ( startRow < 64 )
                    {
                        patternBreak = true;
                        patternStartRow = startRow;
                    } // else if impulse tracker then ...
                    break;
                }
                case EXTENDED_EFFECTS : 
                {
                    unsigned extFXArg = argument;
                    if ( st3StyleEffectMemory_ ) 
                        extFXArg = channel.lastExtendedEffect;
                    unsigned xfx    = extFXArg >> 4;
                    unsigned xfxArg = extFXArg & 0xF;
                    if ( st3StyleEffectMemory_ ) // remap st3 style effects
                    {
                        switch ( xfx )
                        {
                            case S3M_SET_GLISSANDO_CONTROL: 
                            {                                    
                                xfx = SET_GLISSANDO_CONTROL;
                                break;
                            }
                            case S3M_SET_FINETUNE: 
                            {
                                xfx = SET_FINETUNE;
                                break;
                            }
                            case S3M_SET_VIBRATO_CONTROL: 
                            {
                                xfx = SET_VIBRATO_CONTROL;
                                break;
                            }
                            case S3M_SET_TREMOLO_CONTROL: 
                            {
                                xfx = SET_TREMOLO_CONTROL;
                                break;
                            }
                            case S3M_SET_PANBRELLO_CONTROL: 
                            {
                                channel.panbrelloWaveForm = xfxArg & 0x7;
                                xfx = NO_EFFECT;
                                xfxArg = NO_EFFECT;
                                break;
                            }
                            case S3M_FINE_PATTERN_DELAY:     // todo
                            {
                                xfx = NO_EFFECT;
                                xfxArg = NO_EFFECT;
                                break;
                            }
                            case S3M_SOUND_CONTROL:          // todo
                            {
                                xfx = NO_EFFECT;
                                xfxArg = NO_EFFECT;
                                break;
                            }
                            case S3M_SET_HIGH_SAMPLE_OFFSET: // todo
                            {
                                xfx = NO_EFFECT;
                                xfxArg = NO_EFFECT;
                                break;
                            }
                            case S3M_SET_PATTERN_LOOP: 
                            {
                                xfx = SET_PATTERN_LOOP;
                                break;
                            }
                            // S3M_NOTE_CUT:      same as .mod
                            // S3M_NOTE_DELAY:    same as .mod
                            // S3M_PATTERN_DELAY: same as .mod
                            case 0x0:
                            case 0x7:
                            case 0xF:
                            {
                                xfx = NO_EFFECT;
                                xfxArg = NO_EFFECT;
                                break;
                            }
                        }
                        channel.newNote.effects[fxloop].argument = 
                            (xfx << 4) | xfxArg;
                    }
                    // Protracker & XM extended effect handling:
                    switch ( xfx ) {
                        case NO_EFFECT:
                        {
                            break;
                        }
                        case FINE_PORTAMENTO_UP:
                        {
                            if ( xfxArg ) channel.lastFinePortamentoUp = xfxArg;
                            break;
                        }
                        case FINE_PORTAMENTO_DOWN:
                        {
                            if ( xfxArg ) channel.lastFinePortamentoDown = xfxArg;
                            break;
                        }
                        case SET_GLISSANDO_CONTROL:
                        {
                            break;
                        }
                        case SET_VIBRATO_CONTROL: 
                        {
                            channel.vibratoWaveForm = xfxArg & 0x7;
                            break;
                        }
                        case SET_FINETUNE:
                        {
                            finetune = xfxArg;
                            if (finetune & 8) finetune |= 0xFFFFFFF0;
                            break;
                        }
                        case SET_PATTERN_LOOP: 
                        {
                            if ( !xfxArg ) channel.patternLoopStart = patternRow;
                            else {
                                if ( !channel.patternIsLooping ) 
                                { 
                                    channel.patternLoopCounter = xfxArg;
                                    patternLoopStartRow_ = channel.patternLoopStart;
                                    channel.patternIsLooping = true;
                                    patternLoopFlag_ = true;
                                } else { 
                                    channel.patternLoopCounter--;
                                    if ( !channel.patternLoopCounter )
                                        channel.patternIsLooping = false;
                                    else { 
                                        patternLoopStartRow_ = channel.patternLoopStart;
                                        patternLoopFlag_ = true;
                                    }
                                }
                            }
                            break;
                        }
                        case SET_TREMOLO_CONTROL:
                        {
                            channel.tremoloWaveForm = xfxArg & 0x7;
                            break;
                        }
                        case SET_ROUGH_PANNING: 
                        {
                            if ( !ft2StyleEffects_ )
                                channel.panning = xfxArg << 4;
                            break;
                        }
                        case NOTE_CUT: 
                        {
                            if ( !xfxArg ) channel.volume = 0;
                            break;
                        }
                        case NOTE_DELAY: 
                        {
                            if( xfxArg < tempo )
                            {
                                channel.delayCount = xfxArg;
                                isNoteDelayed = true;
                            }
                            break;
                        }
                        case PATTERN_DELAY :
                        {
                            /* 
                                ScreamTracker 3 & Impulse Tracker:
                                mostLeft delay command prevails if multiple 
                                pattern delay commands are found on the same
                                row.

                                ProTracker & FastTracker:
                                mostRight delay command prevails if multiple
                                pattern delay commands are found on the same
                                row.
                            */
                            if ( st3StyleEffectMemory_ ) { 
                                if ( !patternDelay_ ) patternDelay_ = xfxArg;
                            } else patternDelay_ = xfxArg;
                            break;
                        }
                    }
                    break;
                } // end of S3M / XM extended effects
                case SET_TEMPO :
                {
                    tempo = argument;
                    break;
                }
                case SET_BPM :
                {
                    bpm = argument;
                    setBpm();
                    break;
                }
                case SET_GLOBAL_VOLUME :
                {
                    globalVolume_ = argument;
                    break;
                }
                case GLOBAL_VOLUME_SLIDE :
                {
                    if ( argument ) 
                        channel.lastGlobalVolumeSlide = argument;
                    break;
                }
                case SET_ENVELOPE_POSITION :
                {
                    break;
                }
                case PANNING_SLIDE :
                {
                    if ( argument ) channel.lastPanningSlide = argument;
                    break;
                }
                case MULTI_NOTE_RETRIG :
                {
                    if( argument ) channel.lastMultiNoteRetrig = argument;                        
                    break;
                }
                case TREMOR :
                {
                    break;
                }
                case EXTRA_FINE_PORTAMENTO :
                {           
                    unsigned xfx = argument >> 4;
                    unsigned xfxArg = argument & 0xF;
                    switch ( xfx ) {
                        case EXTRA_FINE_PORTAMENTO_UP:
                        {
                            if ( xfxArg ) channel.lastExtraFinePortamentoUp = xfxArg;
                            break;
                        }
                        case EXTRA_FINE_PORTAMENTO_DOWN:
                        {
                            if ( xfxArg ) channel.lastExtraFinePortamentoDown = xfxArg;
                            break;
                        }
                    }
                    break;
                }            
            }
        }
        // valid sample for replay ? -> replay sample if new note
        if ( replay && channel.pSample && (!isNoteDelayed) ) {
            playSample( iChannel,channel.pSample,
                channel.sampleOffset,FORWARD );
            channel.period = noteToPeriod(
                note + channel.pSample->getRelativeNote(),
                finetune
            );
            setFrequency( 
                iChannel,
                periodToFrequency( channel.period ) );
        }
        if (!isNoteDelayed) { 
            /*
               channel->panning = channel->newpanning;
               channel->volume  = channel->newvolume;
            */
            //setPanning(iChannel, channel->panning);  // temp
            //setVolume(iChannel, channel->volume);    // temp
        }
#ifdef debug_mixer
        #define FOREGROUND_LIGHTGRAY    (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)
        #define FOREGROUND_WHITE        (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY )
        #define FOREGROUND_BROWN        (FOREGROUND_GREEN | FOREGROUND_RED )
        #define FOREGROUND_YELLOW       (FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)
        #define FOREGROUND_LIGHTCYAN    (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
        #define FOREGROUND_LIGHTMAGENTA (FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY)
        #define FOREGROUND_LIGHTBLUE    (FOREGROUND_BLUE | FOREGROUND_INTENSITY)
        #define BACKGROUND_BROWN        (BACKGROUND_RED | BACKGROUND_GREEN)
        #define BACKGROUND_LIGHTBLUE    (BACKGROUND_BLUE | BACKGROUND_INTENSITY )
        #define BACKGROUND_LIGHTGREEN   (BACKGROUND_GREEN | BACKGROUND_INTENSITY )
        if ( iChannel < 8 )
        {            
            // **************************************************
            // colors in console requires weird shit in windows
            HANDLE hStdin = GetStdHandle( STD_INPUT_HANDLE );
            HANDLE hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
            CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
            GetConsoleScreenBufferInfo( hStdout,&csbiInfo );
            // **************************************************

            // display note
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
            std::cout << "|";
            SetConsoleTextAttribute( hStdout, FOREGROUND_LIGHTCYAN );
            
            if ( note < (MAXIMUM_NOTES + 2)) std::cout << noteStrings[note];
            else {
                std::cout << std::hex << std::setw(3) << (unsigned)note << std::dec;
            }
            /*
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
            std::cout << std::setw( 5 ) << noteToPeriod( note,channel.pSample->getFinetune() );

            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTBLUE );
            std::cout << "," << std::setw( 5 ) << channel.period;

            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTMAGENTA );
            std::cout << "," << std::setw( 5 ) << channel.portaDestPeriod;
            */
            // display instrument
            SetConsoleTextAttribute( hStdout,FOREGROUND_YELLOW );
            if( instrument )
                std::cout << std::dec << std::setw( 2 ) << instrument;
            else std::cout << "  ";
            /*
            // display volume column
            SetConsoleTextAttribute( hStdout,FOREGROUND_GREEN | FOREGROUND_INTENSITY );
            if ( iNote->effects[0].effect )
                std::cout << std::hex << std::uppercase
                    << std::setw( 1 ) << iNote->effects[0].effect
                    << std::setw( 2 ) << iNote->effects[0].argument;
            else std::cout << "   ";
            */
            // display volume:
            SetConsoleTextAttribute( hStdout,FOREGROUND_GREEN | FOREGROUND_INTENSITY );
            std::cout << std::hex << std::uppercase
                << std::setw( 2 ) << channel.volume;

            // effect
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
            for ( unsigned fxloop = 1; fxloop < MAX_EFFECT_COLUMNS; fxloop++ ) {
                if( iNote->effects[fxloop].effect )
                    std::cout
                        << std::hex << std::uppercase
                        << std::setw( 2 ) << iNote->effects[fxloop].effect;
                else std::cout << "--";
                SetConsoleTextAttribute( hStdout,FOREGROUND_BROWN );
                std::cout
                    << std::setw( 2 ) << (iNote->effects[fxloop].argument ) 
                    << std::dec;
            }
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
        } 
#endif
        iNote++;
    } // end of effect processing
#ifdef debug_mixer
    std::cout << std::endl;
#endif
/*
Pattern loops are set by channel, and nesting is allowed as long as
the loops are in different tracks. The nested loop behaviour is somewhat
nonobvious as seen in the following examples.

   00 --- 00 E60 | --- 00 E60		00 --- 00 000 | --- 00 E60
   01 C-2 01 000 | --- 00 000		01 C-2 01 000 | --- 00 000
   02 --- 00 E62 | --- 00 E62		02 --- 00 000 | --- 00 E62

In both cases the note is played three times.

   00 --- 00 E60 | --- 00 E60
   01 C-2 01 000 | --- 00 000
   02 --- 00 E63 | --- 00 E62

In this situation the note is played twelve times. We escape the loop when
both counters are zeroed. Here's how the internal registers look like:

	Iteration => 1 2 3 4 5 6 7 8 9 A B C
	Channel 1 => 3 2 1 0 3 2 1 0 3 2 1 0
	Channel 2 => 2 1 0 2 1 0 2 1 0 2 1 0


If a loop end is used with no start point set, it jumps to the first line
of the pattern. If a pattern break is inside a loop and there is a loop
end in the next pattern it jumps to the row set as loop start in the
previous pattern. It is also possible to make an infinite loop in
Protracker and Fast Tracker II using nested loops in the same track:

   00 --- 00 E60 | --- 00 000
   01 --- 00 000 | --- 00 000
   02 C-2 01 000 | --- 00 000
   03 --- 00 E61 | --- 00 000
   04 --- 00 E61 | --- 00 000	<= infinite loop

S3M and IT set a new start point in the line after the end of the previous
loop, making the infinite loop impossible.
*/

    if ( patternLoopFlag_ ) 
    { 
        patternRow = patternLoopStartRow_;
        iNote = pattern->getRow( patternRow );
    } else {
        // prepare for next row / next function call
        if ( !patternBreak ) patternRow++;
        else {
            patternLoopStartRow_ = 0;
            patternRow = pattern->getnRows();
        }
    }
    if ( patternRow >= pattern->getnRows() ) {
#ifdef debug_mixer
        std::cout << "\n";
        //_getch();
#endif
        patternRow = patternStartRow;
        /*
            FT2 starts the pattern on the same row that the pattern loop 
            started in the pattern before it
        */
        if ( ft2StyleEffects_ ) {
            patternRow = patternLoopStartRow_;
            patternLoopStartRow_ = 0;
        }
        int iPtnTable = (int)iPatternTable + nextPatternDelta;
        if ( iPtnTable < 0 ) iPatternTable = 0; // should be impossible
        else iPatternTable = iPtnTable;
        // skip marker patterns:
        for ( ; 
            (iPatternTable < module->getSongLength()) && 
            (module->getPatternTable( iPatternTable ) == MARKER_PATTERN)
            ; iPatternTable++ 
            );
        if (iPatternTable >= module->getSongLength()) { 
            iPatternTable = module->getSongRestartPosition(); // repeat song
            // skip marker patterns:
            for ( ;
                (iPatternTable < module->getSongLength()) &&
                (module->getPatternTable( iPatternTable ) == MARKER_PATTERN)
                ; iPatternTable++
            );
        }
        pattern = module->getPattern( module->getPatternTable( iPatternTable ) );
        iNote = pattern->getRow( patternRow );
#ifdef debug_mixer
        std::cout << std::endl
            << "Playing pattern # " 
            << module->getPatternTable( iPatternTable ) 
            << ", order # " << iPatternTable
            << std::endl;
#endif
    }
    return 0;
}

int Mixer::updateEffects () {
    for (unsigned iChannel = 0; iChannel < nChannels; iChannel++) {
        Channel&     channel = channels[iChannel];

        for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMNS; fxloop++) {
            Note&       note = channel.newNote;
            unsigned    effect   = note.effects[fxloop].effect;
            unsigned    argument = note.effects[fxloop].argument;

            // Volume slide + pitch commands: handle volume slide first
            /* 
                MOD / XM: command Axy where x = increase, y = decrease volume
                S3M / IT / MPT: command Dxy, same parameters, with Fy / xF
                                being fine slides.

                So, apparently, in .S3M:
                D01 = volume slide down by 1
                D10 = volume slide up   by 1
                DF0 = volume slide up   by F
                D0F = volume slide down by F
                D82 = volume slide down by 2      (!) (*)
                DF1 = fine volume slide down by 1
                D1F = fine volume slide up   by 1
                DFF = fine volume slide up   by F (!)

                (*) Note that this is different from .mod where
                volume slide up has priority over volume slide
                down if both values are specified. Impulse
                tracker (.IT) ignores illegal volume slide effects
                altogether. Illegal volume slide arguments are
                corrected in the .mod and .xm loaders. This is
                not possible with .s3m because of the way effect
                memory works in .s3m.                
            */
            switch ( effect ) {
                case VOLUME_SLIDE:
                case TONE_PORTAMENTO_AND_VOLUME_SLIDE:
                case VIBRATO_AND_VOLUME_SLIDE:
                {
                    unsigned& lastSlide = channel.lastVolumeSlide;
                    unsigned slide1 = lastSlide >> 4;
                    unsigned slide2 = lastSlide & 0xF;
                    if ( slide1 & slide2 )
                    {
                        // these are fine slides:
                        if ( (slide1 == 0xF) || (slide2 == 0xF) ) break;
                        // if ( impulse tracker ) break; // impulse tracker ignores illegal vol slides!
                    }
                    unsigned& v = channel.volume;
                    //if ( st3StyleEffectMemory_ )
                    //{
                        // slide down has priority in .s3m
                        if ( slide2 ) { 
                            if ( slide2 > v ) v = 0;
                            else              v -= slide2;
                        } else {        // slide up
                            v += slide1;
                            if ( v > MAX_VOLUME ) v = MAX_VOLUME;
                        }
                    /* // fixed in .mod and .xm loader
                    } else { 
                        // slide up has priority in .mod
                        if ( slide1 ) {
                            v += slide1;
                            if ( v > MAX_VOLUME ) v = MAX_VOLUME;
                        } else {        // slide down
                            if ( slide2 > v ) v = 0;
                            else              v -= slide2;
                        }
                    }
                    */
                    break;
                }
            }
            /*
                Handle the pitch & other commands
            */
            switch ( effect ) {
                case ARPEGGIO :
                {
                    if ( channel.pSample ) {                        
                        channel.arpeggioCount++;
                        if ( channel.arpeggioCount >= 3 )
                            channel.arpeggioCount = 0; // added 
                        unsigned arpeggioPeriod;
                        switch ( channel.arpeggioCount ) {
                            case 0 : 
                            {
                                arpeggioPeriod = noteToPeriod(
                                        channel.lastNote +
                                        channel.pSample->getRelativeNote(),
                                        channel.pSample->getFinetune()
                                );
                                break;
                            }
                            case 1 :
                            {
                                arpeggioPeriod = noteToPeriod(
                                        channel.arpeggioNote1 +
                                        channel.pSample->getRelativeNote(),
                                        channel.pSample->getFinetune() 
                                );
                                break;
                            }
                            case 2 : 
                            {
                                arpeggioPeriod = noteToPeriod(
                                        channel.arpeggioNote2 +
                                        channel.pSample->getRelativeNote(),
                                        channel.pSample->getFinetune() 
                                );
                                break;
                            }
                        }
                        /*
                        channel.arpeggioCount++;
                        if ( channel.arpeggioCount >= 3 )
                            channel.arpeggioCount = 0;
                        */
                        /*
                        playSample( 
                            iChannel,
                            channel.pSample,
                            channel.sampleOffset,
                            FORWARD );
                        */
                        setFrequency( 
                            iChannel,
                            periodToFrequency( arpeggioPeriod ) );
                    }
                    break;
                }
                case PORTAMENTO_UP :
                {       
                    argument = channel.lastPortamentoUp << 2;
                    if ( channel.period > argument )
                    {
                        channel.period -= argument;
                        if ( channel.period < module->getMinPeriod() )
                            channel.period = module->getMinPeriod();
                    } else channel.period = module->getMinPeriod();
                    setFrequency(
                        iChannel,
                        periodToFrequency( channel.period ) );
                    break;
                }
                case PORTAMENTO_DOWN :
                {
                    argument = channel.lastPortamentoDown << 2;
                    channel.period += argument;
                    if ( channel.period > module->getMaxPeriod() )
                        channel.period = module->getMaxPeriod();
                    setFrequency( iChannel,
                        periodToFrequency( channel.period ) );
                    break;
                }
                case TONE_PORTAMENTO :
                case TONE_PORTAMENTO_AND_VOLUME_SLIDE :
                {                    
                    unsigned portaSpeed = channel.lastTonePortamento << 2;
                    //if ( note.note && () && channel.portaDestPeriod )
                    /*
                    if ( ((channel.oldNote.effects[fxloop].effect == TONE_PORTAMENTO) || 
                          (channel.oldNote.effects[fxloop].effect == TONE_PORTAMENTO_AND_VOLUME_SLIDE))
                        || (note.note && (note.note != KEY_OFF)) )
                    */
                    {
                        if ( channel.portaDestPeriod )
                        {
                            if ( channel.period < channel.portaDestPeriod )
                            {
                                channel.period += portaSpeed;
                                if ( channel.period > channel.portaDestPeriod )
                                    channel.period = channel.portaDestPeriod;
                            } else if ( channel.period > channel.portaDestPeriod )
                            {
                                if ( channel.period > portaSpeed )
                                    channel.period -= portaSpeed;
                                else channel.period = channel.portaDestPeriod;
                                if ( channel.period < channel.portaDestPeriod )
                                    channel.period = channel.portaDestPeriod;
                            }
                            setFrequency( iChannel,
                                periodToFrequency( channel.period ) );
                        }
                        
                    }
                    break;
                }

                case VIBRATO:
                case FINE_VIBRATO:
                case VIBRATO_AND_VOLUME_SLIDE:
                {
                    // Hxy: vibrato with x speed and y amplitude
                    channel.vibratoCount += channel.lastVibrato >> 4;
                    if ( channel.vibratoCount > 31 ) channel.vibratoCount -= 64;
                    unsigned vibAmp;
                    unsigned tableIdx;
                    if (channel.vibratoCount < 0 ) tableIdx = -channel.vibratoCount;
                    else                           tableIdx = channel.vibratoCount;                    
                    switch ( channel.vibratoWaveForm & 0x3 )
                    {
                        case VIBRATO_RANDOM:
                        case VIBRATO_SINEWAVE:
                        {
                            vibAmp = sineTable[tableIdx];
                            break;
                        }
                        case VIBRATO_RAMPDOWN:
                        {
                            tableIdx <<= 3;
                            if ( channel.vibratoCount < 0 ) vibAmp = 255 - tableIdx;
                            else vibAmp = tableIdx;
                            break;
                        }
                        case VIBRATO_SQUAREWAVE:
                        {
                            vibAmp = 255;
                            break;
                        }
                    }
                    vibAmp *= channel.lastVibrato & 0xF;
                    vibAmp >>= 7;
                    if (effect != FINE_VIBRATO ) vibAmp <<= 2;
                    //vibAmp >>= 1;

                    //unsigned frequency = periodToFrequency( channel.period );
                    //if ( channel.vibratoCount >= 0 )    frequency += vibAmp;
                    //else                                frequency -= vibAmp;
                    //std::cout << "F = " << frequency << std::endl;
                    //setFrequency( iChannel,frequency );

                    unsigned period = channel.period;
                    if ( channel.vibratoCount > 0 ) period += vibAmp;
                    else                            period -= vibAmp;
                    setFrequency( iChannel,periodToFrequency( period ) );

                    break;
                }
                case TREMOLO: 
                {
                    break;
                }
                case EXTENDED_EFFECTS:
                {
                    effect = argument >> 4;
                    argument &= 0xF;
                    switch ( effect ) {
                        case NOTE_RETRIG :                             
                        {
                            /*
                                Note Retrig effect details for .MOD, reference:
                                OpenMPT 1.28.02.00
                                Tick 0 == row itself, where note is processed
                                Tick 1 == after first row, where we process 
                                            volume slide etc.

                                ** speed 1: **
                                E9x: Ignored completely, for every value of x, even 
                                for x == 0. Subsequent E9x commands immediately
                                afterwards the previous E9x command are also 
                                ignored, even if no sample is playing.
                                Bottom line: don't process E9x when speed == 1.

                                ** speed 2: **
                                E90: retrigs sample 1x on tick 1
                                E91: retrigs sample 1x on tick 1
                                E92 alone: does nothing
                                E92 followed by another E92, E91 or E90,
                                not necessarily on the next row but
                                simply on a later row: retrigs the sample on tick 1

                                E92 followed by E90,
                                E92 followed by E91,
                                E92 followed by E92: retrigs sample 1x on tick 1 
                                after the row on which the second retrig occurs (!)
                                E92 followed by E93: no retrig 

                                ** speed 3: **
                                E90: retrigs sample 2x

                                A lone E9x command with x < speed retrigs the sample

                                E96
                                E96
                                E96
                                E96
                                E96
                                E96 <-- retrig here on tick 1

                                E97
                                (..)
                                E95
                                (..)
                                E94
                                (..)
                                E98
                                (..)
                                E96
                                (..)
                                E96 <-- retrig sample here on tick 1

                                Bottom line:
                                - when the effect occurs on the row:
                                    --> set retriglimit to argument
                                - increase retrig tick counter on all ticks 
                                  except tick 0 (the row)
                                - if retrig tick counter >= argument then:
                                    - reset retrig tick counter
                                    - retrig sample
                                - a new note action resets the retrig tick 
                                  counter
                                - an instrument change without any note 
                                  doesn't

                                ***********************************************

                                FT2 ignores the effect if the argument is 
                                greater than or equal to the nr of ticks per 
                                row
                            */
                            if ( channel.pSample ) {
                                if ( ft2StyleEffects_ && (argument >= tempo) ) break;
                                channel.retrigCount++;
                                if ( channel.retrigCount >= argument ) {
                                    channel.retrigCount = 0;
                                    playSample( iChannel,
                                        channel.pSample,
                                        channel.sampleOffset,
                                        FORWARD );
                                }
                            }
                            break;
                        }
                        case NOTE_CUT : 
                        {
                            //std::cout << "vv"; // DEBUG
                            if (tick >= argument) {
                                channel.volume = 0;
                                note.effects[fxloop].effect   = 0;
                                note.effects[fxloop].argument = 0;
                            }
                            break;
                        }
                        case NOTE_DELAY : 
                        {                                    
                            if (channel.delayCount <= tick) {
                                // valid sample for replay ? 
                                //  -> replay sample if new note
                                if (channel.pSample) { 
                                    playSample(iChannel, 
                                                channel.pSample, 
                                                channel.sampleOffset, 
                                                FORWARD);
                                    setFrequency(iChannel, 
                                        periodToFrequency(
                                            noteToPeriod(
                                                channel.lastNote + 
                                                    channel.pSample->getRelativeNote(), 
                                                channel.pSample->getFinetune())
                                        )
                                    );
                                }
                                note.effects[fxloop].effect   = 0; 
                                note.effects[fxloop].argument = 0; 
                            }                                   
                            break;
                        }
                        case INVERT_LOOP: 
                        {
                            break;
                        }
                    }
                    break;
                }
                case GLOBAL_VOLUME_SLIDE:
                {
                    unsigned    arg = channel.lastGlobalVolumeSlide;
                    unsigned    slide = (arg & 0xF0) >> 4;                      
                    if ( slide ) { // slide up
                        globalVolume_ += slide;
                        if (globalVolume_ > MAX_VOLUME) 
                            globalVolume_ = MAX_VOLUME;
                    } else {     // slide down
                        slide = arg & 0x0F;
                        if (slide > globalVolume_) 
                                globalVolume_ = 0;
                        else globalVolume_ -= slide;
                    }
                    break;
                }
                case PANNING_SLIDE:
                {
                    unsigned    panning = channel.panning;
                    unsigned    arg = channel.lastPanningSlide;
                    unsigned    slide = (arg & 0xF0) >> 4;                      
                    if ( slide ) { // slide up
                        panning += slide;
                        if (panning > PANNING_FULL_RIGHT) 
                            panning = PANNING_FULL_RIGHT;
                    } else {     // slide down
                        slide = arg & 0x0F;
                        if  (slide > panning ) 
                                panning = PANNING_FULL_LEFT;
                        else panning -= slide;
                    }
                    channel.panning = panning;
                    break;
                }
                case MULTI_NOTE_RETRIG: /* R + volume change + interval */   
                {  
                    if ( channel.pSample ) {
                        channel.retrigCount++;
                        /*
                        if ( (argument == 0) && 
                            (channel.oldNote.effects[fxloop].effect == MULTI_NOTE_RETRIG) )
                            argument = channel.lastMultiNoteRetrig;
                        */
                        if ( channel.retrigCount >= 
                            (channel.lastMultiNoteRetrig & 0xF) ) {
                            channel.retrigCount = 0;                             
                            int v = channel.volume;
                            switch ( argument >> 4 ) {
                                case  1: { v--;             break; }
                                case  2: { v -= 2;          break; }
                                case  3: { v -= 4;          break; }
                                case  4: { v -= 8;          break; }
                                case  5: { v -= 16;         break; }
                                case  6: { v <<= 1; v /= 3; break; }
                                case  7: { v >>= 1;         break; }
                                case  9: { v++;             break; }
                                case 10: { v += 2;          break; }
                                case 11: { v += 4;          break; }
                                case 12: { v += 8;          break; }
                                case 13: { v += 16;         break; }
                                case 14: { v *= 3; v >>= 1; break; }
                                case 15: { v <<= 1;         break; }
                            }
                            if ( v < 0 )          channel.volume = 0;
                            if ( v > MAX_VOLUME ) channel.volume = MAX_VOLUME;
                            playSample(
                                iChannel, 
                                channel.pSample, 
                                channel.sampleOffset, 
                                FORWARD );
                        }
                    }
                    break;
                }
                case TREMOR:
                {
                    break;
                }
            }
        }
    }
    return 0;
}

int Mixer::updateImmediateEffects () {
    for (unsigned iChannel = 0; iChannel < nChannels; iChannel++) {
        Channel&     channel = channels[iChannel];
        for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMNS; fxloop++) {
            Note&       note = channel.newNote;
            unsigned    effect   = note.effects[fxloop].effect;
            unsigned    argument = note.effects[fxloop].argument;

            switch ( effect ) {
                case VOLUME_SLIDE:
                case TONE_PORTAMENTO_AND_VOLUME_SLIDE:
                case VIBRATO_AND_VOLUME_SLIDE:
                {
                    /*
                        So, apparently:
                        D01 = volume slide down by 1
                        D10 = volume slide up   by 1
                        DF0 = volume slide up   by F
                        D0F = volume slide down by F
                        D82 = volume slide down by 2      (!)
                        DF1 = fine volume slide down by 1
                        D1F = fine volume slide up   by 1
                        DFF = fine volume slide up   by F (!)
                    */
                    if ( st3StyleEffectMemory_ )
                    {
                        unsigned& lastSlide = channel.lastVolumeSlide;
                        unsigned slide1 = lastSlide >> 4;
                        unsigned slide2 = lastSlide & 0xF;
                        unsigned& v = channel.volume;
                        if ( slide2 == 0xF )
                        {
                            v += slide1;
                            if ( v > MAX_VOLUME ) v = MAX_VOLUME;
                        } else if (slide1 == 0xF) {
                            if ( slide2 > v ) v = 0;
                            else              v -= slide2;
                        } 
                    }
                    break;
                }
                case EXTENDED_EFFECTS :
                {
                    effect = argument >> 4;
                    argument &= 0xF;
                    switch (effect) {
                        case FINE_PORTAMENTO_UP :
                        {
                            argument = channel.lastFinePortamentoUp;
                            argument <<= 2;
                            if ( argument < channel.period )
                                channel.period -= argument;
                            else channel.period = module->getMinPeriod();
                            if ( channel.period < module->getMinPeriod() )
                                channel.period = module->getMinPeriod();
                            setFrequency( iChannel,
                                periodToFrequency( channel.period ) );                                    
                            //std::cout << " F ^ by " << channel.lastFinePortamentoUp;
                            break;
                        }
                        case FINE_PORTAMENTO_DOWN :
                        {
                            argument = channel.lastFinePortamentoDown;
                            argument <<= 2;
                            channel.period += argument;
                            if ( channel.period > module->getMaxPeriod() )
                                channel.period = module->getMaxPeriod();
                            setFrequency( iChannel,
                                periodToFrequency( channel.period ) );
                            //std::cout << " F v by " << channel.lastFinePortamentoDown;
                            break;
                        }
                        case FINE_VOLUME_SLIDE_UP :
                        {
                            if ( argument ) 
                                channel.lastFineVolumeSlideUp = argument;
                            channel.volume += 
                                channel.lastFineVolumeSlideUp;
                            if ( channel.volume > MAX_VOLUME ) 
                                channel.volume = MAX_VOLUME;
                            break;
                        }
                        case FINE_VOLUME_SLIDE_DOWN :
                        {
                            if ( argument ) 
                                channel.lastFineVolumeSlideDown = argument;
                            if ( channel.lastFineVolumeSlideDown >= 
                                channel.volume ) 
                                    channel.volume = 0;
                            else channel.volume -= 
                                    channel.lastFineVolumeSlideDown;
                            break;
                        }
                    }
                    break;
                }
                case EXTRA_FINE_PORTAMENTO :
                {
                    effect = argument >> 4;
                    argument &= 0xF;
                    switch ( effect ) {
                        case EXTRA_FINE_PORTAMENTO_UP : // increase pitch, decrease period
                        {
                            argument = channel.lastExtraFinePortamentoUp;
                            if ( argument < channel.period )
                                channel.period -= argument;
                            else channel.period = module->getMinPeriod();
                            if ( channel.period < module->getMinPeriod() )
                                channel.period = module->getMinPeriod();
                            setFrequency( iChannel,
                                periodToFrequency( channel.period ) );
                            //std::cout << " XF ^ by " << channel.lastExtraFinePortamentoUp;
                            break;
                        }
                        case EXTRA_FINE_PORTAMENTO_DOWN :
                        {
                            argument = channel.lastExtraFinePortamentoDown;
                            channel.period += argument;
                            if ( channel.period > module->getMaxPeriod() )
                                channel.period = module->getMaxPeriod();
                            setFrequency( iChannel,
                                periodToFrequency( channel.period ) );
                            //std::cout << " XF v by " << channel.lastExtraFinePortamentoDown;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
    return 0;
}

int Mixer::setVolumes () {
    for (unsigned iChannel = 0; iChannel < nChannels; iChannel++) {
        setMixerVolume(iChannel);
    }
    return 0;
}

int Mixer::updateBpm () {
    tick++;
    if ( tick < tempo ) { 
        updateEffects ();         
    } else { 
        tick = 0;
        if ( !patternDelay_ ) updateNotes();
        else                  patternDelay_--;
        updateImmediateEffects();


        /*
        if ( patternDelay_ ) { 
            patternDelay_--; 
        } else {
            updateNotes();
        }
        tick = 0;
        updateImmediateEffects();
        */

        /*
        tick = 0; 
        updateNotes (); 
        updateImmediateEffects ();
        */
    }
    setVolumes ();
    return 0;
}

// ****************************************************************************
// ****************************************************************************
// ******* Start of Benchmarking code *****************************************
// ****************************************************************************
// ****************************************************************************



enum TimerToUseType { ttuUnknown,ttuHiRes,ttuClock };
TimerToUseType TimerToUse = ttuUnknown;
LARGE_INTEGER PerfFreq;     // ticks per second
int PerfFreqAdjust;         // in case Freq is too big
int OverheadTicks;          // overhead  in calling timer

void DunselFunction() { return; }

void DetermineTimer()
{
    void( *pFunc )() = DunselFunction;

    // Assume the worst
    TimerToUse = ttuClock;
    if ( QueryPerformanceFrequency( &PerfFreq ) )
    {
        // We can use hires timer, determine overhead
        TimerToUse = ttuHiRes;
        OverheadTicks = 200;
        for ( int i = 0; i < 20; i++ )
        {
            LARGE_INTEGER b,e;
            int Ticks;
            QueryPerformanceCounter( &b );
            (*pFunc)();
            QueryPerformanceCounter( &e );
            Ticks = e.LowPart - b.LowPart;
            if ( Ticks >= 0 && Ticks < OverheadTicks )
                OverheadTicks = Ticks;
        }
        // See if Freq fits in 32 bits; if not lose some precision
        PerfFreqAdjust = 0;
        int High32 = PerfFreq.HighPart;
        while ( High32 )
        {
            High32 >>= 1;
            PerfFreqAdjust++;
        }
    }
    return;
}

//double DoBench( void( *funcp )() )
double DoBench( Mixer &mixer )
{
    double time;      /* Elapsed time */

                      // Let any other stuff happen before we start
    MSG msg;
    PeekMessage( &msg,NULL,NULL,NULL,PM_NOREMOVE );
    Sleep( 0 );

    if ( TimerToUse == ttuUnknown )
        DetermineTimer();

    if ( TimerToUse == ttuHiRes )
    {
        LARGE_INTEGER tStart,tStop;
        LARGE_INTEGER Freq = PerfFreq;
        int Oht = OverheadTicks;
        int ReduceMag = 0;
        SetThreadPriority( GetCurrentThread(),
            THREAD_PRIORITY_TIME_CRITICAL );
        QueryPerformanceCounter( &tStart );

        //(*funcp)();   //call the actual function being timed

        mixer.doMixBuffer( mixer.waveBuffers[0] );
        for ( int bench = 0; bench < BENCHMARK_REPEAT_ACTION - 1; bench++ )
        {
            mixer.resetSong();  // does not reset everything?
            //std::cout << mixer.channels[0].
            mixer.doMixBuffer( mixer.waveBuffers[0] );
        }


        QueryPerformanceCounter( &tStop );
        SetThreadPriority( GetCurrentThread(),THREAD_PRIORITY_NORMAL );
        // Results are 64 bits but we only do 32
        unsigned int High32 = tStop.HighPart - tStart.HighPart;
        while ( High32 )
        {
            High32 >>= 1;
            ReduceMag++;
        }
        if ( PerfFreqAdjust || ReduceMag )
        {
            if ( PerfFreqAdjust > ReduceMag )
                ReduceMag = PerfFreqAdjust;
            tStart.QuadPart = Int64ShrlMod32( tStart.QuadPart,ReduceMag );
            tStop.QuadPart = Int64ShrlMod32( tStop.QuadPart,ReduceMag );
            Freq.QuadPart = Int64ShrlMod32( Freq.QuadPart,ReduceMag );
            Oht >>= ReduceMag;
        }

        // Reduced numbers to 32 bits, now can do the math
        if ( Freq.LowPart == 0 )
            time = 0.0;
        else
            time = ((double)(tStop.LowPart - tStart.LowPart
                - Oht)) / Freq.LowPart;
    } else
    {
        long stime,etime;
        SetThreadPriority( GetCurrentThread(),
            THREAD_PRIORITY_TIME_CRITICAL );
        stime = clock();
        //(*funcp)();
        mixer.startReplay();
        etime = clock();
        SetThreadPriority( GetCurrentThread(),THREAD_PRIORITY_NORMAL );
        time = ((double)(etime - stime)) / CLOCKS_PER_SEC;
    }

    return (time);
}

void startReplay( Mixer &mixer ) {
    mixer.startReplay();
}

// ****************************************************************************
// ****************************************************************************
// ******* End of benchmarking code *******************************************
// ****************************************************************************
// ****************************************************************************

/*
1 pixel = 1 tick, ft2 envelope window width == 6 sec
vibrato is active even if envelope is not
vibrato sweep: amount of ticks before vibrato reaches max. amplitude
*/

int main(int argc, char *argv[])  { 
    std::vector< std::string > filePaths;
    char        *modPaths[] = {
        
        //"D:\\MODS\\M2W_BUGTEST\\cd2part2b.mod",
        //"D:\\MODS\\M2W_BUGTEST\\women2.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\menutune3.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\ssi2.S3m",
        //"D:\\MODS\\M2W_BUGTEST\\menutune3.xm",
        //"D:\\MODS\\M2W_BUGTEST\\pullmax-portatest.xm",
        //"D:\\MODS\\M2W_BUGTEST\\appeal.mod",
        //"D:\\MODS\\M2W_BUGTEST\\againstptnloop.MOD",
        //"D:\\MODS\\M2W_BUGTEST\\againstptnloop.xm",
        //"D:\\MODS\\MOD\\hoffman_and_daytripper_-_professional_tracker.mod",
        //"D:\\MODS\\S3M\\Purple Motion\\inside.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\WORLD-vals.S3M",
        //"D:\\MODS\\M2W_BUGTEST\\WORLD-vals.xm",
        //"D:\\MODS\\M2W_BUGTEST\\2nd_pm-porta.s3m",

        //"D:\\MODS\\dosprog\\mods\\demotune.mod", // xm = wrong, ptn loop tester
        //"D:\\MODS\\dosprog\\ode2pro.MOD",
        //"D:\\MODS\\M2W_BUGTEST\\alf_-_no-mercy-SampleOffsetBug.mod",
        //"D:\\MODS\\M2W_BUGTEST\\against-retrigtest.s3m",
        //"D:\\MODS\\S3M\\Purple Motion\\zak.s3m",
        //"D:\\MODS\\dosprog\\audiopls\\YEO.MOD",
        //"D:\\MODS\\dosprog\\mods\\over2bg.xm",
        //"D:\\MODS\\M2W_BUGTEST\\resolution-loader-corrupts-sample-data.xm",
        //"D:\\MODS\\M2W_BUGTEST\\resolution-loader-corrupts-sample-data2.mod",
        //"D:\\MODS\\M2W_BUGTEST\\believe.mod",
        //"D:\\MODS\\M2W_BUGTEST\\believe-wrong notes.mod",
        //"D:\\MODS\\M2W_BUGTEST\\ParamMemory2.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\global trash 3 v2-songrepeat-error.mod",
        //"D:\\MODS\\M2W_BUGTEST\\CHINA1.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\Creagaia.it",   // impulse tracker unknown
        
        //"D:\\MODS\\M2W_BUGTEST\\Crystals.wow",
        "D:\\MODS\\M2W_BUGTEST\\Crea2.it",      // impulse tracker v1.6
        "D:\\MODS\\M2W_BUGTEST\\Crea.it",       // impulse tracker v2.0+
        "D:\\MODS\\M2W_BUGTEST\\finalreality-credits.it",
        
        //"D:\\MODS\\dosprog\\mods\\pullmax.xm",
        //"D:\\MODS\\mod_to_wav\\CHINA1.MOD",
        //"D:\\MODS\\MOD\\Jogeir Liljedahl\\slow-motion.mod",
        //"D:\\MODS\\M2W_BUGTEST\\slow-motion-pos15-porta.mod",
        //"D:\\MODS\\dosprog\\MUSIC\\S3M\\2nd_pm.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\sundance-fantomnotes.mod",
        //"D:\\MODS\\M2W_BUGTEST\\vibtest.mod",
        //"D:\\MODS\\M2W_BUGTEST\\menutune4.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\ssi.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\menutune2.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\algrhyth2.mod",
        //"D:\\MODS\\M2W_BUGTEST\\algrhyth2.s3m",
        //"D:\\MODS\\dosprog\\audiopls\\ALGRHYTH.MOD",
        //"D:\\MODS\\dosprog\\mods\\probmod\\nowwhat3.mod",
        //"D:\\MODS\\dosprog\\mods\\probmod\\xenolog1.mod",
        //"D:\\MODS\\dosprog\\mods\\menutune.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\ssi.s3m",
        "D:\\MODS\\mod_to_wav\\XM JL\\BIZARE.XM",
        //"D:\\MODS\\S3M\\Karsten Koch\\aryx.s3m",
        "D:\\MODS\\dosprog\\mods\\women.s3m",
        //"D:\\MODS\\dosprog\\backward.s3m",
        //"D:\\MODS\\dosprog\\audiopls\\ALGRHYTH.MOD",
        //"D:\\MODS\\dosprog\\mods\\starsmuz.xm",
        //"c:\\Users\\Erland-i5\\desktop\\morning.mod",
        //"D:\\MODS\\dosprog\\china1-okt.s3m",
        //"D:\\MODS\\dosprog\\2nd_pm.xm",
        "D:\\MODS\\dosprog\\stardstm.mod",
        //"D:\\MODS\\dosprog\\lchina.s3m",
        //"D:\\MODS\\dosprog\\mods\\againstr.s3m",
        //"D:\\MODS\\dosprog\\mods\\againstr.mod",
        //"D:\\MODS\\dosprog\\mods\\bluishbg2.xm",
        "D:\\MODS\\dosprog\\mods\\un-land2.s3m",
        "D:\\MODS\\dosprog\\mods\\un-land.s3m",
        "D:\\MODS\\dosprog\\mods\\un-vectr.s3m",
        "D:\\MODS\\dosprog\\mods\\un-worm.s3m",
        "D:\\MODS\\dosprog\\chipmod\\mental.mod",
        "D:\\MODS\\dosprog\\mods\\theend.mod",
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\jz-scpsm2.xm",
        //"D:\\MODS\\dosprog\\music\\xm\\united_7.xm",
        //"D:\\MODS\\dosprog\\ctstoast.xm",
        //"D:\\MODS\\dosprog\\mods\\probmod\\xenolog1.mod",
        "C:\\Users\\Erland-i5\\Desktop\\mods\\mech8.s3m",
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\jazz1\\Tubelectric.S3M",
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\jazz1\\bonus.S3M",        
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\Silverball\\fantasy.s3m",
        //"D:\\MODS\\dosprog\\women.xm",
        /*
        "D:\\MODS\\dosprog\\mods\\menutune.s3m",
        "D:\\MODS\\dosprog\\mods\\track1.s3m",
        "D:\\MODS\\dosprog\\mods\\track2.s3m",
        "D:\\MODS\\dosprog\\mods\\track3.s3m",
        "D:\\MODS\\dosprog\\mods\\track4.s3m",
        "D:\\MODS\\dosprog\\mods\\track5.s3m",
        "D:\\MODS\\dosprog\\mods\\track6.s3m",
        "D:\\MODS\\dosprog\\mods\\track7.s3m",
        "D:\\MODS\\dosprog\\mods\\track8.s3m",
        "D:\\MODS\\dosprog\\mods\\track9.s3m",
        */
        //"D:\\MODS\\dosprog\\mods\\ssi.s3m",
        //"D:\\MODS\\dosprog\\mods\\ssi.xm",
        "D:\\MODS\\dosprog\\mods\\pori.s3m",
        "D:\\MODS\\dosprog\\mods\\tearhate.s3m",
        "D:\\MODS\\dosprog\\mods\\starsmuz.s3m",
        
        "D:\\MODS\\MOD\\beastsong.mod",
        //"D:\\MODS\\dosprog\\mods\\over2bg.xm",
        //"D:\\MODS\\dosprog\\chipmod\\mental.mod",
        //"D:\\MODS\\dosprog\\mods\\probmod\\chipmod\\mental.xm",
        "D:\\MODS\\dosprog\\mods\\probmod\\chipmod\\MENTALbidi.xm",
        "D:\\MODS\\dosprog\\mods\\baska.mod",
        "D:\\MODS\\dosprog\\chipmod\\mental.mod",
        "D:\\MODS\\dosprog\\dope.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\bp7\\bin\\exe\\cd2part2.mod",
//        "D:\\MODS\\dosprog\\audiopls\\crmx-trm.mod",
        "D:\\MODS\\dosprog\\ctstoast.xm",
        "D:\\MODS\\dosprog\\dope.mod",
//        "D:\\MODS\\dosprog\\smokeoutstripped.xm",
        "D:\\MODS\\dosprog\\smokeout.xm",
        "D:\\MODS\\dosprog\\KNGDMSKY.XM",
        "D:\\MODS\\dosprog\\KNGDMSKY-mpt.XM",
        "D:\\MODS\\dosprog\\myrieh.xm",
        "D:\\MODS\\dosprog\\chipmod\\mental.mod",
        "D:\\MODS\\dosprog\\chipmod\\crain.mod",
        "D:\\MODS\\dosprog\\chipmod\\toybox.mod",
        "D:\\MODS\\dosprog\\chipmod\\etanol.mod",
        "D:\\MODS\\dosprog\\chipmod\\sac09.mod",
        "D:\\MODS\\dosprog\\chipmod\\1.mod",
        "D:\\MODS\\dosprog\\chipmod\\bbobble.mod",
        "D:\\MODS\\dosprog\\chipmod\\asm94.mod",
        "D:\\MODS\\dosprog\\chipmod\\4ma.mod",
        "D:\\MODS\\dosprog\\chipmod\\mental.mod",
        "D:\\MODS\\dosprog\\mods\\over2bg.xm",
        "D:\\MODS\\dosprog\\mods\\explorat.xm",
        "D:\\MODS\\dosprog\\mods\\devlpr94.xm",
        "D:\\MODS\\dosprog\\mods\\bj-eyes.xm",
        "D:\\MODS\\dosprog\\mods\\1993.mod",
        "D:\\MODS\\dosprog\\mods\\1993.xm",
        "D:\\MODS\\dosprog\\mods\\baska.mod",
        "D:\\MODS\\dosprog\\mods\\bj-love.xm",
//        "D:\\MODS\\dosprog\\mods\\probmod\\3demon.mod",
        "D:\\MODS\\dosprog\\mods\\probmod\\veena.wow",
        "D:\\MODS\\dosprog\\mods\\probmod\\flt8_1.mod",
        nullptr
    };
    /*
    int positive_val = 199;
    int negative_val = -199;
    std::cout << (positive_val / 2) << std::endl; // rondt af naar beneden
    std::cout << (positive_val >> 1) << std::endl; // rondt af naar beneden

    std::cout << (negative_val / 2) << std::endl; // rondt af naar BOVEN!
    std::cout << (negative_val >> 1) << std::endl; // rondt af naar beneden
    std::cout << std::endl
        << "finetunes: " << std::endl;
    for ( int i = 0; i < 16; i++ ) {
        std::cout << "Fine tune for i = " << std::setw(2) << i << ": " 
            << std::setw( 2 )
            << (int)((signed char)( i > 7 ? (i | 0xF0) : i )) << std::endl;
    }
    std::cout << std::endl;
    _getch();
    */



    if (argc > 1) {
        for ( int i = 1; i < argc; i++ ) filePaths.push_back( argv[i] );
    } else {
        if ((!strcmp(argv[0], 
            "C:\\Users\\Erland-i5\\Documents\\Visual Studio 2015\\Projects\\Mod_to_WAV\\Debug\\Mod_to_WAV.exe")) ||
            (!strcmp(argv[0], 
            "C:\\Users\\Erland-i5\\Documents\\Visual Studio 2015\\Projects\\Mod_to_WAV\\Release\\Mod_to_WAV.exe")) ||
            (!strcmp(argv[0], 
            "C:\\Dev-Cpp\\Projects\\Mod2Wav.exe"))) {                                                    
            for ( int i = 0; modPaths[i] !=nullptr; i++ ) filePaths.push_back( modPaths[i] );
        } else {
            unsigned    slen = strlen(argv[0]);
            char        *exeName = (argv[0] + slen - 1);

            while (slen && (*exeName != '\\')) { slen--; exeName--; }
            exeName++;
            std::cout << "\n\nUsage: ";
            std::cout << exeName << " + <modfile.mod>\n\n";
            _getch();
            return 0;
        }
    }
    


    for (unsigned i = 0; i < filePaths.size(); i++) {
        Module      sourceFile( filePaths[i] );
        Mixer       mixer;

        std::cout << "\n\nLoading " << filePaths[i].c_str()//moduleFilename
                  << ": " << (sourceFile.isLoaded() ? "Success." : "Error!") << std::endl;

        if (sourceFile.isLoaded ()) {
            unsigned s = BUFFER_SIZE / (MIXRATE * 2); // * 2 for stereo
            std::cout << "\nCompiling module \"" 
                      << sourceFile.getSongTitle().c_str()  
                      << "\" into " << (s / 60) << "m "
                      << (s % 60) << "s of 16 bit WAVE data"  << std::endl 
                      << "Hit any key to start mixing." << std::endl;

            // show instruments
            for ( unsigned i = 1; i <= sourceFile.getnInstruments(); i++ )
            {
                std::cout 
                    << i << ":" 
                    << sourceFile.getInstrument( i )->getName().c_str() 
                    << std::endl;
            }


            _getch();
            mixer.initialise( &sourceFile );          
            double benchTime = 0.0L;
            benchTime = DoBench( mixer ) / BENCHMARK_REPEAT_ACTION; // only for staging

            mixer.startReplay();          

            std::cout << "A " << sourceFile.getnChannels() << " channel module was rendered to " 
                      << BUFFER_LENGTH_IN_MINUTES << " min of wave data in " << benchTime << " seconds." << std::endl
                      << "Estimated realtime cpu charge is " 
                      << (benchTime * 100) / (BUFFER_LENGTH_IN_MINUTES * 60) << " percent." << std::endl
                      << "On average " << (benchTime * 1000.0 / sourceFile.getnChannels()) / BUFFER_LENGTH_IN_MINUTES << " milliseconds per channel per minute." << std::endl
                      << "\nPlaying... Hit any key to stop.";
            _getch();  
            mixer.stopReplay();
        }
        std::cout << "\nHit any key to load next / exit...";
        _getch();  
    }
/*
    std::cout << "\nHit any key to start memory leak test.";
    _getch();
    for (int i = 0; i < 40; i++) { 
        Module sourceFile;
        sourceFile.setFileName(modPaths[3]);
        sourceFile.loadFile();
        std::cout << "\nisLoaded = " << ((sourceFile.isLoaded ()) ? "Yes" : "No");
        std::cout << ", for the " << (i + 1) << "th time";
    }
    std::cout << "\nHit any key to exit program.";
    _getch();
/**/
    _getch();
	return 0;
}

   
