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
#include "time.h"

#include "Module.h"

//extern const char *noteStrings[2 + MAXIMUM_NOTES];

const char *noteStrings[2 + MAXIMUM_NOTES] = { "---",
     "C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0",
     "C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1",
     "C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2",
     "C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3",
     "C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4",
     "C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5",
     "C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6",
     "C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7",
     "off"};

#define BUFFER_LENGTH_IN_MINUTES   4
#define BENCHMARK_REPEAT_ACTION    4

//#define debug_mixer

#define LINEAR_INTERPOLATION  // TEMP

#define BUFFER_SIZE (44100 * 2 * 60 * BUFFER_LENGTH_IN_MINUTES) // 0x4000 // temp bloated buf for easy progging
#define FILTER_SQUARE 0
#define FILTER_LINEAR 1
#define FILTER_CUBIC 2
#define PLAYER_MAX_CHANNELS 64
#define MIXER_MAX_CHANNELS 256
#define WAVE_BUFFER_NO 1 // temp, should be at least 4
#define VOLUME_RAMP_SPEED 41
#define MAX_NOTES_PER_CHANNEL 16
#define MIXRATE 44100

typedef int MixBufferType;

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


    /*
    effect memory:
    */
    unsigned        sampleOffset;
    unsigned        vibratoWave;
    unsigned        tremoloWave;
    unsigned        retrigCount;
    unsigned        delayCount;
    unsigned        arpeggioCount;
    unsigned        arpeggioNote1;
    unsigned        arpeggioNote2;
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
    unsigned        lastMultiNoteRetrig;
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
    unsigned long long  sampleOffset_; // changed
    // added:
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
    bool            isInitialised;
    Module          *module;
    MixBufferType   *mixBuffer;
    Channel         *channels[PLAYER_MAX_CHANNELS];
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
    unsigned        periodToFrequency(unsigned period);
//    int             setVolume   (unsigned fromChannel, unsigned volume);  // range: 0..64
//    int             setPanning  (unsigned fromChannel, unsigned panning); // range: 0..255: extr left... extr right
    int             setMixerVolume(unsigned fromChannel);
    int             setFrequency(unsigned fromChannel, unsigned frequency);
    int             playSample  (unsigned fromChannel, Sample *sample, unsigned offset, bool direction);
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
    for ( unsigned i = 0; i < MIXER_MAX_CHANNELS; i++ ) {
        mixerChannels[i].init();
    }
    for ( unsigned i = 0; i < nChannels; i++ ) {
        channels[i]->init();
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
    }
    globalPanning_ = 0x2F;  // 0 means extreme LEFT & RIGHT, so no attenuation
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
    memset(mixBuffer, 0, sizeof(MixBufferType[BUFFER_SIZE]));

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

    for (unsigned i = 0; i < nChannels; i++) {
        channels[i] = new Channel;
        /*
        switch (module->getPanningStyle()) {
            case PANNING_STYLE_MOD :
                {
                    switch(i % 4) {
                        case 1 : 
                        case 2 : { channels[i]->panning = PANNING_FULL_RIGHT; break; }
                        case 0 : 
                        case 3 : { channels[i]->panning = PANNING_FULL_LEFT;  break; }          
                    }
                    break;
                }
            case PANNING_STYLE_S3M :
                {
                    if (i & 1)  channels[i]->panning = PANNING_FULL_RIGHT;
                    else        channels[i]->panning = PANNING_FULL_LEFT;
                    break;
                }
            case PANNING_STYLE_XM :
            default : 
                {
                    channels[i]->panning = PANNING_CENTER;
                    break;
                }
        }
        */
    }
    /*
    globalPanning_ = 0x2F;  // 0 means extreme LEFT & RIGHT, so no attenuation
    globalVolume_ = 64;
    tempo = module->getDefaultTempo();
    bpm = module->getDefaultBpm();
    setBpm ();
    gain = 64;//128; // max = 256
    */
    resetSong();


    isInitialised = true;
    return 0;
}

int Mixer::doMixBuffer (SHORT *buffer) {
    unsigned    x, y;

    memset(mixBuffer, 0, BUFFER_SIZE * sizeof(MixBufferType));
    mixIndex = 0;
    x = callBpm - mixCount;
    y = BUFFER_SIZE / 2;   // stereo
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
/*
 mixbuffer is now 64 bit
*/
    // transfer sampled data from ?? bit buffer into 16 bit buffer
    saturation = 0;
    /*
    for (unsigned i = 0; i < BUFFER_SIZE; i++) {
        MixBufferType tmp = mixBuffer[i] >> (6 + 6 + 8); // globalvolume + volume + gain
        if (tmp < -32768) { tmp = -32768; saturation++; }       
        if (tmp >  32767) { tmp =  32767; saturation++; }
        buffer[i] = (SHORT)tmp;
//        if ((i > 40000) && (i < 41000)) std::cout << " " << tmp; // DEBUG
    }
    */

    
    MixBufferType *src = mixBuffer;
    SHORT *dst = buffer;
    for ( unsigned i = 0; i < BUFFER_SIZE; i++ ) {
        //int tmp = (int)((*src++) >> (6 + 6 + 8)); // globalvolume + volume + gain
        int tmp = (int)(*src++);
        if ( tmp < -32768 ) { tmp = -32768; saturation++; }
        //else 
            if ( tmp >  32767 ) { tmp = 32767; saturation++; }
        *dst++ = (SHORT)tmp;
    }
    


    std::cout << "\n\nSaturation = " << saturation << "\n"; // DEBUG
    return 0;
}

int Mixer::doMixSixteenbitStereo(unsigned nSamples) {
    //std::cout << nSamples << std::endl; // debug


    nActiveMixChannels = 0;
/* debug:        
    std::cout << "\n";
    for (unsigned i = 0; i < nChannels; i++) {
        std::cout << "\nChannel ";
        if (i < 10) std::cout << " ";
        std::cout << i << ": ";
        for (unsigned j = 0; j < MAX_SAMPLES; j++) {
            unsigned k = channels[i]->mixerChannelsTable[j];
            if (k < 10) std::cout << " ";
            std::cout << k << " ";
        }
    }
    _getch();

// debug end 
    std::cout << "\n";
    for (unsigned i = 0; i < nChannels; i++) {
        unsigned v = channels[i]->volume;
        if (v < 10) std::cout << " ";
        std::cout << v << " ";
    }
    if(iPatternTable == 6) _getch();
// debug end */

    for (unsigned i = 0; i < MIXER_MAX_CHANNELS; i++) {
        MixerChannel *chn = &mixerChannels[i];
        if (chn->isActive) {
            Sample      *smp = chn->sample;
            unsigned    mixOffset = mixIndex;
            int         leftGain  = (gain * chn->leftVolume) >> 6;   // added >> 6
            int         rightGain = (gain * chn->rightVolume) >> 6;  // added >> 6
            unsigned    iSampleData = chn->sampleOffset; //(unsigned)(chn->sampleOffset >> 16);

            chn->age++;
            nActiveMixChannels++;

            // quick hack for first tests
            if (chn->isFadingOut) { 
                chn->isActive = false;
                //chn->isVolumeRampActive = true;
            }

            if (!chn->isVolumeRampActive 
              //   && (i == 1) 
                ) {

                //std::cout << chn->sampleIncrement << std::endl; // debug

                chn->sampleOffsetFrac &= 0xFFFF;
                unsigned modulus = 0; // debug

                for (unsigned j = 0; j < nSamples; /* j++ */ ) {
                    if (chn->isPlayingBackwards) {

                        // no backwards sample playing right now
                        j = nSamples;
/* 
                        MixBufferType s1 = smp->getData()[iSampleData];
#ifdef LINEAR_INTERPOLATION
                        int s2 = smp->getData()[iSampleData - 1];
                        int xd = (0x10000 - (chn->sampleOffset & 0xFFFF)) >> 1;  // time delta
                        int yd = s2 - s1;                                        // sample delta
                        s1 += (xd * yd) >> 15;
#endif                        
                        mixBuffer[mixOffset++] += (s1 * leftGain) >> 14;
                        mixBuffer[mixOffset++] += (s1 * rightGain) >> 14;


                        if (chn->sampleOffset >= chn->sampleIncrement)
                            chn->sampleOffset -= chn->sampleIncrement;
                        else chn->sampleOffset &= 0xFFFF; 

                        iSampleData = (unsigned)(chn->sampleOffset >> 16);

                        
                        //chn->sampleOffsetFrac += chn->sampleIncrement;
                        //iSampleData = chn->sampleOffset + (chn->sampleOffsetFrac >> 16);
                        










                        if (iSampleData <= smp->getRepeatOffset()) {
                            if (smp->isRepeatSample()) {
                                chn->sampleOffset &= 0xFFFF;
                                if (smp->isPingpongSample()) {
                                    iSampleData = smp->getRepeatOffset() - 1; // ?
                                    chn->isPlayingBackwards = false;
                                } 
                                chn->sampleOffset += (long long)iSampleData << 16;
                            } else {
                                unsigned k;
                                chn->isActive = false;
                                chn->isPrimary = false;
                            
                                // remove reference to this mixer channel in the channels mixer channels table
                                for (k = 0; k < MAX_SAMPLES; k++) {
                                    if (channels[chn->fromChannel]->mixerChannelsTable[k] == i) break;
                                }
                                channels[chn->fromChannel]->mixerChannelsTable[k] = 0;
                                j = nSamples; // quit loop, we're done here
                            }
                        } else {

                        }
*/
                    } else { // if (chn->isPlayingBackwards)









                        unsigned sampleDataLeft = smp->getRepeatEnd() - iSampleData; // samples to mix before we need to process sample repeat logic
                        if ( sampleDataLeft > nSamples ) sampleDataLeft = nSamples;
                        unsigned nrLoops = ((((sampleDataLeft << 16) + chn->sampleIncrement - 1) - (chn->sampleOffsetFrac & 0xFFFF)) / chn->sampleIncrement);
                        if ( nrLoops >= nSamples - j ) nrLoops = nSamples - j;


                        MixBufferType *mixBufferPTR = mixBuffer + mixOffset;
                        for ( unsigned j2 = 0; j2 < nrLoops; j2++ ) {
                            MixBufferType s1 = smp->getData()[iSampleData];
#ifdef LINEAR_INTERPOLATION
                            int s2 = smp->getData()[iSampleData + 1];
                            int xd = (chn->sampleOffsetFrac & 0xFFFF) >> 1;  // time delta
                            int yd = s2 - s1;                                // sample delta
                            s1 += (xd * yd) >> 15;
#endif
                            //mixBuffer[mixOffset++] += (s1 * leftGain) >> 14;
                            //mixBuffer[mixOffset++] += (s1 * rightGain) >> 14;
                            *mixBufferPTR++ += (s1 * leftGain) >> 14;
                            *mixBufferPTR++ += (s1 * rightGain) >> 14;

                            chn->sampleOffsetFrac += chn->sampleIncrement;
                            iSampleData = chn->sampleOffset + (chn->sampleOffsetFrac >> 16);
                        }
                        j += nrLoops;
                        mixOffset += nrLoops << 1;









                        if (iSampleData >= smp->getRepeatEnd()) {
                            if (smp->isRepeatSample()) {
                                if (smp->isPingpongSample()) {
                                    iSampleData = smp->getRepeatEnd() + 1;    // ?
                                    chn->isPlayingBackwards = true;
                                } else {
                                    iSampleData = smp->getRepeatOffset();
                                }
                                chn->sampleOffset = iSampleData; 
                                chn->sampleOffsetFrac &= 0xFFFF;
                                modulus = 0; // debug
                            } else {                                
                                chn->isActive = false;
                                chn->isPrimary = false;
                            
                                // remove reference to this mixer channel in the channels mixer channels table
                                unsigned k;
                                for (k = 0; k < MAX_SAMPLES; k++) {
                                    if (channels[chn->fromChannel]->mixerChannelsTable[k] == i) break;
                                }
                                channels[chn->fromChannel]->mixerChannelsTable[k] = 0;
                                j = nSamples; // quit loop, we're done here
                            }
                        }
                    }
                }
                chn->sampleOffset = iSampleData;
            }
        }
    }
    mixIndex += (nSamples << 1); // stereo
//    if (activeChannels < 10) std::cout << " "; std::cout << activeChannels << "  ";
    return 0;
}

Mixer::~Mixer() {
    for (unsigned i = 0; i < PLAYER_MAX_CHANNELS; i++) delete channels[i];
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

//int Mixer::startReplay () {
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
/*
        return periods[note + 11] - (finetune >> 4);   // doesn't work either!!!!!
*/
        //finally. thank god for Benjamin Rousseaux!!!
        return (unsigned)(pow(2.0, (133.0 - ((double)note + (double)(finetune / 128.0))) / 12.0) * 13.375);
    }
}

unsigned Mixer::periodToFrequency(unsigned period) {
    if (module->useLinearFrequencies()) {
        return (unsigned)(8363.0 * pow(2, ((4608.0 - (double)period) / 768.0)));
    } else {
        //return (period ? (unsigned)(PAL_CALC / (period << 1)) : 0);
        return (period ? ((8363 * 1712) / period) : 0);
    }
}

int Mixer::setMixerVolume(unsigned fromChannel) {
    unsigned        gp = globalPanning_;
    bool            invchn = (gp >= PANNING_CENTER);
    unsigned        soften = (invchn ? (PANNING_FULL_RIGHT - gp) : gp);

    for (int i = 0; i < MAX_SAMPLES; i++) {
        unsigned    mc = channels[fromChannel]->mixerChannelsTable[i];
        if (mc) {
            MixerChannel    *pmc = &mixerChannels[mc];
            if (pmc->isActive) {
                unsigned        p = channels[fromChannel]->panning;
                unsigned        v = channels[fromChannel]->volume
                                        * globalVolume_;

                p = soften + ((p * (PANNING_MAX_STEPS - (soften << 1))) >> PANNING_SHIFT);
                if (invchn) p = PANNING_FULL_RIGHT - p;                 
                pmc->leftVolume  = ((PANNING_FULL_RIGHT - p) * v) >> PANNING_SHIFT; 
                pmc->rightVolume = ( p                       * v) >> PANNING_SHIFT; 
                if (balance_ < 0) {
                    pmc->rightVolume *= (100 + balance_);
                    pmc->rightVolume /= 100;
                }
                if (balance_ > 0) {
                    pmc->leftVolume  *= (100 - balance_);
                    pmc->leftVolume /= 100;
                }
            }
        }
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
    for (int i = 0; i < MAX_SAMPLES; i++) {
        unsigned    mc = channels[fromChannel]->mixerChannelsTable[i];
        if (mc) {
            MixerChannel    *pmc = &mixerChannels[mc];
            if (pmc->isPrimary) {
                /*
                double f;
                if (module->useLinearFrequencies()) {
                    f = 8363.0 * pow(2, ((4608.0 - (double)period) / 768.0));
                } else {
                    f = (period ? (PAL_CALC / (double)(period << 1)) : 0);
                } 
                */
                double f = ((double)frequency * 65536.0) / (double)MIXRATE;
                pmc->sampleIncrement = (unsigned) f;
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

int Mixer::playSample (unsigned fromChannel, Sample *sample, unsigned sampleOffset, bool direction) {
    unsigned    oldestSlot;
    unsigned    emptySlot;
    unsigned    age;
    unsigned    newMc;
    unsigned    mcIndex;
    bool        sampleStarted = false;

    // stop previous note in same logical Channel
    
    for (unsigned i = 0; i < MAX_NOTES_PER_CHANNEL; i++) {
        unsigned j = channels[fromChannel]->mixerChannelsTable[i]; 
        if (j) {
            if (mixerChannels[j].isPrimary && (mixerChannels[j].fromChannel == fromChannel)) {
                mixerChannels[j].isPrimary = false;
                channels[fromChannel]->mixerChannelsTable[i] = 0;
                // temp: 
                mixerChannels[j].isFadingOut = true;
                mixerChannels[j].isActive = false;
                break;
            }
        }
    }
    /*
    mixerChannels[channels[fromChannel]->iPrimary].isPrimary = false;
    mixerChannels[channels[fromChannel]->iPrimary].isActive = false;
    channels[fromChannel]->mixerChannelsTable[channels[fromChannel]->iPrimary] = 0;
    */
    // find an empty slot in mixer channels table
    for (emptySlot = 0; emptySlot < MAX_NOTES_PER_CHANNEL; emptySlot++) {
        if(!channels[fromChannel]->mixerChannelsTable[emptySlot]) break;
    }
    // None found, remove oldest sample (the longest playing one)
    // and use it's channel for the new sample
    if (emptySlot >= MAX_NOTES_PER_CHANNEL) {
        oldestSlot = 0;
        age = 0;
        for (unsigned i = 0; i < MAX_NOTES_PER_CHANNEL; i++) {
            mcIndex = channels[fromChannel]->mixerChannelsTable[i];
            if (mixerChannels[mcIndex].age > age) {
                age = mixerChannels[mcIndex].age;
                oldestSlot = i;
            }
        }
        emptySlot = oldestSlot;
        newMc = mcIndex;
    } else {
    // find a new channel for mixing
        newMc = 1; // mix channel 0 is never used
        while (newMc < MIXER_MAX_CHANNELS) {
            if (!mixerChannels[newMc].isActive) break;
            newMc++;
        }
    }

    if (newMc < MIXER_MAX_CHANNELS) { // should be unnecessary  (the check)
        mixerChannels[newMc].isActive = true;
        mixerChannels[newMc].isPrimary = true;
        mixerChannels[newMc].isPlayingBackwards = direction;
        mixerChannels[newMc].isFadingOut = false;
        mixerChannels[newMc].age = 0;
        mixerChannels[newMc].fromChannel = fromChannel;
        mixerChannels[newMc].sample = sample;
            // temp hack
        mixerChannels[newMc].isVolumeRampActive = false;
        //mixerChannels[newMc].sampleOffset = (long long)sampleOffset << 16; // removed
        mixerChannels[newMc].sampleOffset = sampleOffset; // added
        channels[fromChannel]->mixerChannelsTable[emptySlot] = newMc; 
        channels[fromChannel]->iPrimary = emptySlot;
    }
    return 0;
}

int Mixer::updateNotes () {
    bool            patternBreak = false;
    unsigned        patternStartRow = 0;
    int             nextPatternDelta = 1;

    if (patternDelay_) { patternDelay_--; return 0; }       

/*
    std::cout << "\n";
    for (int f = -127; f < -120; f++) {
        std::cout << "finetune " << f << "\n";
        for (unsigned n = 0; n < MAXIMUM_NOTES; n++) {
            unsigned r = noteToPeriod(n + 1, f);
            if (r < 100000000 ) std::cout << " ";
            if (r < 10000000  ) std::cout << " ";
            if (r < 1000000   ) std::cout << " ";
            if (r < 100000    ) std::cout << " ";
            if (r < 10000     ) std::cout << " ";
            if (r < 1000      ) std::cout << " ";
            if (r < 100       ) std::cout << " ";
            if (r < 10        ) std::cout << " ";
            std::cout << r << " ";
        }
        std::cout << "\n";
    }
    _getch();
*/
#ifdef debug_mixer
    char    hex[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned p = module->getPatternTable(iPatternTable);
    std::cout << "\n";
    if (iPatternTable < 10)    std::cout << " ";
    else                       std::cout << (iPatternTable / 10);
    std::cout << (iPatternTable % 10) << ":";
    if (p < 10) std::cout << " ";
    else        std::cout << (p / 10);
    std::cout << (p % 10) << "|";
#endif

    for (unsigned iChannel = 0; iChannel < nChannels; iChannel++) {
        Channel     *channel = channels[iChannel];
        unsigned    note, instrument, sample, effect, argument;
        bool        isNewNote;
        bool        isNewInstrument;
        bool        isValidInstrument;
        bool        isDifferentInstrument;
        bool        isNoteDelayed = false;
        bool        replay = false;
        bool        keyedOff = false;
        unsigned    oldInstrument;      
        int         finetune = 0;


        channel->sampleOffset = 0;
        /*
        channel->volume;
        channel->panning; 
        */

        note       = iNote->note;
        instrument = iNote->instrument;

        if (note) {
            channel->lastNote = note;
            if (note == KEY_OFF) {
                isNewNote = false;
                keyedOff = true;
            } else isNewNote = true;
        } else {
            isNewNote = false;
        }
        
        oldInstrument = channel->instrumentNo;
        if (instrument) {
            isNewInstrument = true;
            isDifferentInstrument = (oldInstrument != instrument);
            channel->pInstrument = module->getInstrument(instrument - 1); 
            if (channel->pInstrument) {
                if (channel->lastNote) {
                    sample = channel->pInstrument->getSampleForNote
                        (channel->lastNote - 1);
                    channel->pSample = 
                        channel->pInstrument->getSample(sample);
                }
                if (channel->pSample) {
                    channel->volume = 
                        channel->pSample->getVolume();
                    isValidInstrument = true;
                } else {
                    isValidInstrument = false;
                }
            }
        } else {
            isNewInstrument = false;
            channel->pInstrument = 
                module->getInstrument(oldInstrument - 1);
            if (channel->pInstrument) {
                if (channel->lastNote) {               
                    sample = channel->pInstrument->getSampleForNote
                        (channel->lastNote - 1);
                    channel->pSample = 
                        channel->pInstrument->getSample(sample); 
                }
            }
        }
/*
        if (isNewNote) {
            if (isNewInstrument) {
                channel->instrumentNo = instrument;
                if (isValidInstrument) replay = true;                
            }
            channel->oldNote = channel->newNote;
        }
*/
        if (isNewInstrument) channel->instrumentNo = instrument;
        if (isNewNote) replay = true;
        channel->oldNote = channel->newNote;


        channel->newNote = *iNote;

        for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMS; fxloop++) {
            effect   = iNote->effects[fxloop].effect;
            argument = iNote->effects[fxloop].argument;
            switch (effect) {
                case ARPEGGIO :
                    {
                        if (argument) {
                            channel->arpeggioCount = 0;
                            channel->arpeggioNote1  = channel->arpeggioNote2 
                                                    = channel->lastNote;
                            channel->arpeggioNote1 += argument >> 4;
                            channel->arpeggioNote2 += argument & 0xF;
                        }
                        break;
                    }
                case PORTAMENTO_UP :
                    {
                        if (argument) channel->lastPortamentoUp = argument;
                        break;
                    }
                case PORTAMENTO_DOWN :
                    {
                        if (argument) channel->lastPortamentoDown = argument;
                        break;
                    }
                case TONE_PORTAMENTO :
                    {
                        if (argument) channel->lastTonePortamento = argument;
                        break;
                    }
                case VIBRATO :
                    {
                        unsigned lv = channel->lastVibrato;
                        if (argument & 0xF0) 
                             lv = (lv & 0xF) + (argument & 0xF0);
                        if (argument & 0xF) 
                             lv = (lv & 0xF0) + (argument & 0xF);
                        channel->lastVibrato = lv;
                        break;
                    }
                case TREMOLO : 
                    {
                        unsigned lt = channel->lastTremolo;
                        if (argument & 0xF0) 
                             lt = (lt & 0xF) + (argument & 0xF0);
                        if (argument & 0xF) 
                             lt = (lt & 0xF0) + (argument & 0xF);
                        channel->lastTremolo = lt;
                        break;
                    }
                case SET_FINE_PANNING : 
                    {
                        channel->panning = argument;                       
                        break;
                    }
                case SET_SAMPLE_OFFSET : 
                    {                   
                        if (channel->pSample) {
                            argument <<= 8;
                            if (argument < (channel->pSample->getLength())) {
                                channel->sampleOffset = argument;
                                if (isNewNote) replay = true;
                            } else {
                                replay = false;
                            }
                        } 
                        break;
                    }
                case VOLUME_SLIDE :
                case TONE_PORTAMENTO_AND_VOLUME_SLIDE :
                case VIBRATO_AND_VOLUME_SLIDE :
                    {
                        if (argument) channel->lastVolumeSlide = argument;
                        break;
                    }
                case POSITION_JUMP :
                    {
                        patternBreak = true;
                        patternStartRow = 0;
                        nextPatternDelta = argument - iPatternTable;
                        break;
                    }
                case SET_VOLUME :
                    {
                        channel->volume = argument;
                        break;
                    }
                case PATTERN_BREAK :
                    {
                        patternBreak = true;
                        patternStartRow = ((argument & 0xF0) >> 4) * 10 +
                                           (argument & 0x0F);
                        break;
                    }
                case EXTENDED_EFFECTS : 
                    {
                        effect = argument >> 4;
                        argument &= 0xF;
                        switch (effect) {
                            case SET_GLISSANDO_CONTROL :
                                {
                                    break;
                                }
                            case SET_VIBRATO_CONTROL : 
                                {
                                    break;
                                }
                            case SET_FINETUNE :
                                {
                                    finetune = argument;
                                    if (finetune & 8) finetune |= 0xFFFFFFF0;
                                    break;
                                }
                            case SET_PATTERN_LOOP : 
                                {
                                    break;
                                }
                            case SET_TREMOLO_CONTROL :
                                {
                                    break;
                                }
                            case NOTE_RETRIG : 
                                {
                                    if (argument) channel->retrigCount = 0;//argument;
                                    else {
                                        iNote->effects[fxloop].effect = 0;
                                        iNote->effects[fxloop].argument = 0;
                                    }
                                    break;
                                }
                            case NOTE_CUT : 
                                {
                                    if (!argument) channel->volume = 0;
                                    break;
                                }
                            case NOTE_DELAY : 
                                {
                                    channel->delayCount = argument;
                                    isNoteDelayed = true;
                                    break;
                                }
                            case PATTERN_DELAY :
                                {
                                    if (argument) patternDelay_ = argument /* + 1 */;  // + 1 removed for inside.mod
                                    break;
                                }
                        }
                        break;
                    }
                case SET_TEMPO_BPM :
                    {
                        if (argument > 0x1F) {
                            bpm = argument;
                            setBpm();
                        } else {
                            tempo = argument;
                        }
                        break;
                    }
                case SET_GLOBAL_VOLUME :
                    {
                        globalVolume_ = argument;
                        break;
                    }
                case GLOBAL_VOLUME_SLIDE :
                    {
                        if (argument) 
                            channel->lastGlobalVolumeSlide = argument;
                        break;
                    }
                case SET_ENVELOPE_POSITION :
                    {
                        break;
                    }
                case PANNING_SLIDE :
                    {
                        if (argument) channel->lastPanningSlide = argument;
                        break;
                    }
                case MULTI_NOTE_RETRIG :
                    {
                        if (argument) channel->lastMultiNoteRetrig = argument;
                        break;
                    }
                case TREMOR :
                    {
                        break;
                    }
            }
        }

        // valid sample for replay ? -> replay sample if new note
        if (replay && channel->pSample && (!isNoteDelayed)) {
            playSample(iChannel, channel->pSample, 
                       channel->sampleOffset, FORWARD);
            if(!finetune) finetune = channel->pSample->getFinetune();
            setFrequency(iChannel, periodToFrequency(noteToPeriod(note + 
                channel->pSample->getRelativeNote(), finetune)));
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
        if (note) std::cout << noteStrings[note + 12];
        else std::cout << "---";
/*        
        if (channel->volume < 10)    std::cout << " ";
        else                    std::cout << (channel->volume / 10);
        std::cout << (channel->volume % 10) << "|";
*/
/*
        if (replay) {
            if(note < 10) std::cout << " ";
            std::cout << note << "|";
            if(channel->pSample->getRelativeNote() < 10) std::cout << " ";
            std::cout << channel->pSample->getRelativeNote() << "|";

            std::cout << noteToPeriod(note + 
                channel->pSample->getRelativeNote(), finetune) << "|";
        } else std::cout << "     ";
*/
        if (instrument < 10)    std::cout << " ";
        else                    std::cout << (instrument / 10);
        std::cout << (instrument % 10);
        if(iChannel == 0) {
            std::cout << "(";
            if (channel->volume < 10)   std::cout << " ";
            else                        std::cout << (channel->volume / 10);
            std::cout << (channel->volume % 10) << ")";
        }
        for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMS; fxloop++) {
            cout  << "." << hex[iNote->effects[fxloop].effect];
            std::cout << hex[iNote->effects[fxloop].argument >> 4];
            std::cout << hex[iNote->effects[fxloop].argument & 0xF]; 
        }
        std::cout << "|";
#endif
        iNote++;
    }  

    // prepare for next row / next function call
    if (!patternBreak) {
        patternRow++;
    } else {
        patternRow = pattern->getnRows();
    }
    if (patternRow >= pattern->getnRows()) {
#ifdef debug_mixer
        std::cout << "\n";
        _getch();
#endif
        patternRow = patternStartRow;
        if(nextPatternDelta > 0) iPatternTable += nextPatternDelta; // Disable repeat!
        else                     iPatternTable++;
        if (iPatternTable >= module->getSongLength()) {
            // repeat song
            iPatternTable = module->getSongRestartPosition();
        }
        pattern = module->getPattern(module->getPatternTable(iPatternTable));
        iNote = pattern->getRow(patternStartRow);
        //std::cout << "\nPlaying pattern # " << module->getPatternTable(iPatternTable) << ", order # " << iPatternTable; // debug
    }
    return 0;
}

int Mixer::updateEffects () {
    for (unsigned iChannel = 0; iChannel < nChannels; iChannel++) {
        Channel     *channel = channels[iChannel];

        for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMS; fxloop++) {
            Note        *note = &(channel->newNote);
            unsigned    effect   = note->effects[fxloop].effect;
            unsigned    argument = note->effects[fxloop].argument;

            switch (effect) {
                case VOLUME_SLIDE :
                case TONE_PORTAMENTO_AND_VOLUME_SLIDE :
                case VIBRATO_AND_VOLUME_SLIDE :
                    {
                        unsigned    *v = &(channel->volume);
                        unsigned    arg = channel->lastVolumeSlide;
                        unsigned    slide = (arg & 0xF0) >> 4;                      
                        if (slide) { // slide up
                            *v += slide;
                            if (*v > MAX_VOLUME) *v = MAX_VOLUME;
                        } else {     // slide down
                            slide = arg & 0x0F;
                            if (slide > *v) *v = 0;
                            else            *v -= slide;
                        }
                        break;
                    }
            }

            switch (effect) {
                case ARPEGGIO :
                    {
                        if (argument) {
                            switch (channel->arpeggioCount) {
                                case 0 : 
                                    {

                                        break;
                                    }
                                case 1 :
                                    {
                                        break;
                                    }
                                case 2 : 
                                    {
                                        break;
                                    }
                            }
                        }
                        break;
                    }
                case PORTAMENTO_UP :
                    {
                        break;
                    }
                case PORTAMENTO_DOWN :
                    {
                        break;
                    }
                case TONE_PORTAMENTO :
                case TONE_PORTAMENTO_AND_VOLUME_SLIDE :
                    {
                        break;
                    }
                case VIBRATO :
                case VIBRATO_AND_VOLUME_SLIDE :
                    {
                        break;
                    }
                case TREMOLO : 
                    {
                        break;
                    }
                case EXTENDED_EFFECTS :
                    {
                        effect = argument >> 4;
                        argument &= 0xF;
                        switch (effect) {
                            case NOTE_RETRIG : 
                                {
                                    channel->retrigCount++;
                                    if (channel->retrigCount >= argument) { 
                                        channel->retrigCount = 0;
                                        if (channel->pSample) { 
                                            playSample(iChannel, 
                                                       channel->pSample, 
                                                       channel->sampleOffset, 
                                                       FORWARD);
                                            setFrequency(iChannel, periodToFrequency(
                                                noteToPeriod(
                                                    channel->lastNote + 
                                                    channel->pSample->getRelativeNote(), 
                                                    channel->pSample->getFinetune())));
                                        }
                                    }
                                    break;
                                }
                            case NOTE_CUT : 
                                {
                                    if (tick > argument) {
                                        channel->volume = 0;
                                        //setVolume(iChannel, channel->volume);
                                        note->effects[fxloop].effect   = 0;
                                        note->effects[fxloop].argument = 0;
                                    }
                                    break;
                                }
                            case NOTE_DELAY : 
                                {                                    
                                    if (channel->delayCount <= tick) {
                                        // valid sample for replay ? -> replay sample if new note
                                        if (channel->pSample) { 
                                            playSample(iChannel, 
                                                       channel->pSample, 
                                                       channel->sampleOffset, 
                                                       FORWARD);
                                            setFrequency(iChannel, periodToFrequency(
                                                noteToPeriod(
                                                    channel->lastNote + 
                                                    channel->pSample->getRelativeNote(), 
                                                    channel->pSample->getFinetune())));
                                        }
                                        note->effects[fxloop].effect   = 0;
                                        note->effects[fxloop].argument = 0;
                                        //setPanning(iChannel, channel->panning);  // temp
                                        //setVolume(iChannel, channel->volume);    // temp
                                    }                                   
                                    break;
                                }
                            case FUNK_REPEAT : 
                                {
                                    break;
                                }
                        }
                        break;
                    }
                case GLOBAL_VOLUME_SLIDE :
                    {
                        unsigned    arg = channel->lastGlobalVolumeSlide;
                        unsigned    slide = (arg & 0xF0) >> 4;                      
                        if (slide) { // slide up
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
                case PANNING_SLIDE :
                    {
                        unsigned    panning = channel->panning;
                        unsigned    arg = channel->lastPanningSlide;
                        unsigned    slide = (arg & 0xF0) >> 4;                      
                        if (slide) { // slide up
                            panning += slide;
                            if (panning > PANNING_FULL_RIGHT) 
                                panning = PANNING_FULL_RIGHT;
                        } else {     // slide down
                            slide = arg & 0x0F;
                            if (slide > panning) 
                                 panning = PANNING_FULL_LEFT;
                            else panning -= slide;
                        }
                        //setPanning(iChannel, panning);
                        channel->panning = panning;
                        break;
                    }
                case MULTI_NOTE_RETRIG :
                    {  /* R + interval + Volume change */
                        int    *v = (int *)(&(channel->volume));
                        switch (argument & 0xF) {
                            case 1 : { (*v) --;                  break; }
                            case 2 : { (*v) -= 2;                break; }
                            case 3 : { (*v) -= 4;                break; }
                            case 4 : { (*v) -= 8;                break; }
                            case 5 : { (*v) -= 16;               break; }
                            case 6 : { (*v) <<= 1; (*v) /= 3;    break; }
                            case 7 : { (*v) >>= 1;               break; }
                            case 9 : { (*v) ++;                  break; }
                            case 10: { (*v) += 2;                break; }
                            case 11: { (*v) += 4;                break; }
                            case 12: { (*v) += 8;                break; }
                            case 13: { (*v) += 16;               break; }
                            case 14: { (*v) *= 3; (*v) >>= 1;    break; }
                            case 15: { (*v) >>= 1;               break; }
                        }
                        if (*v < 0) *v = 0;
                        if (*v > MAX_VOLUME) *v = MAX_VOLUME;

                        channel->retrigCount++;
                        if (channel->retrigCount >= (argument >> 4)) {
                            channel->retrigCount = 0;
                            if (channel->pSample) { 
                                playSample(iChannel, 
                                           channel->pSample, 
                                           channel->sampleOffset, 
                                           FORWARD);
                                setFrequency(iChannel, periodToFrequency(
                                    noteToPeriod(
                                        channel->lastNote + 
                                        channel->pSample->getRelativeNote(), 
                                        channel->pSample->getFinetune())));
                            }
                        }
                        //setVolume(iChannel, channel->volume);    // temp
                        break;
                    }
                case TREMOR :
                    {
                        break;
                    }
            }
        }
    }
    
    //setGlobalVolume(globalVolume_);
    return 0;
}

int Mixer::updateImmediateEffects () {
    for (unsigned iChannel = 0; iChannel < nChannels; iChannel++) {
        Channel     *channel = channels[iChannel];
        for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMS; fxloop++) {
            Note        *note = &(channel->newNote);
            unsigned    effect   = note->effects[fxloop].effect;
            unsigned    argument = note->effects[fxloop].argument;

            switch (effect) {
                case EXTENDED_EFFECTS :
                    {
                        effect = argument >> 4;
                        argument &= 0xF;
                        switch (effect) {
                            case FINE_PORTAMENTO_UP :
                                {
                                    if (argument) 
                                        channel->lastFinePortamentoUp = argument;
                                    break;
                                }
                            case FINE_PORTAMENTO_DOWN :
                                {
                                    if (argument) 
                                        channel->lastFinePortamentoDown = argument;
                                    break;
                                }
                            case FINE_VOLUME_SLIDE_UP :
                                {
                                    if (argument) 
                                        channel->lastFineVolumeSlideUp = argument;
                                    channel->volume += 
                                        channel->lastFineVolumeSlideUp;
                                    if (channel->volume > MAX_VOLUME) 
                                        channel->volume = MAX_VOLUME;
                                    break;
                                }
                            case FINE_VOLUME_SLIDE_DOWN :
                                {
                                    if (argument) 
                                        channel->lastFineVolumeSlideDown = argument;
                                    if (channel->lastFineVolumeSlideDown >= 
                                        channel->volume) 
                                            channel->volume = 0;
                                    else channel->volume -= 
                                            channel->lastFineVolumeSlideDown;
                                    break;
                                }
                        }
                        break;
                    }
                case EXTRA_FINE_PORTAMENTO :
                    {
                        effect = argument >> 4;
                        argument &= 0xF;
                        switch (effect) {
                            case EXTRA_FINE_PORTAMENTO_UP :
                                {
                                    if (argument) channel->lastExtraFinePortamentoUp = argument;
                                    break;
                                }
                            case EXTRA_FINE_PORTAMENTO_DOWN :
                                {
                                    if (argument) channel->lastExtraFinePortamentoDown = argument;
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
    if (tick < tempo) { 
        updateEffects ();         
    } else { 
        tick = 0; 
        updateNotes (); 
        updateImmediateEffects ();
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
            mixer.resetSong();
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

int main(int argc, char *argv[])  { 
    std::vector< std::string > filePaths;
    char        *modPaths[] = {
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\ctstoast.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\dope.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\baska.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\bp7\\bin\\exe\\cd2part2.mod",
//        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\audiopls\\crmx-trm.mod",
//        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\music\\xm\\united_7.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\ctstoast.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\dope.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\smokeoutstripped.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\smokeout.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\KNGDMSKY.XM",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\KNGDMSKY-mpt.XM",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\myrieh.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\mental.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\crain.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\toybox.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\etanol.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\sac09.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\1.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\bbobble.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\asm94.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\4ma.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\chipmod\\mental.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\over2bg.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\explorat.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\pullmax.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\bluishbg.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\devlpr94.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\theend.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\bj-eyes.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\1993.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\1993.xm",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\demotune.mod", // xm = wrong, ptn loop tester
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\baska.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\bj-love.xm",
//        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\probmod\\3demon.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\probmod\\veena.wow",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\probmod\\flt8_1.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\probmod\\nowwhat3.mod",
        "d:\\Erland Backup\\C_SCHIJF\\erland\\dosprog\\mods\\probmod\\xenolog1.mod",
        nullptr
    };

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
        const char *moduleFilename = filePaths[i].c_str();
        Module      sourceFile( moduleFilename );
        Mixer       mixer;

        std::cout << "\n\nLoading " << moduleFilename 
                  << ": " << (sourceFile.isLoaded() ? "Success." : "Error!") << std::endl;

        if (sourceFile.isLoaded ()) {
            unsigned s = BUFFER_SIZE / (MIXRATE * 2); // * 2 for stereo
            std::cout << "\nCompiling module into " << (s / 60) << "m "
                      << (s % 60) << "s of 16 bit WAVE data"  << std::endl 
                      << "Hit any key to start mixing." << std::endl;
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

   
