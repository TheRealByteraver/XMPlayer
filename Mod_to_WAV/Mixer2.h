#pragma once
// Floating point mixer

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
        flags_ |= MXR_VOLUME_RAMP_IS_ACTIVE_FLAG;
        if( direction == MXR_VOLUME_RAMP_DOWN )
            flags_ |= MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG;
        else 
            flags_ &= 0xFFFFFFFF - MXR_VOLUME_RAMP_IS_DOWNWARDS_FLAG;
    /*
    
        volume ramps occur on:
        - start of sample 
        - end of sample
        - volume change
        - panning change
    
    */

        //if ( leftVolume )
        //    return;
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
    }

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
    void            setGlobalVolume() {}
    void            setGlobalPanning() {}
    void            setGlobalBalance() {}
    void            setInterpolationType() {}
    void            setTempo() {} // set BPM
    void            setSpeed() {} // set nr of ticks / beat
    void            setNNAMode( int logicalChannel,int NNA,int DCT ) {}
    void            setChannelVolume( int logicalChannel,int volume ) {}
    void            setChannelGlobalVolume( int logicalChannel,int channelGlobalVolume ) {}
    void            setPanning( int logicalChannel,int panning )
    {
        LogicalChannel& logChn = logicalChannels_[logicalChannel];
        int physChn = logChn.physicalChannelNr;
        if ( physChn == MXR_NO_PHYSICAL_CHANNEL_ATTACHED )
            return;

        logChn.panning = panning;

        float leftVolume;
        float rightVolume;

        // all logic must be here because the logical channel class has no 
        // knowledge of global panning, global balance etc

        physicalChannels_[physChn].setVolume( leftVolume,rightVolume );
    }

    void            playSample(
        int logicalChannel,
        int sampleNr,
        unsigned offset,
        unsigned frequency )
    {
    }


private:
    int             getFreeChannel() // returns -1 if no channel found
    {
        for ( int i = 0;i < MXR_MAX_PHYSICAL_CHANNELS;i++ )
            if ( !physicalChannels_[i].isActive() )
                return i;
        return -1;
    }
    int             getPhysicalChannel( int logicalChannel )
    {
        assert( logicalChannel >=  0 );
        assert( logicalChannel < MXR_MAX_LOGICAL_CHANNELS );
        return logicalChannels_[logicalChannel].physicalChannelNr;
    }

private:
    float           globalVolume_;  // 0..1

    // Global panning: 0 = full stereo, 127 = mono, 255 = full reversed stereo
    int             globalPanning_; 

    // Setting the balance further right will only lower the left volume
    // and vice versa
    int             globalBalance_; // -100 .. +100

    float           gain_;          // 1.0 = full volume, no reduction

    std::uint16_t   tempo_;
    std::uint16_t   speed_;

    LogicalChannel  logicalChannels_[MXR_MAX_LOGICAL_CHANNELS];
    Mixer2Channel   physicalChannels_[MXR_MAX_PHYSICAL_CHANNELS];
};

