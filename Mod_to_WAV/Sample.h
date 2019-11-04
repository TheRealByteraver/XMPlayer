#pragma once

#include "Constants.h"

/*
    The SampleHeader is a simple container for values that are used to
    initialize the Sample class with. 
*/
class SampleHeader {
public:
    SampleHeader()
    {
        name.clear();
        length = 0;
        repeatOffset = 0;
        repeatLength = 0;
        sustainRepeatStart = 0;
        sustainRepeatEnd = 0;
        isRepeatSample = false;
        isSustainedSample = false;
        isPingpongSample = false;
        isSustainedPingpongSample = false;
        isUsed = false;
        globalVolume = 64;
        volume = 64;
        relativeNote = 0;
        panning = PANNING_CENTER;
        finetune = 0;
        vibratoSpeed = 0;     // 0..64
        vibratoDepth = 0;     // 0..64
        vibratoWaveForm = 0;  // 0 = sine,1 = ramp down,2 = square,3 = random
        vibratoRate = 0;      // 0..64
        dataType = SAMPLEDATA_TYPE_UNKNOWN;
        data = nullptr;
    }
    std::string     name;
    unsigned        length;
    unsigned        repeatOffset;
    unsigned        repeatLength;
    bool            isRepeatSample;
    bool            isPingpongSample;
    bool            isSustainedSample;
    bool            isSustainedPingpongSample;
    bool            isUsed;           // if the sample is used in the song
    int             globalVolume;
    int             volume;
    int             relativeNote;
    unsigned        panning;
    int             finetune;
    int             dataType;         // 8 or 16 bit, compressed, etc
    unsigned        sustainRepeatStart;
    unsigned        sustainRepeatEnd;
    unsigned        vibratoSpeed;     // 0..64
    unsigned        vibratoDepth;     // 0..64
    unsigned        vibratoWaveForm;  // 0 = sine,1 = ramp down,2 = square,3 = random
    unsigned        vibratoRate;      // 0..64    
    SHORT* data;                      // only 16 bit samples allowed                    
};

class Sample {
public:
    Sample()
    {
        name_.clear();
        length_ = 0;
        repeatOffset_ = 0;
        repeatLength_ = 0;
        isRepeatSample_ = false;
        isPingpongSample_ = false;
        isSustainedSample_ = false;
        isPingpongSustainedSample_ = false;
        isUsed_ = false;
        volume_ = false;
        relativeNote_ = 0;
        panning_ = PANNING_CENTER;
        finetune_ = 0;
    }
    bool            load( const SampleHeader& sampleHeader );
    std::string     getName() { return name_; }
    unsigned        getLength() { return length_; }
    unsigned        getRepeatOffset() { return repeatOffset_; }
    unsigned        getRepeatLength() { return repeatLength_; }
    unsigned        getRepeatEnd() { return repeatEnd_; }
    bool            isRepeatSample() { return isRepeatSample_; }
    bool            isPingpongSample() { return isPingpongSample_; }
    bool            isUsed() { return isUsed_; }
    int             getVolume() { return volume_; }
    int             getRelativeNote() { return relativeNote_; }
    unsigned        getPanning() { return panning_; }
    int             getFinetune() { return finetune_; }
    SHORT* getData() { return data_.get() + INTERPOLATION_SPACER; }
private:
    std::string     name_;
    unsigned        length_;
    unsigned        repeatOffset_;
    unsigned        repeatEnd_;
    unsigned        repeatLength_;
    unsigned        sustainRepeatStart_;
    unsigned        sustainRepeatEnd_;
    bool            isRepeatSample_;
    bool            isPingpongSample_;
    bool            isSustainedSample_;
    bool            isPingpongSustainedSample_;
    bool            isUsed_;            // if the sample is used in the song
    int             globalVolume_;
    int             volume_;
    int             relativeNote_;
    unsigned        panning_;
    int             finetune_;
    std::unique_ptr<SHORT[]> data_;     // only 16 bit samples allowed
};

