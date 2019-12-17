#pragma once
// Floating point mixer

#include <climits>
#if CHAR_BIT != 8 
This code requires a byte to be 8 bits wide
#endif

#include <cstdint>

#include "Constants.h"
#include "Instrument.h"
#include "Sample.h"




class Mixer2Channel {
public:


private:
    /*
    possible flags:
    1 - if the channel is active
    2 - if a volume ramp is active (click removal)
    4 - if the volume is ramping down (volume ramps up otherwise)
    8 - if the sample is playing backwards
    
    
    */
    std::uint32_t   flags_;
    float           leftVolume_;    // -1 .. +1
    float           rightVolume_;   // -1 .. +1
    float           frequencyInc_;  // frequency / mixRate
    std::uint16_t   fadeOut_;       // 65535 .. 0
    std::uint16_t   volEnvIdx_;     // 0 .. 65535
    std::uint16_t   panEnvIdx_;     // 0 .. 65535
    std::uint16_t   PitchEnvIdx_;   // 0 .. 65535
    unsigned        offset_;        // non fractional part of the offset
    unsigned        fracOffset_;    // fractional part of the offset
    Sample*         pSample_;
    Instrument*     pInstrument;    // for the envelopes

};


class Mixer2 {
public:
    void            setGlobalVolume() {}
    void            setGlobalPanning() {}
    void            setGlobalBalance() {}
    void            setInterpolationType() {}
    void            setTempo() {} // set BPM
    void            setSpeed() {} // set nr of ticks / beat


private:
    std::uint8_t    globalVolume_;  // 0..128

    // Global panning: 0 = full stereo, 127 = mono, 255 = full reversed stereo
    std::uint8_t    globalPanning_; 

    // Setting the balance further right will only lower the left volume
    // and vice versa
    std::int8_t     globalBalance_; // -100 .. +100




};