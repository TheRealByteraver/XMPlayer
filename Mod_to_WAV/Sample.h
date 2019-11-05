#pragma once

#include "Constants.h"

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
    int             globalVolume = 64;
    int             volume = 64;
    int             relativeNote = 0;
    unsigned        panning = PANNING_CENTER;
    int             finetune = 0;
    int             dataType = SAMPLEDATA_TYPE_UNKNOWN;// 8 or 16 bit, compressed, etc
    unsigned        sustainRepeatStart = 0;
    unsigned        sustainRepeatEnd = 0;
    unsigned        vibratoSpeed = 0; // 0..64
    unsigned        vibratoDepth = 0; // 0..64
    unsigned        vibratoWaveForm = 0;// 0 = sine,1 = ramp down,2 = square,3 = random
    unsigned        vibratoRate = 0;  // 0..64    
    SHORT*          data = nullptr;   // only 16 bit samples allowed                    
};

class Sample {
public:
    //bool            load( const SampleHeader& sampleHeader );
    Sample( const SampleHeader& sampleHeader );
    void operator=( const Sample& sourceSample );

    std::string     getName()           const { return name_; }
    unsigned        getLength()         const { return length_; }
    unsigned        getRepeatOffset()   const { return repeatOffset_; }
    unsigned        getRepeatLength()   const { return repeatLength_; }
    unsigned        getRepeatEnd()      const { return repeatEnd_; }
    bool            isRepeatSample()    const { return isRepeatSample_; }
    bool            isPingpongSample()  const { return isPingpongSample_; }
    bool            isUsed()            const { return isUsed_; }
    int             getVolume()         const { return volume_; }
    int             getRelativeNote()   const { return relativeNote_; }
    unsigned        getPanning()        const { return panning_; }
    int             getFinetune()       const { return finetune_; }
    SHORT*          getData()           const { return data_.get() + INTERPOLATION_SPACER; }
private:
    std::string     name_;
    unsigned        length_ = 0;
    unsigned        repeatOffset_ = 0;
    unsigned        repeatEnd_ = 0;
    unsigned        repeatLength_ = 0;
    unsigned        sustainRepeatStart_ = 0;
    unsigned        sustainRepeatEnd_ = 0;
    bool            isRepeatSample_ = false;
    bool            isPingpongSample_ = false;
    bool            isSustainedSample_ = false;
    bool            isPingpongSustainedSample_ = false;
    bool            isUsed_ = false; // if the sample is used in the song
    int             globalVolume_ = 64;
    int             volume_ = 64;
    int             relativeNote_ = 0;
    unsigned        panning_ = PANNING_CENTER;
    int             finetune_ = 0;
    std::unique_ptr<SHORT[]> data_;     // only 16 bit samples allowed
};

