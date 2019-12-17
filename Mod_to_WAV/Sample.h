#pragma once

#include <cstdint>
#include <memory>
#include "Constants.h"

const int   SMP_REPEAT_FLAG             = 1;
const int   SMP_PINGPONG_FLAG           = 2;
const int   SMP_SUSTAIN_FLAG            = 4;
const int   SMP_PINGPONG_SUSTAIN_FLAG   = 8;
const int   SMP_IS_STEREO_FLAG          = 16;
const int   SMP_ISUSED_FLAG             = 128;  // not used so far

/*
    The SampleHeader is a simple container for values that are used to
    initialize the Sample class with. 
*/
class SampleHeader {
public:
    std::string     name;
    unsigned        length = 0;
    unsigned        repeatOffset = 0;
    unsigned        repeatLength = 0;
    bool            isRepeatSample = false;
    bool            isPingpongSample = false;
    bool            isSustainedSample = false;
    bool            isSustainedPingpongSample = false;
    bool            isUsed = false;   // if the sample is used in the song
    unsigned        globalVolume = 64;
    unsigned        volume = 64;
    unsigned        relativeNote = 0;
    unsigned        panning = PANNING_CENTER;
    std::int8_t     finetune = 0;
    int             dataType = SAMPLEDATA_TYPE_UNKNOWN; // 8 or 16 bit, (un)signed, stereo
    unsigned        sustainRepeatStart = 0;
    unsigned        sustainRepeatEnd = 0;
    unsigned char   vibratoSpeed = 0; // 0..64
    unsigned char   vibratoDepth = 0; // 0..64
    unsigned char   vibratoWaveForm = 0;// 0 = sine,1 = ramp down,2 = square,3 = random
    unsigned char   vibratoRate = 0;  // 0..64    
    std::int16_t*   data = nullptr;   // if stereo, 1st left then right channel               
};

class Sample {
public:
    Sample( const SampleHeader& sampleHeader );
    void operator=( const Sample& sourceSample );

    std::string     getName()           const { return name_; }
    unsigned        getLength()         const { return length_; }
    unsigned        getRepeatOffset()   const { return repeatOffset_; }
    unsigned        getRepeatLength()   const { return repeatLength_; }
    unsigned        getRepeatEnd()      const { return repeatEnd_; }
    bool            isRepeatSample()    const { return (flags_ & SMP_REPEAT_FLAG) != 0; }
    bool            isPingpongSample()  const { return (flags_ & SMP_PINGPONG_FLAG) != 0; }
    bool            isSustained()       const { return (flags_ & SMP_SUSTAIN_FLAG) != 0; }
    bool            isPingpongSustained() const { return (flags_ & SMP_PINGPONG_SUSTAIN_FLAG) != 0; }
    bool            isStereo()          const { return (flags_ & SMP_IS_STEREO_FLAG) != 0; }
    bool            isUsed()            const { return (flags_ & SMP_ISUSED_FLAG) != 0; }
    int             getVolume()         const { return volume_; }
    int             getGlobalVolume()   const { return globalVolume_; }
    int             getRelativeNote()   const { return relativeNote_; }
    unsigned        getPanning()        const { return panning_; }
    int             getFinetune()       const { return finetune_; }
    std::int16_t*   getData()           const { return data_.get() + INTERPOLATION_SPACER; }
private:
    std::string     name_;
    unsigned        length_ = 0;
    unsigned        repeatOffset_ = 0;
    unsigned        repeatEnd_ = 0;
    unsigned        repeatLength_ = 0;
    unsigned        sustainRepeatStart_ = 0;
    unsigned        sustainRepeatEnd_ = 0;
    unsigned        flags_ = 0;
    int             globalVolume_ = 64;
    int             volume_ = 64;
    int             relativeNote_ = 0;
    unsigned        panning_ = PANNING_CENTER;
    int             finetune_ = 0;
    unsigned        datalength_ = 0;       // total memory allocated for this sample
    std::unique_ptr<std::int16_t[]> data_; // 16 bit signed only, stereo == interleaved
};

