

#include "Module.h"
#include "Mixer.h"

// define static variables from the Mixer class:
CRITICAL_SECTION    Mixer::waveCriticalSection;
WAVEHDR*            Mixer::waveBlocks;
volatile int        Mixer::waveFreeBlockCount;
int                 Mixer::waveCurrentBlock;

Mixer::Mixer()
{
    /*
        Allocate memory for the buffer used by the mixing routine:
    */
    mixBuffer = std::make_unique < MixBufferType[] >( SAMPLES_PER_BLOCK );

    /*
        Allocate memory for the <BLOCK_COUNT> amount of WAVE buffers:
    */
    unsigned char* buffer;
    unsigned       totalBufferSize = (sizeof( WAVEHDR ) + BLOCK_SIZE)
        * BLOCK_COUNT;
    if ( (buffer = (unsigned char*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        totalBufferSize )) == NULL ) {
        std::cout << "Memory allocation error\n";
        ExitProcess( 1 );
    }
    /*
        Set up the pointers to each bit in memory, all headers are side by
        side, followed by the data buffers - so the audio data will form
        one continuous block. Point to first data buffer:
    */
    waveBlocks = (WAVEHDR *)buffer;
    buffer += sizeof( WAVEHDR ) * BLOCK_COUNT;
    for ( int i = 0; i < BLOCK_COUNT; i++ ) {
        waveBlocks[i].dwBufferLength = BLOCK_SIZE;
        waveBlocks[i].lpData = (LPSTR)buffer;
        buffer += BLOCK_SIZE;
    }

    /*
        prepare the header for the windows WAVE functions
    */
    if ( BITS_PER_SAMPLE == 16 )
        waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
    else
        waveFormatEx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;//WAVE_FORMAT_PCM;// WAVE_FORMAT_EXTENSIBLE
    waveFormatEx.nChannels = 2;               // stereo
    waveFormatEx.nSamplesPerSec = MIXRATE;
    waveFormatEx.wBitsPerSample = BITS_PER_SAMPLE;
    waveFormatEx.nBlockAlign = waveFormatEx.nChannels *
        waveFormatEx.wBitsPerSample / 8;
    waveFormatEx.nAvgBytesPerSec = waveFormatEx.nSamplesPerSec *
        waveFormatEx.nBlockAlign;
    waveFormatEx.cbSize = 0;

    /*
    // not actually needed: the basic waveformatex supports float wave files
    waveFormatEx.cbSize = sizeof( WAVEFORMATEXTENSIBLE ) - sizeof( WAVEFORMATEX );
    waveFormatExtensible.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    waveFormatExtensible.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    //waveFormatExtensible.Samples.wSamplesPerBlock = BLOCK_SIZE / sizeof( DestBufferType );
    waveFormatExtensible.Samples.wValidBitsPerSample = 32; // ????
    //waveFormatExtensible.Samples.wReserved = 0;
    waveFormatExtensible.Format = waveFormatEx;
    */
}

Mixer::~Mixer()
{
    /*
        Free the WAVEHDR blocks that were allocated in the constructor:
    */
    HeapFree( GetProcessHeap(),0,waveBlocks );
    return;
}

void CALLBACK Mixer::waveOutProc(
    HWAVEOUT hWaveOut,
    UINT uMsg,
    DWORD *dwInstance,
    DWORD dwParam1,
    DWORD dwParam2 )
{

    // pointer to free block counter
    DWORD* freeBlockCounter = dwInstance;

    // ignore calls that occur due to openining and closing the device.
    if ( uMsg != WOM_DONE )
        return;
    EnterCriticalSection( &waveCriticalSection );
    (*freeBlockCounter)++;
    LeaveCriticalSection( &waveCriticalSection );
    //updateWaveBuffers();
}

void Mixer::startReplay()
{
    /*
        Initialize mixCount index:
    */
    mixCount = 0;

    /*
        Initialize block counter & block index:
    */
    waveFreeBlockCount = BLOCK_COUNT;
    waveCurrentBlock = 0;
    InitializeCriticalSection( &waveCriticalSection );

    /*
        try to open the default wave device. WAVE_MAPPER is
        a constant defined in mmsystem.h, it always points to the
        default wave device on the system (some people have 2 or
        more sound cards).
    */
    MMRESULT mmrError = waveOutOpen(
        &hWaveOut,
        WAVE_MAPPER,
        &waveFormatEx,
        //(const WAVEFORMATEX *)(&waveFormatExtensible),
        (DWORD_PTR)waveOutProc,
        (DWORD_PTR)&waveFreeBlockCount,
        CALLBACK_FUNCTION
    );
    if( mmrError != MMSYSERR_NOERROR ) {
        std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>( 256 );
        memset( buffer.get(), 0, sizeof( wchar_t ) * 256 );
        waveOutGetErrorText( mmrError,(wchar_t *)buffer.get(),254 );
        std::wcout 
            << "\nUnable to open wave mapper device: " 
            << buffer.get() 
            << "\n";
        ExitProcess( 1 );
    }
}

int Mixer::stopReplay()
{
    /*
        wait for all blocks to complete
    */
    //while ( waveFreeBlockCount < BLOCK_COUNT )
    //    Sleep( 10 );

    /*
        unprepare any blocks that are still prepared
    */
    for ( int i = 0; i < waveFreeBlockCount; i++ )
        if ( waveBlocks[i].dwFlags & WHDR_PREPARED )
            waveOutUnprepareHeader( hWaveOut,&waveBlocks[i],sizeof( WAVEHDR ) );

    waveOutReset( hWaveOut ); // not necessary?
    waveOutClose( hWaveOut );
    DeleteCriticalSection( &waveCriticalSection );
    return 0;
}

void Mixer::updateWaveBuffers() {
    while ( waveFreeBlockCount ) {
        WAVEHDR* current = &(waveBlocks[waveCurrentBlock]);

        /*
            first make sure the header we're going to use is unprepared
        */
        if ( current->dwFlags & WHDR_PREPARED )
            waveOutUnprepareHeader( hWaveOut,current,sizeof( WAVEHDR ) );

        doMixBuffer( (DestBufferType*)(current->lpData) );

        waveOutPrepareHeader( hWaveOut,current,sizeof( WAVEHDR ) );
        waveOutWrite( hWaveOut,current,sizeof( WAVEHDR ) );

        EnterCriticalSection( &(waveCriticalSection) );
        waveFreeBlockCount--;
        LeaveCriticalSection( &(waveCriticalSection) );

        /*
            point to the next block
        */
        waveCurrentBlock++;
        waveCurrentBlock %= BLOCK_COUNT;
    }
}

void Mixer::resetSong()
{
    mixIndex = 0;
    for ( unsigned i = 0; i < MIXER_MAX_CHANNELS; i++ )
        mixerChannels[i].init();

    for ( unsigned i = 0; i < nrChannels; i++ ) {
        channels[i].init();
        channels[i].panning = module->getDefaultPanPosition( i );
    }
    globalPanning_ = 0x20;  // 0 means extreme LEFT & RIGHT, so no attenuation
    globalVolume_ = 64;
    gain = 96;              // max = 256

    tempo = module->getDefaultTempo();
    bpm = module->getDefaultBpm();
    setBpm();

    patternDelay_ = 0;
    patternRow = 0;
    iPatternTable = 0;
    pattern = &(module->getPattern( module->getPatternTable( iPatternTable ) ));
    iNote = pattern->getRow( 0 );
}

int Mixer::initialize( Module* m )
{
    if ( !m )
        return 0;
    if ( !m->isLoaded() )
        return 0;
    module = m;
    nrChannels = module->getnChannels();
    switch ( module->getTrackerType() ) {
        case TRACKER_PROTRACKER:
        {
            st300FastVolSlides_ = false;
            st3StyleEffectMemory_ = false;
            ft2StyleEffects_ = false;
            itStyleEffects_ = false;
            break;
        }
        case TRACKER_ST300:
        {
            st300FastVolSlides_ = true;
            st3StyleEffectMemory_ = true;
            ft2StyleEffects_ = false;
            itStyleEffects_ = false;
            break;
        }
        case TRACKER_ST321:
        {
            // S3M loader resets tracker version to ST3.0 if fastvolslides is on
            st300FastVolSlides_ = false;
            st3StyleEffectMemory_ = true;
            ft2StyleEffects_ = false;
            itStyleEffects_ = false;
            break;
        }
        case TRACKER_FT2:
        {
            st300FastVolSlides_ = false;
            st3StyleEffectMemory_ = false;
            ft2StyleEffects_ = true;
            itStyleEffects_ = false;
            break;
        }
        case TRACKER_IT:
        {
            st300FastVolSlides_ = false;
            st3StyleEffectMemory_ = false;
            ft2StyleEffects_ = false;
            itStyleEffects_ = true;
            break;
        }
    }
    resetSong();
    isInitialised = true;
    return 0;
}

int Mixer::doMixBuffer( DestBufferType* buffer )
{
    memset( mixBuffer.get(),0,SAMPLES_PER_BLOCK * sizeof( MixBufferType ) );

    mixIndex = 0;
    unsigned x = callBpm - mixCount;
    unsigned y = BLOCK_SIZE / waveFormatEx.nBlockAlign;
    if ( x > y ) {
        mixCount += y;
        doMixSixteenbitStereo( y );
    } 
    else {
        doMixSixteenbitStereo( x );
        x = y - x;
        mixCount = 0;
        updateBpm();
        for ( ;x >= callBpm; ) {
            doMixSixteenbitStereo( callBpm );
            x -= callBpm;
            updateBpm();
        }
        if ( x ) {
            mixCount = x;
            doMixSixteenbitStereo( x );
        }
    }
    /*
        transfer sampled data from [sizeof( MixBufferType ) * 8] bit buffer
        into BITS_PER_SAMPLE bit buffer:
    */
    saturation = 0;
    MixBufferType* src = mixBuffer.get();
    DestBufferType* dst = buffer;

    switch ( BITS_PER_SAMPLE ) { 
        case 16:
        {
            // 16 bit is fixed point, never floating point
            for ( unsigned i = 0; i < SAMPLES_PER_BLOCK; i++ ) {
                MixBufferType tmp = src[i] >> 8;
                //MixBufferType tmp = src[i] / 256;

                if ( tmp < -32768 ) {
                    tmp = -32768;
                    saturation++;
                }
                if ( tmp > 32767 ) {
                    tmp = 32767;
                    saturation++;
                }
                dst[i] = (DestBufferType)tmp;
            }  
            //std::cout << "\n\nSaturation = " << saturation << "\n"; // DEBUG
            break;
        }
        case 32:
        {
            // next 3 values == debug
            float min = 0.0f;
            float max = 0.0f;
            int saturation = 0;

            for ( unsigned i = 0; i < SAMPLES_PER_BLOCK; i++ ) {
                //dst[i] = src[i] << 6; // for 32 bit PCM DATA

                float t = (float)(src[i]) / (32768.0f * 256.0f);
                t = std::max( -1.0f,t );
                t = std::min( 1.0f,t );

                if ( t == -1.0f || t == 1.0f ) // debug
                    saturation++;

                dst[i] = t;

                min = std::min( min,t ); // debug
                max = std::max( max,t ); // debug
            }
            //std::cout                    // debug
            //    << "\nmin: " << std::setw( 16 ) << min
            //    << " max: " << std::setw( 16 ) << max
            //    << " saturation: " << saturation;
            //break;
        }
    }
    return 0;
}

int Mixer::doMixSixteenbitStereo( unsigned nSamples )
{
    /*
    for ( int i = 0; i < 8; i++ )
        std::cout
        << "mChn " << std::setw( 1 ) << i << ":"
        << std::setw( 1 ) << mixerChannels[i].masterChannel << " ";
    std::cout << "\n";
    */

    nrActiveMixChannels = 0;
    for ( unsigned i = 1; i < MIXER_MAX_CHANNELS; i++ ) {
        MixerChannel& mChn = mixerChannels[i];

        /*
        std::cout
            << std::setw( 4 ) << i << ": "
            << std::setw( 4 );
        if ( mChn.isActive )
            std::cout << (int)mChn.masterChannel;
        else
            std::cout << "  X ";
        */


        if ( mChn.isActive ) {
            Sample& sample = *mChn.sample;
            unsigned        mixOffset = mixIndex;
            int             leftGain = (gain * mChn.leftVolume) >> 12;
            int             rightGain = (gain * mChn.rightVolume) >> 12;

            // div by zero safety. Probably because of portamento over/under flow
            if ( !mChn.sampleIncrement )
                continue;

            mChn.age++;

            /*  DEBUG:
                only works if age of channel is reset at start of downwards
                volume ramp. Might trigger a warning with backwards playing
                samples as there is no volume ramp implemented there
            */
            if ( (!mChn.isMaster) &&
                (mChn.age > 2) &&
                (mChn.volumeRampDelta == VOLUME_RAMPING_DOWN) )
                std::cout
                << "\nFailed to stop channel " << i << "!\n"
                << "\nvolumeRampCounter: " << (unsigned)mChn.volumeRampCounter
                << "\nvolumeRampDelta  : " << (int)mChn.volumeRampDelta
                << "\nvolumeRampStart  : " << (int)mChn.volumeRampStart
                << "\nAge              : " << mChn.age << "\n";

            nrActiveMixChannels++;

            MixBufferType* mixBufferPTR = mixBuffer.get() + mixIndex;
            int chnInc = mChn.sampleIncrement;
            for ( unsigned j = 0; j < nSamples; ) {
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
                    SHORT* SampleDataPTR = sample.getData() + mChn.sampleOffset;

                    // *********************
                    // added for volume ramp
                    // *********************
                    // following code overflows mixbuffer and corrupts wave format ex header! -> fixed
                    int loopsLeft = nrLoops;
                    if ( mChn.volumeRampCounter ) {
                        int loopEnd;
                        if ( mChn.volumeRampCounter <= nrLoops ) {
                            loopEnd = mChn.volumeRampCounter
                                * mChn.sampleIncrement
                                + mChn.sampleOffsetFrac;
                            loopsLeft -= mChn.volumeRampCounter;
                            mChn.volumeRampCounter = 0;
                        } else {
                            loopEnd = nrLoops
                                * mChn.sampleIncrement
                                + mChn.sampleOffsetFrac;
                            mChn.volumeRampCounter -= loopsLeft;
                            loopsLeft = 0;
                        }

                        for ( int ofsFrac = mChn.sampleOffsetFrac;
                            ofsFrac < loopEnd; ofsFrac += chnInc ) {

                            int idx = ofsFrac >> 15;
                            int p0 = SampleDataPTR[idx - 1];
                            int p1 = SampleDataPTR[idx];
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
                                ((((((a * fract) >> FRAC_RES_SHIFT)
                                    + b)* fract) >> FRAC_RES_SHIFT)
                                    + c)* fract) >> FRAC_RES_SHIFT)
                                + p1;

                            int lG = (leftGain * mChn.volumeRampStart) / VOLUME_RAMP_SPEED;
                            int rG = (rightGain * mChn.volumeRampStart) / VOLUME_RAMP_SPEED;

                            *mixBufferPTR++ += f2 * lG;
                            *mixBufferPTR++ += f2 * rG;
                            mChn.volumeRampStart += mChn.volumeRampDelta;
                        }
                        mChn.sampleOffsetFrac = loopEnd;

                        if ( (mChn.volumeRampStart <= 0) &&
                            (mChn.volumeRampDelta == VOLUME_RAMPING_DOWN) ) {
                            mChn.isActive = false;
                            if ( mChn.isMaster ) {
                                channels[mChn.masterChannel].mixerChannelNr = 0;
                                mChn.isMaster = false;
                            }
                            j = nSamples;
                            loopsLeft = 0;
                        }

                    }
                    /* */
                    // **************************
                    // added for volume ramp: end
                    // **************************

                    //int loopEnd = nrLoops * mChn.sampleIncrement + mChn.sampleOffsetFrac; // orig
                    int loopEnd = loopsLeft * mChn.sampleIncrement + mChn.sampleOffsetFrac;

                    for ( int ofsFrac = mChn.sampleOffsetFrac;
                        ofsFrac < loopEnd; ofsFrac += chnInc ) {

                        int idx = ofsFrac >> 15;
                        int p0 = SampleDataPTR[idx - 1];
                        int p1 = SampleDataPTR[idx];
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
                            ((((((a * fract) >> FRAC_RES_SHIFT)
                                + b)* fract) >> FRAC_RES_SHIFT)
                                + c)* fract) >> FRAC_RES_SHIFT)
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
                    if ( mChn.sampleOffset >= sample.getRepeatEnd() ) {
                        if ( sample.isRepeatSample() ) {
                            if ( !sample.isPingpongSample() ) {
                                mChn.sampleOffset = sample.getRepeatOffset();  // ?
                            } else {
                                mChn.sampleOffset = sample.getRepeatEnd() - 1; // ?
                                mChn.isPlayingBackwards = true;
                            }
                        } else {
                            mChn.isActive = false;
                            if ( mChn.isMaster ) {
                                channels[mChn.masterChannel].mixerChannelNr = 0;
                                mChn.isMaster = false;
                            }
                            // quit loop, we're done here
                            j = nSamples;
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
                    if ( nrSamplesLeft > 8191 )
                        nrSamplesLeft = 8191;
                    int nrLoops =
                        //    ((nrSamplesLeft << 16) + (int)mChn.sampleIncrement - 1 
                        ((nrSamplesLeft << 15) + (int)mChn.sampleIncrement - 1
                            - (int)mChn.sampleOffsetFrac) / (int)mChn.sampleIncrement;
                    if ( nrLoops >= (int)(nSamples - j) )
                        nrLoops = (int)(nSamples - j);
                    if ( nrLoops < 0 )
                        nrLoops = 0;

                    mChn.sampleOffsetFrac = (int)mChn.sampleOffsetFrac + nrLoops * (int)mChn.sampleIncrement;
                    //int smpDataShift = mChn.sampleOffsetFrac >> 16;
                    int smpDataShift = mChn.sampleOffsetFrac >> 15;
                    if ( (int)mChn.sampleOffset < smpDataShift ) { // for bluishbg2.xm
                        mChn.sampleOffset = 0;
                        //std::cout << "underrun!" << std::endl;
                    } else mChn.sampleOffset -= smpDataShift;
                    SHORT* SampleDataPTR = sample.getData() + mChn.sampleOffset;

                    for ( int j2 = 0; j2 < nrLoops; j2++ ) {
                        //int s1 = SampleDataPTR[     (mChn.sampleOffsetFrac >> 16)];
                        //int s2 = SampleDataPTR[(int)(mChn.sampleOffsetFrac >> 16) - 1];
                        int s1 = SampleDataPTR[(mChn.sampleOffsetFrac >> 15)];
                        int s2 = SampleDataPTR[(int)(mChn.sampleOffsetFrac >> 15) - 1];
                        //int xd = (0x10000 - (mChn.sampleOffsetFrac & 0xFFFF)) >> 1;  // time delta
                        int xd = (0x8000 - (mChn.sampleOffsetFrac & 0x7FFF));       // time delta
                        int yd = s2 - s1;                                            // sample delta
                        s1 += (xd * yd) >> 15;

                        int lG = leftGain;
                        int rG = rightGain;

                        // lame volume ramp implementation starts here
                        if ( mChn.volumeRampCounter ) {
                            mChn.volumeRampCounter--;
                            lG = (lG * mChn.volumeRampStart) / VOLUME_RAMP_SPEED;
                            rG = (rG * mChn.volumeRampStart) / VOLUME_RAMP_SPEED;
                            mChn.volumeRampStart += mChn.volumeRampDelta;
                        } else if ( mChn.volumeRampDelta == VOLUME_RAMPING_DOWN ) {
                            mChn.isActive = false;
                            if ( mChn.isMaster ) {
                                channels[mChn.masterChannel].mixerChannelNr = 0;
                                mChn.isMaster = false;
                            }
                        }
                        // lame volume ramp implementation ends here

                        *mixBufferPTR++ += (s1 * lG);
                        *mixBufferPTR++ += (s1 * rG);
                        mChn.sampleOffsetFrac -= mChn.sampleIncrement;
                    }
                    mixOffset += nrLoops << 1;
                    j += nrLoops;

                    if ( mChn.sampleOffset <= sample.getRepeatOffset() ) {
                        if ( sample.isRepeatSample() ) {
                            mChn.sampleOffset = sample.getRepeatOffset();
                            mChn.isPlayingBackwards = false;
                        } else {
                            mChn.isActive = false;
                            if ( mChn.isMaster ) {
                                channels[mChn.masterChannel].mixerChannelNr = 0;
                                mChn.isMaster = false;
                            }
                            // quit loop, we're done here
                            j = nSamples;
                        }
                    }
                }
            }

        }
    }
    mixIndex += (nSamples << 1); // *2 for stereo
    //std::cout << "\nNr of active chn = " << nActiveMixChannels;
    //std::cout << std::setw( 4 ) << nActiveMixChannels;

    return 0;
}


/*

AMIGA calculations:

period = (1712 / 128) * 2 ^ [ ( 132 - (note - 1) - finetune / 128 ) / 12 ]

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


unsigned Mixer::noteToPeriod( unsigned note,int finetune )
{
    if ( module->useLinearFrequencies() ) {
        return (7680 - ((note - 1) << 6) - (finetune >> 1));
    } else {
        return (unsigned)(
            pow( 2.0,
            ((11.0 * 12.0) - ((double)(note - 1) + ((double)finetune / 128.0))) / 12.0
            )
            * (1712.0 / 128.0)
            );
    }
}

unsigned Mixer::periodToFrequency( unsigned period )
{
    return module->useLinearFrequencies() ?
        (unsigned)(8363 * pow( 2,((4608.0 - (double)period) / 768.0) ))
        :
        (period ? ((8363 * 1712) / period) : 0);
}

int Mixer::setMixerVolume( unsigned fromChannel )
{
    unsigned        gp = globalPanning_;
    bool            invchn = (gp >= PANNING_CENTER);
    unsigned        soften = (invchn ? (PANNING_FULL_RIGHT - gp) : gp);

    MixerChannel& pmc = mixerChannels[channels[fromChannel].mixerChannelNr];
    if ( pmc.isActive ) {
        unsigned p = channels[fromChannel].panning;
        unsigned v = channels[fromChannel].volume * globalVolume_;

        p = soften +
            ((p * (PANNING_MAX_STEPS - (soften << 1))) >> PANNING_SHIFT);
        if ( invchn )
            p = PANNING_FULL_RIGHT - p;
        pmc.leftVolume = ((PANNING_FULL_RIGHT - p) * v) >> PANNING_SHIFT;
        pmc.rightVolume = (p * v) >> PANNING_SHIFT;
        if ( balance_ < 0 ) {
            pmc.rightVolume *= (100 + balance_);
            pmc.rightVolume /= 100;
        }
        if ( balance_ > 0 ) {
            pmc.leftVolume *= (100 - balance_);
            pmc.leftVolume /= 100;
        }
    }

    /*
    //std::cout << " fc " << fromChannel << ",";
    for ( int i = 0; i < MAX_NOTES_PER_CHANNEL; i++ ) {
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
    */
    return 0;
}

int Mixer::setFrequency( unsigned fromChannel,unsigned frequency )
{
    MixerChannel& pmc = mixerChannels[channels[fromChannel].mixerChannelNr];
    pmc.sampleIncrement = (unsigned)
        (((double)frequency * 32768.0) / (double)MIXRATE);

    /*
    for ( int i = 0; i < MAX_NOTES_PER_CHANNEL; i++ ) {
        unsigned    mc = channels[fromChannel].mixerChannelsTable[i];
        if (mc) {
            MixerChannel&    pmc = mixerChannels[mc];
            if ( pmc.isMaster ) {
                //double f = ((double)frequency * 65536.0) / (double)MIXRATE;
                double f = ((double)frequency * 32768.0) / (double)MIXRATE;
                pmc.sampleIncrement = (unsigned) f;
                return 0;
            }
        }
    }
    */
    return 0;
}
// range: 0..255 (extreme stereo... extreme reversed stereo)
int Mixer::setGlobalPanning( unsigned panning )
{
    globalPanning_ = panning;
    return 0;
}

int Mixer::setGlobalBalance( int balance )  // range: -100...0...+100
{
    balance_ = balance;
    return 0;
}

int Mixer::playSample(    // rename to playInstrument() ?
    unsigned    fromChannel,
    Sample* sample,
    unsigned    sampleOffset,
    bool        direction )
{
    /*
        todo:
        - check NNA situation;
            - if NNA = cut, set the virtual channel to volume fade out
            - if NNA = fade, ...
            - if NNA = continue, ...

          --=> Set the isPrimary (isMaster) flag to false, and allocate a new
          virtual channel for this master channel. "fromChannel" is always a
          "Master" Channel

    */

    // cut previous note if it is still playing:
    stopChannelPrimary( fromChannel );

    // find an empty slot in mixer channels table
    unsigned newMc;
    /*
    for ( newMc = 1;
        (newMc < MIXER_MAX_CHANNELS) && mixerChannels[newMc].isActive;
        newMc++ );
    */
    for ( newMc = 1; newMc < MIXER_MAX_CHANNELS; newMc++ )
        if ( !mixerChannels[newMc].isActive )
            break;

    // "no free channel found" logic here
    // find oldest channel logic and use that instead
    if ( newMc >= MIXER_MAX_CHANNELS ) {
        std::cout << "\nFailed to allocate mixer channel!\n";
        return -1;
    }

    // added for envelope processing:
    channels[fromChannel].iVolumeEnvelope = 0;
    channels[fromChannel].iPanningEnvelope = 0;
    channels[fromChannel].iPitchFltrEnvelope = 0;
    channels[fromChannel].keyIsReleased = false;
    // end of addition for envelopes


    mixerChannels[newMc].isActive = true;
    mixerChannels[newMc].isMaster = true;
    mixerChannels[newMc].isPlayingBackwards = direction;
    mixerChannels[newMc].age = 0;
    mixerChannels[newMc].masterChannel = fromChannel;
    mixerChannels[newMc].sample = sample;

    mixerChannels[newMc].volumeRampDelta = VOLUME_RAMPING_UP;
    mixerChannels[newMc].volumeRampCounter = VOLUME_RAMP_SPEED;
    mixerChannels[newMc].volumeRampStart = 0;
    mixerChannels[newMc].sampleOffset = sampleOffset;
    channels[fromChannel].mixerChannelNr = newMc;
    return 0;
}

int Mixer::stopChannelPrimary( unsigned fromChannel )
{
    MixerChannel& pmc = mixerChannels[channels[fromChannel].mixerChannelNr];
    if ( ((pmc.masterChannel == fromChannel) && pmc.isMaster)
        // || (channels[fromChannel].mixerChannelNr == 0)
        ) {
        //pmc.isActive = false;//debug: no vol ramping
        pmc.isMaster = false;
        pmc.age = 0; // TEMP DEBUG!!!
        channels[fromChannel].mixerChannelNr = 0;
        pmc.volumeRampDelta = VOLUME_RAMPING_DOWN;
        pmc.volumeRampCounter = VOLUME_RAMP_SPEED;
        pmc.volumeRampStart = VOLUME_RAMP_SPEED;
    }
    return 0;
}

int Mixer::updateEnvelopes()
{
    for ( unsigned iChannel = 0; iChannel < nrChannels; iChannel++ ) {

    }
    return 0;
}

int Mixer::setVolumes()
{
    for ( unsigned iChannel = 0; iChannel < nrChannels; iChannel++ ) {
        setMixerVolume( iChannel );
    }
    return 0;
}

int Mixer::updateBpm()
{
    tick++;
    if ( tick < tempo ) {
        updateEffects();
    } 
    else {
        tick = 0;
        if ( !patternDelay_ )
            updateNotes();
        else
            patternDelay_--;
        updateImmediateEffects();
    }
    setVolumes();
    return 0;
}

