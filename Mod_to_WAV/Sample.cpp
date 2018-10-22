#include <iostream>
#include <cstring>
#include <conio.h>

#include "Module.h"

using namespace std;

bool Sample::load(const SampleHeader &sampleHeader) {
    char        *s = sampleHeader.name;
    char        *d;
    unsigned    k = strlen(sampleHeader.name);
    unsigned    i = 0;

    if (k > MAX_SAMPLENAME_LENGTH) k = MAX_SAMPLENAME_LENGTH;
    name_ = new char[k + 1];
    name_[k] = '\0';
    d = name_;
    while (*s && (i < k)) { *d++ = *s++; i++; }
    *d = '\0';

    if (sampleHeader.data) { 
        length_           = sampleHeader.length;
        repeatOffset_     = sampleHeader.repeatOffset;
        repeatLength_     = sampleHeader.repeatLength;
        isRepeatSample_   = sampleHeader.isRepeatSample;
        repeatEnd_        = (isRepeatSample_ ? (repeatOffset_ + repeatLength_) : sampleHeader.length);
        isPingpongSample_ = sampleHeader.isPingpongSample;
        isUsed_           = sampleHeader.isUsed;
        volume_           = sampleHeader.volume;
        relativeNote_     = sampleHeader.relativeNote;
        panning_          = sampleHeader.panning;
        finetune_         = sampleHeader.finetune;
        
        // allocate memory for 16 bit version of sample + some spare space
        k = length_ + 2 * INTERPOLATION_SPACER;
        k += 16;
        k &= 0xFFFFFFF0; 
        data_ = new SHORT [k]; 

        switch (sampleHeader.dataType) {
            case SIGNED_SIXTEEN_BIT_SAMPLE : 
                {
                    SHORT  *ps = (SHORT *) sampleHeader.data;
                    SHORT  *pd = (data_ + INTERPOLATION_SPACER);

                    for (unsigned j = 0; j < length_; j++) {
                        *pd++ = *ps++;
                    }
                    break;
                }
            case SIGNED_EIGHT_BIT_SAMPLE : 
                {
                    signed char *ps = (signed char *)(sampleHeader.data);
                    SHORT       *pd = (data_ + INTERPOLATION_SPACER);

                    for (unsigned j = 0; j < length_; j++) {
                        *pd++ = *ps++ << 8;
                    }
                    break;
                }
            default :
                {
                    return false;
                }
        }
        if (INTERPOLATION_SPACER) {
            SHORT   *iData = data_ + INTERPOLATION_SPACER;
            for (unsigned iSamples = INTERPOLATION_SPACER; iSamples; iSamples--) {
                data_[iSamples - 1] = 
                    data_[INTERPOLATION_SPACER] -
                    iData[INTERPOLATION_SPACER - iSamples + 1];
                    //data_[INTERPOLATION_SPACER + INTERPOLATION_SPACER - iSamples + 1];
            }

            for (unsigned iSamples = 0; iSamples < INTERPOLATION_SPACER; iSamples++) {
                if (sampleHeader.isRepeatSample) {
                    iData[repeatEnd_ + iSamples] = iData[repeatOffset_ + iSamples];
                } else {
                    iData[length_ + iSamples] = 
                        iData[length_ - 1] - iData[length_ - 1 - iSamples];
                }
            }
        }
        return true;
    } 
    return false;
}

Sample::~Sample() {
    delete name_;
    delete data_;
}
