#pragma once
// Floating point mixer


#define NOMINMAX
#include <algorithm>

#include <climits>
#if CHAR_BIT != 8 
This code requires a byte to be 8 bits wide
#endif

#include <cstdint>
#include <assert.h>

#include "Constants.h"
#include "Instrument.h"
#include "Sample.h"

/*
possible flags:
 1 - if the channel is active
 2 - if a volume ramp is active (click removal)
 4 - if the volume is ramping down (volume ramps up otherwise)
 8 - if the sample is playing backwards
16 - if the sample is sustained
32 - if the sample is keyed off


*/

// constants for the mixerChannel:
const bool  MXR_VOLUME_RAMP_UP      = false;
const bool  MXR_VOLUME_RAMP_DOWN    = true;
const int   MXR_PANNING_FULL_LEFT   = 0;
const int   MXR_PANNING_CENTER      = 127;
const int   MXR_PANNING_FULL_RIGHT  = 255;
const int   MXR_NO_PHYSICAL_CHANNEL_ATTACHED = -1;

// flags for the mixerChannel:
const int   MXR_CHANNEL_IS_ACTIVE_FLAG          = 1;
const int   MXR_VOLUME_RAMP_IS_ACTIVE_FLAG      = 2;
const int   MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG   = 4;
const int   MXR_PLAYING_BACKWARDS_FLAG          = 8;
const int   MXR_SAMPLE_IS_SUSTAINED_FLAG        = 16;
const int   MXR_SAMPLE_IS_KEYED_OFF_FLAG        = 32;
const int   MXR_SAMPLE_IS_FADING_OUT_FLAG       = 64;

// constants for the mixer:
const int MXR_MIXRATE = 44100;          // in Hz
                                            
const int MXR_MAX_PHYSICAL_CHANNELS = 256;
const int MXR_MAX_LOGICAL_CHANNELS = 64;

/*
    The physical mixer channels play a sample while taking into account the
    volume, panning and pitch envelopes


*/

class Mixer2Channel {
public:
    Mixer2Channel() { clear(); }
    void            clear()
    {
        flags_ = 0;
        parentLogicalChannel = 0;
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

    void            setVolume( float leftVolume, float rightVolume )
    {
        leftVolume_ = leftVolume;
        rightVolume_ = rightVolume;
    }

    void            playSample(
        Instrument* pInstrument,
        Sample* pSample,
        unsigned offset,
        unsigned frequency )
    {
        clear();
        activate();
        pInstrument_ = pInstrument;
        pSample_ = pSample;
        offset_ = offset;
        fracOffset_ = 0;
        frequencyInc_ = frequency / (float)MXR_MIXRATE;


        // setvolumeramp
        
    }

private:
    void            setFlags( const int flags ) { flags_ |= flags; }
    void            clearFlags( const int flags ) { flags_ &= 0xFFFFFFFF - flags; }
    bool            isSet( const int flag ) const { return (flags_ & flag) != 0; }
    void            activate() { setFlags( MXR_CHANNEL_IS_ACTIVE_FLAG ); }
    void            deactivate() { clearFlags( MXR_CHANNEL_IS_ACTIVE_FLAG ); }
    bool            isVolumeRamping() const { return isSet( MXR_VOLUME_RAMP_IS_ACTIVE_FLAG ); }
    bool            isVolumeRampingDown() const { return isSet( MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG ); }
    void            disableVolumeRamp()
    {
        clearFlags( MXR_VOLUME_RAMP_IS_ACTIVE_FLAG | MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG );
        //Mixer2::globalPanning_ = 128;
    }

    friend class Mixer2;

private:
    std::uint16_t   flags_;
    std::uint16_t   parentLogicalChannel;// for IT effect S7x (NNA process changes)
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

};

// just a small struct to keep track of info for each logical channel
class LogicalChannel {
public:
    LogicalChannel() { clear(); }
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

class Mixer2 {
public:
//    Mixer2( void tickUpdateFunction() ) :
    Mixer2(  ) 

    {
        globalVolume_ = 1.0;
        globalPanning_ = 0x20;
        leftGlobalBalance_ = 1.0;
        rightGlobalBalance_ = 1.0;
        gain_ = 0.4f;
        tempo_ = 125;
        speed_ = 6;
    }

    /* 
        global commands:
    */
    void            setGlobalVolume( float globalVolume ) 
    {
        globalVolume_ = std::min( 1.0f,globalVolume );
        globalVolume_ = std::max( 0.0f,globalVolume_ );
        recalcChannelVolumes();
    }
    void            setGlobalPanning( int globalPanning )
    {
        globalPanning = std::min( MXR_PANNING_FULL_RIGHT,globalPanning );
        globalPanning = std::max( MXR_PANNING_FULL_LEFT,globalPanning );
        recalcChannelVolumes();
    }
    void            setGlobalBalance( float globalBalance )
    {
        // calculate left & right balance multiplication factor (range 0 .. 1)
        globalBalance = std::min( 100.0f,globalBalance );
        globalBalance = std::max( -100.0f,globalBalance );
        if ( globalBalance <= 0 ) {
            leftGlobalBalance_ = 1.0f;
            rightGlobalBalance_ = (100.0f + globalBalance) / 100.0f;
        }
        else {
            leftGlobalBalance_ = (100.0f - globalBalance) / 100.0f;
            rightGlobalBalance_ = 1.0f;
        }
        recalcChannelVolumes();
    }


    void            setInterpolationType() {}
    void            setTempo() {} // set BPM
    void            setSpeed() {} // set nr of ticks / beat
    void            delaySpeed( int nrNticks ) {}

    /*
        logical channel commands:
    */
    void            setNNAMode( int logicalChannelNr,int NNA,int DCT ) {}
    void            setChannelVolume( int logicalChannelNr,int volume )
    {

    }
    void            setChannelGlobalVolume( int logicalChannelNr,int channelGlobalVolume ) {}
    void            setPanning( int logicalChannelNr,int panning )
    {
        assert( logicalChannelNr >= 0 );
        assert( logicalChannelNr < MXR_MAX_LOGICAL_CHANNELS );
        LogicalChannel& logicalChannel = logicalChannels_[logicalChannelNr];

        assert( panning >= MXR_PANNING_FULL_LEFT );
        assert( panning <= MXR_PANNING_FULL_RIGHT );
        logicalChannel.panning = panning;

        int physicalChannelNr = getPhysicalChannelNr( logicalChannelNr );
        if ( physicalChannelNr == MXR_NO_PHYSICAL_CHANNEL_ATTACHED )
            return;
        // recalculate volume on the physical channel
        calculatePhysicalChannelVolume( physicalChannelNr );        
    }
    void            setSurround( int logicalChannelNr ) 
    {
        setPanning( logicalChannelNr,MXR_PANNING_CENTER );
        int physicalChannelNr = getPhysicalChannelNr( logicalChannelNr );
        if ( physicalChannelNr == MXR_NO_PHYSICAL_CHANNEL_ATTACHED )
            return;
        physicalChannels_[physicalChannelNr].rightVolume_ =
            -physicalChannels_[physicalChannelNr].rightVolume_;
    }
    void            setFrequency( int logicalChannelNr,int frequency ) {}

    void            playSample(
        int logicalChannel,
        int sampleNr,
        int instrumentNr,
        unsigned offset,
        unsigned frequency )
    {}

    void            stopChannel( int logicalChannelNr )
    {
        int physChn = getPhysicalChannelNr( logicalChannelNr );
        if ( physChn == MXR_NO_PHYSICAL_CHANNEL_ATTACHED )
            return;
    }

private:
    // returns -1 if no channel found
    int             getFreeChannel() const 
    {
        for ( int i = 0;i < MXR_MAX_PHYSICAL_CHANNELS;i++ )
            if ( !physicalChannels_[i].isActive() )
                return i;
        return -1;
    }

    // Might return -1 i.e. MXR_NO_PHYSICAL_CHANNEL_ATTACHED
    int             getPhysicalChannelNr( int logicalChannelNr ) const
    {
        assert( logicalChannel >=  0 );
        assert( logicalChannel < MXR_MAX_LOGICAL_CHANNELS );
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
        assert( physicalChannel >= 0 );
        assert( physicalChannel < MXR_MAX_PHYSICAL_CHANNELS );
        if ( !physicalChannels_[physicalChannelNr].isActive() )
            return; // nothing to do here

        assert( physicalChannels_[physicalChannelNr].pSample_ != nullptr );
        assert( physicalChannels_[physicalChannelNr].pInstrument_ != nullptr );

        int logicalChannelNr = physicalChannels_[physicalChannelNr].parentLogicalChannel;

        assert( logicalChannelNr >= 0 );
        assert( logicalChannelNr < MXR_MAX_LOGICAL_CHANNELS );

        LogicalChannel& logicalChannel = logicalChannels_[logicalChannelNr];

        float finalVolume =
            gain_ *
            globalVolume_ *
            logicalChannel.globalVolume *
            logicalChannel.volume *
            (float)(physicalChannels_[physicalChannelNr].pSample_->getGlobalVolume()) / (float)64.0f;

        int finalPanning = getFinalPanning( logicalChannel.panning );
    
        physicalChannels_[physicalChannelNr].leftVolume_ =
            finalVolume *
            leftGlobalBalance_ *
            (float)(MXR_PANNING_FULL_RIGHT - finalPanning) / (float)MXR_PANNING_FULL_RIGHT;

        physicalChannels_[physicalChannelNr].rightVolume_ =
            finalVolume *
            rightGlobalBalance_ *
            (float)finalPanning / (float)MXR_PANNING_FULL_RIGHT;
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
            if ( physicalChannels_[i].isActive() )
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
        int range = ((MXR_PANNING_CENTER - globalPanning_) << 1) + 1;
        /*
            Note that:

            MXR_PANNING_FULL_RIGHT - MXR_PANNING_FULL_LEFT + 1 == 256
            x >> 8 is the equivalent of x / 256
        */
        return ((256 - range) >> 1) + ((range * (panning + 1)) >> 8);
    }


private:
    /*
        Global volume as defined by the composer. range: 0..1
    */
    float           globalVolume_;  

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
    int             globalPanning_; 

    /* 
        Setting the balance further right will lower the left volume
        and vice versa.
        The globalbalance range is: -100 .. + 100
        The ranges for the values below are 0 .. 100
    */
    float           leftGlobalBalance_;  
    float           rightGlobalBalance_; 

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
    float           gain_; 

    std::uint16_t   tempo_;
    std::uint16_t   speed_;

    LogicalChannel  logicalChannels_[MXR_MAX_LOGICAL_CHANNELS];
    Mixer2Channel   physicalChannels_[MXR_MAX_PHYSICAL_CHANNELS];
};

