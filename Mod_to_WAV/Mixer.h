#pragma once
// Floating point mixer

#include <climits>
#if CHAR_BIT != 8 
This code requires a byte to be 8 bits wide
#endif

#pragma comment (lib,"winmm.lib") 

#define NOMINMAX
#include <windows.h>
#include <algorithm>

#include <mmreg.h>
#include <cstdint>
#include <assert.h>
#include <functional>
#include <mmsystem.h>
#include <mmreg.h>

#include <ks.h>
#include <Ksmedia.h>

#include "Constants.h"
#include "Instrument.h"
#include "Sample.h"
#include "Module.h"

//#define debug_mixer   // enable to get pattern debuginfo :)


/**************************************************************************
*                                                                         *
*   CONSTANTS FOR THE MIXERCHANNEL:                                       *
*                                                                         *
**************************************************************************/
const bool  MXR_VOLUME_RAMP_UP      = false;
const bool  MXR_VOLUME_RAMP_DOWN    = true;
const int   MXR_PANNING_FULL_LEFT   = 0;
const int   MXR_PANNING_CENTER      = 127;
const int   MXR_PANNING_FULL_RIGHT  = 255;
const int   MXR_NO_PHYSICAL_CHANNEL_ATTACHED = -1;
const float MXR_MIN_FREQUENCY_INC = 2.26757e-4f; // 20 Hz == 20 / (44100 * 2)

/*
possible flags:
 1 - if the channel is active
 2 - if a volume ramp is active (click removal)
 4 - if the volume is ramping down (volume ramps up otherwise)
 8 - if the sample is playing backwards
16 - if the sample is sustained
32 - if the sample is keyed off

*/
const int   MXR_CHANNEL_IS_ACTIVE_FLAG          = 1;
const int   MXR_VOLUME_RAMP_IS_ACTIVE_FLAG      = 2;
const int   MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG   = 4;
const int   MXR_PLAYING_BACKWARDS_FLAG          = 8;
const int   MXR_SAMPLE_IS_SUSTAINED_FLAG        = 16;
const int   MXR_SAMPLE_IS_KEYED_OFF_FLAG        = 32;
const int   MXR_SAMPLE_IS_FADING_OUT_FLAG       = 64;
const int   MXR_SURROUND_IS_ACTIVE_FLAG         = 128;
const int   MXR_IS_PRIMARY_CHANNEL_FLAG         = 256;

/**************************************************************************
*                                                                         *
*   GLOBAL CONSTANTS FOR THE MIXER:                                       *
*                                                                         *
**************************************************************************/
const int MXR_MIXRATE = 44100;          // in Hz                                            
const int MXR_MAX_PHYSICAL_CHANNELS = 256;
const int MXR_MAX_LOGICAL_CHANNELS = 64;

// for internal mixing, 32 bit will do for now
//typedef std::int32_t MixBufferType;      
typedef float MixBufferType;

const int MXR_BLOCK_SIZE = 0x4000;      // normally 0x4000
const int MXR_BLOCK_COUNT = 4;          // at least 2, or 4 (?) for float mixing
#define   MXR_BITS_PER_SAMPLE      32   //  (sizeof( MixBufferType ) / 8) // can't use constexpr here
const int MXR_SAMPLES_PER_BLOCK = MXR_BLOCK_SIZE /
            (MXR_BITS_PER_SAMPLE / 8);  // 32 bit = 4 bytes / sample

#if MXR_BITS_PER_SAMPLE == 32
typedef float DestBufferType;           // DestBufferType must be float for 32 bit mixing
#elif MXR_BITS_PER_SAMPLE == 16
typedef std::int16_t DestBufferType;    // DestBufferType must be std::int16_t for 16 bit mixing
#else
Error!Output buffer must be 16 bit or 32 bit!
#endif

const int MXR_NO_INTERPOLATION = 0;
const int MXR_LINEAR_INTERPOLATION = 1;
const int MXR_CUBIC_INTERPOLATION = 2;
const int MXR_SINC_INTERPOLATION = 3;
const int MXR_INTERPOLATION_TYPES = 4;

/******************************************************************************
*******************************************************************************
*                                                                             *
*   MixerChannel Class Definition                                             *
*                                                                             *
*   The physical mixer channels play a sample while taking into account the   *
*   volume, panning and pitch envelopes                                       *
*                                                                             *
*******************************************************************************
******************************************************************************/
class MixerChannel {
public:
    MixerChannel() { clear(); }
    void            clear()
    {
        flags_ = 0;
        parentLogicalChannel_ = 0;
        rampLeftVolume_ = 0; 
        rampRightVolume_ = 0;
        rampLeftInc_ = 0;
        rampRightInc_ = 0;
        leftVolume_ = 0;
        rightVolume_ = 0;
        frequencyInc_ = 0;
        fadeOut_ = 0;
        volEnvIdx_ = 0;
        panEnvIdx_ = 0;
        PitchEnvIdx_ = 0;
        offset_ = 0;
        fracOffset_ = 0;
        pSample_ = nullptr;
        pInstrument_ = nullptr;   // for the envelopes
    }
    bool            isActive() const { return (flags_ & MXR_CHANNEL_IS_ACTIVE_FLAG) != 0; }
    void            activate() { setFlags( MXR_CHANNEL_IS_ACTIVE_FLAG ); }
    void            deactivate() { clearFlags( MXR_CHANNEL_IS_ACTIVE_FLAG ); }
    void            makePrimary() { setFlags( MXR_IS_PRIMARY_CHANNEL_FLAG ); }
    void            makeSecondary() { clearFlags( MXR_IS_PRIMARY_CHANNEL_FLAG ); }
    bool            isPrimary() const { return isSet( MXR_IS_PRIMARY_CHANNEL_FLAG ); }
    bool            isSecondary() const { return !isPrimary(); }
    void            setVolumeRamp( bool direction, int leftVolume, int rightVolume )
    {
        setFlags( MXR_VOLUME_RAMP_IS_ACTIVE_FLAG );
        if( direction == MXR_VOLUME_RAMP_DOWN )
            setFlags( MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG );
        else 
            clearFlags( MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG );
    /*
        volume ramps occur on:
        - start of sample 
        - end of sample
        - volume change
        - panning change
    
    */
    }
    bool            isVolumeRamping() const { return isSet( MXR_VOLUME_RAMP_IS_ACTIVE_FLAG ); }
    bool            isVolumeRampingDown() const { return isSet( MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG ); }
    void            disableVolumeRamp()
    {
        clearFlags( MXR_VOLUME_RAMP_IS_ACTIVE_FLAG | MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG );
    }
    void            setVolume( float leftVolume, float rightVolume )
    {
        // clearFlags( MXR_SURROUND_IS_ACTIVE_FLAG ); // ???

        assert( leftVolume >= -1.0f );
        assert( rightVolume >= -1.0f );
        assert( leftVolume <= 1.0f );
        assert( rightVolume <= 1.0f );
        leftVolume_ = leftVolume;
        rightVolume_ = rightVolume;
    }
    void            setSurround() // to debug / verify
    {
        setFlags( MXR_SURROUND_IS_ACTIVE_FLAG );
        if ( rightVolume_ > 0 )
            rightVolume_ = -rightVolume_;
    }
    bool            isPlayingForwards() const 
    {
        return !isPlayingBackwards();
    }
    bool            isPlayingBackwards() const
    {
        return isSet( MXR_PLAYING_BACKWARDS_FLAG );
    }
    void            setPlayBackwards()
    {
        setFlags( MXR_PLAYING_BACKWARDS_FLAG );
    }
    void            setPlayForwards()
    {
        clearFlags( MXR_PLAYING_BACKWARDS_FLAG );
    }

    void            setFrequency( unsigned frequency )
    {
        frequencyInc_ = (float)frequency / (float)MXR_MIXRATE;
    }
    void            playSample(
        int logicalChannelNr,
        Instrument* pInstrument,
        Sample* pSample,
        unsigned offset,
        bool direction )
    {
        //clear();
        activate();
        if ( direction == FORWARD )
            setPlayForwards();
        else
            setPlayBackwards();
        pInstrument_ = pInstrument;
        pSample_ = pSample;
        offset_ = offset;
        age_ = 0;
        parentLogicalChannel_ = logicalChannelNr;
        fracOffset_ = 0;
        

        // added for envelope processing:
        //logicalChannels_[logicalChannelNr].iVolumeEnvelope = 0;
        //logicalChannels_[logicalChannelNr].iPanningEnvelope = 0;
        //logicalChannels_[logicalChannelNr].iPitchFltrEnvelope = 0;
        //logicalChannels_[logicalChannelNr].keyIsReleased = false;
        // end of addition for envelopes

        //physicalChannels_[physicalChannelNr].volumeRampDelta = VOLUME_RAMPING_UP;
        //physicalChannels_[physicalChannelNr].volumeRampCounter = VOLUME_RAMP_SPEED;
        //physicalChannels_[physicalChannelNr].volumeRampStart = 0;
    }

    int             getParentLogicalChannel() const { return parentLogicalChannel_; }
    Sample*         getSamplePtr() const { return pSample_; }
    Instrument*     getInstrumentPtr() const { return pInstrument_; }
    float           getLeftVolume() const { return leftVolume_; }
    float           getRightVolume() const { return rightVolume_; }
    float           getfrequencyInc() const { return frequencyInc_; }
    unsigned        getOffset() const { return offset_; }
    float           getFracOffset() const { return fracOffset_; }

    void            setOffset( unsigned offset )
    {
        assert( offset <= pSample_->getLength() );
        offset_ = offset;
    }
    void            setFracOffset( float fracOffset )
    {
        fracOffset_ = fracOffset;
    }

private:
    void            setFlags( const int flags ) { flags_ |= flags; }
    void            clearFlags( const int flags ) { flags_ &= 0xFFFFFFFF - flags; }
    bool            isSet( const int flag ) const { return (flags_ & flag) != 0; }

//    friend class Mixer;

private:
    std::uint16_t   flags_;
    std::uint16_t   parentLogicalChannel_;// for a.o. IT effect S7x (NNA process changes)
    float           rampLeftVolume_;    // current left side volume during ramp
    float           rampRightVolume_;   // current right side volume during ramp
    float           rampLeftInc_;       // -4 .. 4 (approximately)
    float           rampRightInc_;      // -4 .. 4 (approximately)
    float           leftVolume_;        // 0 .. 1
    float           rightVolume_;       // -1 .. 1: negative volume for surround
    float           frequencyInc_;      // frequency / mixRate
    std::uint16_t   fadeOut_;           // 65535 .. 0 // convert to float?
    std::uint16_t   volEnvIdx_;         // 0 .. 65535
    std::uint16_t   panEnvIdx_;         // 0 .. 65535
    std::uint16_t   PitchEnvIdx_;       // 0 .. 65535
    unsigned        offset_;            // non fractional part of the offset
    float           fracOffset_;        // fractional part of the offset
    Sample*         pSample_;           // for the sample data
    Instrument*     pInstrument_;       // for the envelopes

    // might be removed later:
    unsigned        age_;
};

// just a small struct to keep track of info for each logical channel
class LogicalChannelInfo {
public:
    LogicalChannelInfo() { clear(); }
    void            clear()
    {
        globalVolume = 1.0;
        volume = 1.0;
        panning = MXR_PANNING_CENTER;
        physicalChannelNr = MXR_NO_PHYSICAL_CHANNEL_ATTACHED;
    }

public:
    float           globalVolume;       // 0 .. 1
    float           volume;             // 0 .. 1
    int             panning;            // 0 .. 255
    int             physicalChannelNr;  // 0 .. MXR_MAX_PHYSICAL_CHANNELS - 1, or -1 for none
};


/******************************************************************************
*******************************************************************************
*                                                                             *
*   Logical channel Class Definition                                          *
*                                                                             *
*   The logical channels keep track of data that is essential to the replay   *
*   routines, such as effect, note and instrument memory and parameters.      *
*                                                                             *
*******************************************************************************
******************************************************************************/
class Channel {
public:
    Instrument* pInstrument;
    Sample* pSample;
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
    Channel()
    {
        init();
    }
    void            init()
    {
        memset( this,0,sizeof( Channel ) );
    }
};

/******************************************************************************
*******************************************************************************
*                                                                             *
*   Mixer Class Definition                                                    *
*                                                                             *
*   The main mixer class. This class includes both the mixer itself and the   *
*   replay routines.                                                          *
*                                                                             *
*******************************************************************************
******************************************************************************/
class Mixer {
    /**************************************************************************
    *                                                                         *
    *   FUNCTIONS FOR THE MIXER:                                              *
    *                                                                         *
    **************************************************************************/
public:
    Mixer();
    ~Mixer();

    /*
        global functions:
    */
    void            assignModule( Module* module );
    int             startReplay();
    int             stopReplay();
    void            updateWaveBuffers();
    void            resetMixer()
    {
        mixIndex_ = 0;
        mixCount_ = 0;
        for ( unsigned i = 0; i < MXR_MAX_PHYSICAL_CHANNELS; i++ )
            physicalChannels_[i].clear();
    }

    /*
        global replay commands
    */
    // global volume input range: 0..128
    void            setGlobalVolume( int globalVolume )
    {
        assert( globalVolume >= 0 );
        assert( globalVolume <= MAX_GLOBAL_VOLUME );
        mxr_globalVolume_ = (float)globalVolume / (float)MAX_GLOBAL_VOLUME;
        recalcChannelVolumes();
    }
    // global panning input range: 0..255
    void            setGlobalPanning( int globalPanning )
    {
        globalPanning = std::min( MXR_PANNING_FULL_RIGHT,globalPanning );
        globalPanning = std::max( MXR_PANNING_FULL_LEFT,globalPanning );
        recalcChannelVolumes();
    }
    // global balance input range: -100 .. +100
    void            setGlobalBalance( float globalBalance )
    {
        // calculate left & right balance multiplication factor (range 0 .. 1)
        globalBalance = std::min( 100.0f,globalBalance );
        globalBalance = std::max( -100.0f,globalBalance );
        if ( globalBalance <= 0 ) {
            mxr_leftGlobalBalance_ = 1.0f;
            mxr_rightGlobalBalance_ = (100.0f + globalBalance) / 100.0f;
        } else {
            mxr_leftGlobalBalance_ = (100.0f - globalBalance) / 100.0f;
            mxr_rightGlobalBalance_ = 1.0f;
        }
        recalcChannelVolumes();
    }


    void            setInterpolationType( int InterPolationType ) 
    {
        assert( InterPolationType >= 0 );
        assert( InterPolationType < MXR_INTERPOLATION_TYPES );

    }
    void            setTempo( int tempo ) // set BPM
    {
        callBpm_ = (MXR_MIXRATE * 5) / (tempo << 1);
    }
    //void            setSpeed() {} // set nr of ticks / beat
    //void            delaySpeed( int nrNticks ) {}

    /*
        logical channel commands:
    */
    void            setNNAMode( int logicalChannelNr,int NNA,int DCT ) {}
    /*
        volume range input: 0 .. 64
    */
    void            setChannelVolume( int logicalChannelNr,int volume )
    {
        assert( isValidLogicalChannelNr( logicalChannelNr ) );
        assert( volume >= 0 );
        assert( volume <= MAX_VOLUME );
        logicalChannels_[logicalChannelNr].volume = 
            (float)volume / (float)MAX_VOLUME;

        int physicalChannelNr = logicalChannels_[logicalChannelNr].physicalChannelNr;
        if ( isValidPhysicalChannelNr( physicalChannelNr ) ) {
            int masterChannelNr = physicalChannels_[physicalChannelNr].getParentLogicalChannel();
            if ( (masterChannelNr == logicalChannelNr) &&
                physicalChannels_[physicalChannelNr].isPrimary() )
                calculatePhysicalChannelVolume( physicalChannelNr );
        }

    }
    void            setChannelGlobalVolume( int logicalChannelNr,int channelGlobalVolume ) 
    {
        assert( isValidLogicalChannelNr( logicalChannelNr ) );
        assert( channelGlobalVolume >= 0 );
        assert( channelGlobalVolume <= MAX_VOLUME );   // 64? or 128?
        logicalChannels_[logicalChannelNr].globalVolume = 
            (float)channelGlobalVolume / (float)MAX_VOLUME;

        int physicalChannelNr = logicalChannels_[logicalChannelNr].physicalChannelNr;
        if ( isValidPhysicalChannelNr( physicalChannelNr ) )
            calculatePhysicalChannelVolume( physicalChannelNr );
    }
    void            setPanning( int logicalChannelNr,int panning )
    {
        assert( isValidLogicalChannelNr( logicalChannelNr ) );
        LogicalChannelInfo& logicalChannelInfo = logicalChannels_[logicalChannelNr];

        assert( panning >= MXR_PANNING_FULL_LEFT );
        assert( panning <= MXR_PANNING_FULL_RIGHT );
        logicalChannelInfo.panning = panning;

        int physicalChannelNr = getPhysicalChannelNr( logicalChannelNr );
        if ( !isValidPhysicalChannelNr( physicalChannelNr ) )
            return;
        // recalculate volume on the physical channel
        int masterChannelNr = physicalChannels_[physicalChannelNr].getParentLogicalChannel();
        if ( (masterChannelNr == logicalChannelNr) &&
            physicalChannels_[physicalChannelNr].isPrimary() )
            calculatePhysicalChannelVolume( physicalChannelNr );
    }
    void            setSurround( int logicalChannelNr )
    {
        setPanning( logicalChannelNr,MXR_PANNING_CENTER );
        int physicalChannelNr = getPhysicalChannelNr( logicalChannelNr );
        if ( physicalChannelNr == MXR_NO_PHYSICAL_CHANNEL_ATTACHED )
            return;

        physicalChannels_[physicalChannelNr].setSurround();
        //physicalChannels_[physicalChannelNr].rightVolume_ =
        //    -physicalChannels_[physicalChannelNr].rightVolume_;
    }
    void            setFrequency( int logicalChannelNr,unsigned frequency ) 
    {
        int physicalChannelNr = getPhysicalChannelNr( logicalChannelNr );
        if ( !isValidPhysicalChannelNr( physicalChannelNr ) )
            return;
        int masterChannelNr = physicalChannels_[physicalChannelNr].getParentLogicalChannel();
        if ( (masterChannelNr == logicalChannelNr) &&
            physicalChannels_[physicalChannelNr].isPrimary() )
            physicalChannels_[physicalChannelNr].setFrequency( frequency );
        //else
        //    throw("Trying to set frequency in inactive channel!");
    }


    void            playSample(
        int logicalChannelNr,
        Instrument* pInstrument,
        Sample* pSample,
        unsigned offset,
        bool direction )
    {
        assert( pInstrument != nullptr );
        assert( pSample != nullptr );
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
        stopChannelReplay( logicalChannelNr );

        // find an empty slot in mixer channels table
        int physicalChannelNr = getFreePhysicalChannel();

        /*
            "no free channel found" logic here
            find oldest channel logic and use that instead
        */
        if ( !isValidPhysicalChannelNr( physicalChannelNr ) ) {
            std::cout << "\nFailed to allocate mixer channel!\n";
            return;
        }

        attachPhysicalChannel( logicalChannelNr,physicalChannelNr );

        physicalChannels_[physicalChannelNr].activate();
        physicalChannels_[physicalChannelNr].makePrimary();
        physicalChannels_[physicalChannelNr].playSample(
            logicalChannelNr,pInstrument,pSample,offset,direction );
        calculatePhysicalChannelVolume( physicalChannelNr );
    }

    void            stopChannelReplay( int logicalChannelNr )
    {
        assert( isValidLogicalChannelNr( logicalChannelNr ) );
        int physicalChannelNr = getPhysicalChannelNr( logicalChannelNr );

        // exit if no channel is attached:
        if ( !isValidPhysicalChannelNr( physicalChannelNr ) )
            return;

        // exit if the channel is already deactivated:
        if ( !physicalChannels_[physicalChannelNr].isActive() )
            return;

        // check if the logical parent channel of the physical channel
        // is still this logical channel. If not, we can't stop it!
        // also, we can only stop primary channels!
        int masterChannel = physicalChannels_[physicalChannelNr].getParentLogicalChannel();
        if ( (masterChannel != logicalChannelNr) || 
            physicalChannels_[physicalChannelNr].isSecondary() )
            return;

        detachPhysicalChannel( logicalChannelNr );
        physicalChannels_[physicalChannelNr].makeSecondary();

        // temporary, should start volume ramp etc:
        physicalChannels_[physicalChannelNr].deactivate();        
    }

private:
    bool            isValidLogicalChannelNr( int logicalChannelNr ) const 
    {
        return
            (logicalChannelNr >= 0) &&
            (logicalChannelNr < MXR_MAX_LOGICAL_CHANNELS);
    }
    bool            isValidPhysicalChannelNr( int physicalChannelNr ) const
    {
        return
            (physicalChannelNr >= 0 ) &&
            (physicalChannelNr < MXR_MAX_PHYSICAL_CHANNELS );
    }
    void            attachPhysicalChannel( int logicalChannelNr,int physicalChannelNr )
    {
        assert( isValidLogicalChannelNr( logicalChannelNr ) );
        assert( isValidPhysicalChannelNr( physicalChannelNr ) );
        logicalChannels_[logicalChannelNr].physicalChannelNr = physicalChannelNr;
    }
    void            detachPhysicalChannel( int logicalChannelNr )
    {
        // insert safeguard for continuous playing sample (NNA logic) here?
        assert( isValidLogicalChannelNr( logicalChannelNr ) );
        logicalChannels_[logicalChannelNr].physicalChannelNr = 
            MXR_NO_PHYSICAL_CHANNEL_ATTACHED;
    }
    bool            isPhysicalChannelAttached( int logicalChannelNr ) // not necessary?
    {
        int physicalChannelNr = getPhysicalChannelNr( logicalChannelNr );
        if ( !isValidPhysicalChannelNr( physicalChannelNr ) )
            return false;
        if ( !physicalChannels_[physicalChannelNr].isActive() )
            return false;
        int masterChannelNr = physicalChannels_[physicalChannelNr].getParentLogicalChannel();
        return (masterChannelNr == logicalChannelNr) &&
            physicalChannels_[physicalChannelNr].isPrimary();
    }

    // returns -1 if no channel found
    int             getFreePhysicalChannel() const
    {
        for ( int i = 0;i < MXR_MAX_PHYSICAL_CHANNELS;i++ )
            if ( !physicalChannels_[i].isActive() )
                return i;
        return MXR_NO_PHYSICAL_CHANNEL_ATTACHED;
    }

    // Might return -1 i.e. MXR_NO_PHYSICAL_CHANNEL_ATTACHED
    int             getPhysicalChannelNr( int logicalChannelNr ) const
    {
        assert( isValidLogicalChannelNr( logicalChannelNr ) );
        return logicalChannels_[logicalChannelNr].physicalChannelNr;
    }

    /*
        We need to take into account:
        - Gain
        - Global Volume
        - Global Channel Volume
        - Current Channel Volume
        - Global Sample Volume

        - Instrument Volume      // todo
        - Volume Envelope Value  // todo
        - FadeOut Value          // todo

        - Global Panning
        - Global Balance
    */
    void            calculatePhysicalChannelVolume( int physicalChannelNr )
    {
        assert( isValidPhysicalChannelNr ( physicalChannelNr ) );
        if ( !physicalChannels_[physicalChannelNr].isActive() )
            return; // nothing to do here

        Sample* pSample = physicalChannels_[physicalChannelNr].getSamplePtr();
        Instrument* pInstrument = physicalChannels_[physicalChannelNr].getInstrumentPtr();

        assert( pInstrument != nullptr );
        assert( pSample != nullptr );

        int logicalChannelNr = physicalChannels_[physicalChannelNr].getParentLogicalChannel();

        assert( isValidLogicalChannelNr ( logicalChannelNr ) );

        LogicalChannelInfo& logicalChannelInfo = logicalChannels_[logicalChannelNr];

        float finalVolume =
            //mxr_gain_ *
            mxr_globalVolume_ *
            //logicalChannelInfo.globalVolume *
            logicalChannelInfo.volume *
            (float)(pSample->getGlobalVolume()) / (float)64.0f;

        int finalPanning = getFinalPanning( logicalChannelInfo.panning );

        physicalChannels_[physicalChannelNr].setVolume(
            finalVolume *
            mxr_leftGlobalBalance_ *
            (float)(MXR_PANNING_FULL_RIGHT - finalPanning) / (float)MXR_PANNING_FULL_RIGHT,
            
            finalVolume *
            mxr_rightGlobalBalance_ *
            (float)finalPanning / (float)MXR_PANNING_FULL_RIGHT
        );
    }

    /*
        This function will simple recalculate all the volumes for every
        possible channel, usually because one of the following global
        mixer parameters changed:
        - the global volume
        - the global panning
        - the global balance
    */
    void            recalcChannelVolumes()
    {
        for ( int i = 0; i < MXR_MAX_PHYSICAL_CHANNELS; i++ ) {
            //if ( physicalChannels_[i].isActive() )  // check is done in next fn:
                calculatePhysicalChannelVolume( i );
        }
    }

    /*
        Calculates the final panning value of a logical channel based on:
        - the global panning of the mixer
        - the panning of the logical channel

        This then serves as a base for calculating the
        left & right volume for the mixer
    */
    int             getFinalPanning( int panning )
    {
        assert( panning >= MXR_PANNING_FULL_LEFT );
        assert( panning <= MXR_PANNING_FULL_RIGHT );
        int range = ((MXR_PANNING_CENTER - mxr_globalPanning_) << 1) + 1;
        /*
            Note that:

            MXR_PANNING_FULL_RIGHT - MXR_PANNING_FULL_LEFT + 1 == 256
            x >> 8 is the equivalent of x / 256
        */
        return ((256 - range) >> 1) + ((range * (panning + 1)) >> 8);
    }



    static void CALLBACK waveOutProc(
        HWAVEOUT hWaveOut,
        UINT uMsg,
        DWORD* dwInstance,
        DWORD dwParam1,
        DWORD dwParam2 );
 
    /**************************************************************************
    *                                                                         *
    *   ACTUAL MIXING ROUTINES:                                               *
    *                                                                         *
    **************************************************************************/
    /*
        Sinc Interpolation is yet to be implemented
    */


    int             doMixBuffer( DestBufferType* buffer );
    int             doMixSixteenbitStereo( unsigned nrSamples );

    // forwards playing mixing routines:
    void            doMixChannelForward( int channelNr,int nrSamples );
    void            MixMonoSampleForwardNoInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixMonoSampleForwardLinearInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixMonoSampleForwardCubicInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixMonoSampleForwardSincInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleForwardNoInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleForwardLinearInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleForwardCubicInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleForwardSincInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    // backwards playing mixing routines:
    void            MixMonoSampleBackwardNoInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixMonoSampleBackwardLinearInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixMonoSampleBackwardCubicInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixMonoSampleBackwardSincInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleBackwardNoInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleBackwardLinearInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleBackwardCubicInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );
    void            MixStereoSampleBackwardSincInterpolation(
        DestBufferType* pBuffer,
        std::int16_t* pSmpData,
        int nrSamples,
        float leftGain,
        float rightGain,
        float fracOffset,
        float freqInc
    );

    /**************************************************************************
    *                                                                         *
    *   VARIABLES FOR THE MIXER:                                              *
    *                                                                         *
    **************************************************************************/
private:

    /*
        Global volume as defined by the composer. range: 0..1
    */
    float           mxr_globalVolume_;

    /*
        Global panning:
            0   = full stereo
            127 = mono
            255 = full stereo with left & right channels reversed

        We usually set this value to 32 or 48 as extreme stereo
        separation sounds a bit awkward, especially when using
        headphones.
        This value is defined by the user of this program rather than
        by the composer of the song.

    */
    int             mxr_globalPanning_;

    /*
        Setting the balance further right will lower the left volume
        and vice versa.
        The globalbalance range is: -100 .. + 100
        The ranges for the values below are 0 .. 100
    */
    float           mxr_leftGlobalBalance_;
    float           mxr_rightGlobalBalance_;

    /*
        A gain of 1.0 equals full volume with no reduction. This is used
        to control the global volume as set by the user of this program
        rather than the composer of the song.
        - If the gain value is too high, the sound will be compressed and
        distortion may occur, so better use the volume controls of the
        operation system to increase the volume analogically.
        - If the gain value is too low, you might loose some detail, this
        should never be a big issue however as the player outputs 24 bit
        precision sound.
    */
    float           mxr_gain_;

    /*
        no interpolation
        linear interpolation
        cubic interpolation
        sinc interpolation
    */
    int             mxr_interpolationType_ = MXR_CUBIC_INTERPOLATION;

    std::uint16_t   tempo_;
    std::uint16_t   ticksPerRow_;

    unsigned        callBpm_;
    unsigned        mixCount_;
    unsigned        mixIndex_;

    std::unique_ptr < MixBufferType[] > mixBuffer_;

    static CRITICAL_SECTION     waveCriticalSection_;
    static WAVEHDR* waveBlocks_;
    static volatile int         waveFreeBlockCount_;
    static int                  waveCurrentBlock_;

    HWAVEOUT                    hWaveOut_;
    WAVEFORMATEXTENSIBLE        waveFormatExtensible_;
    WAVEFORMATEX                waveFormatEx_;

    LogicalChannelInfo          logicalChannels_[MXR_MAX_LOGICAL_CHANNELS];
    MixerChannel                physicalChannels_[MXR_MAX_PHYSICAL_CHANNELS];
    Module*                     module_ = nullptr;


    /**************************************************************************
    *                                                                         *
    *   FUNCTIONS FOR THE REPLAY ENGINE:                                      *
    *                                                                         *
    **************************************************************************/
public:
    void            updateNotes();
    void            updateImmediateEffects();
    void            updateEffects();

    unsigned        noteToPeriod( unsigned note,int finetune );
    unsigned        periodToFrequency( unsigned period );

    void            updateBpm();

    void            resetSong();

    /**************************************************************************
    *                                                                         *
    *   VARIABLES FOR THE REPLAY ENGINE:                                      *
    *                                                                         *
    **************************************************************************/
private:
    /*
        Number of logical channels as known by the replay routines
    */
    unsigned        nrChannels_;

    /*
        global volume as known by the replay routines
        range = 0..128
    */
    unsigned        globalVolume_;

    /*
        flags for the effect engine:
    */
    bool            st300FastVolSlides_;
    bool            st3StyleEffectMemory_;
    bool            itStyleEffects_;
    bool            ft2StyleEffects_;
    bool            pt35StyleEffects_;

    /*
        Effect handling related variables:
    */
    bool            patternLoopFlag_;
    int             patternLoopStartRow_;

    /*
        Speed / tempo related variables:
    */
    unsigned        tickNr_;
    //unsigned        ticksPerRow_;  // nr of ticks per BPM
    unsigned        patternDelay_;
    //unsigned        bpm_;

    /*
        Keep track of were we are in the song:
    */
    Pattern*        pattern_;
    const Note*     iNote_;
    unsigned        patternTableIdx_;
    unsigned        patternRow_;

    /*
        Effect & note paremeters, effect memory
    */
    Channel         channels_[MXR_MAX_LOGICAL_CHANNELS];
};

