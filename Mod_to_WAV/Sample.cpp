#include <iostream>
#include <cstring>
#include <conio.h>
#include <memory>

#include "Constants.h"
#include "Sample.h"
#include "Module.h"

Sample::Sample( const SampleHeader& sampleHeader )
{
    name_ = sampleHeader.name;

    // Create empty sample if no sample data was provided:
    if ( sampleHeader.data == nullptr ) {
        length_ = SAMPLEDATA_EXTENSION;
        unsigned k = 2 * INTERPOLATION_SPACER + SAMPLEDATA_EXTENSION;
        k += 16;
        k &= 0xFFFFFFF0;
        data_ = std::make_unique<SHORT[]>( k );
        memset( data_.get(),0,k * sizeof( SHORT ) );
        return;
    }

    // load the sample
    length_ = sampleHeader.length;
    repeatOffset_ = sampleHeader.repeatOffset;
    repeatLength_ = sampleHeader.repeatLength;
    isRepeatSample_ = sampleHeader.isRepeatSample;
    repeatEnd_ = (isRepeatSample_ ? (repeatOffset_ + repeatLength_) : sampleHeader.length);
    isPingpongSample_ = sampleHeader.isPingpongSample;
    isUsed_ = sampleHeader.isUsed;
    volume_ = sampleHeader.volume;
    relativeNote_ = sampleHeader.relativeNote;
    panning_ = sampleHeader.panning;
    finetune_ = sampleHeader.finetune;
    //c4Speed_          = sampleHeader.c4Speed;

    // allocate memory for 16 bit version of sample + some spare space
    unsigned k = length_ + 2 * INTERPOLATION_SPACER + SAMPLEDATA_EXTENSION;
    k += 16;
    k &= 0xFFFFFFF0;
    data_ = std::make_unique<SHORT[]>( k );

    switch ( sampleHeader.dataType ) {
        case SAMPLEDATA_SIGNED_16BIT:
        {
            SHORT* ps = (SHORT*)sampleHeader.data;
            SHORT* pd = (data_.get() + INTERPOLATION_SPACER);

            for ( unsigned j = 0; j < length_; j++ ) {
                *pd++ = *ps++;
            }
            break;
        }
        case SAMPLEDATA_SIGNED_8BIT:
        {
            signed char* ps = (signed char*)(sampleHeader.data);
            SHORT* pd = (data_.get() + INTERPOLATION_SPACER);

            for ( unsigned j = 0; j < length_; j++ ) {
                *pd++ = *ps++ << 8;
            }
            break;
        }
        default:
        {
            return;     // !!! 
        }
    }
    if ( INTERPOLATION_SPACER ) {
        SHORT* iData = data_.get() + INTERPOLATION_SPACER;
        for ( unsigned iSamples = INTERPOLATION_SPACER; iSamples; iSamples-- ) {
            data_[iSamples - 1] =
                data_[INTERPOLATION_SPACER] -
                iData[INTERPOLATION_SPACER - iSamples + 1];
                //data_[INTERPOLATION_SPACER + INTERPOLATION_SPACER - iSamples + 1];
        }

        for ( unsigned iSamples = 0; iSamples < INTERPOLATION_SPACER; iSamples++ ) {
            if ( sampleHeader.isRepeatSample ) {
                iData[repeatEnd_ + iSamples] = iData[repeatOffset_ + iSamples];
            }  /*
            else {
                iData[length_ + iSamples] =
                    iData[length_ - 1] - iData[length_ - 1 - iSamples];
            }    */
        }
    }
    // Make the end of the waveform converge to 0 (click removal):
    if ( !sampleHeader.isRepeatSample ) {
        SHORT* iData = data_.get() + INTERPOLATION_SPACER;
        for ( int i = 0; i < SAMPLEDATA_EXTENSION; i++ ) {
            int s = iData[length_ - 1 + i];
            s *= SAMPLEDATA_EXTENSION - 1 - i;
            s /= SAMPLEDATA_EXTENSION;
            iData[length_ + i] = ( abs( s ) < 128 ? 0 : s );
        }
        length_ += SAMPLEDATA_EXTENSION;
    }
}

void Sample::operator=( const Sample& sourceSample )
{
    name_ = sourceSample.name_;
    length_ = sourceSample.length_;
    repeatOffset_ = sourceSample.repeatOffset_;
    repeatEnd_ = sourceSample.repeatEnd_;
    repeatLength_ = sourceSample.repeatLength_;
    sustainRepeatStart_ = sourceSample.sustainRepeatStart_;
    sustainRepeatEnd_ = sourceSample.sustainRepeatEnd_;
    isRepeatSample_ = sourceSample.isRepeatSample_;
    isPingpongSample_ = sourceSample.isPingpongSample_;
    isSustainedSample_ = sourceSample.isSustainedSample_;
    isPingpongSustainedSample_ = sourceSample.isPingpongSustainedSample_;
    isUsed_ = sourceSample.isUsed_; 
    globalVolume_ = sourceSample.globalVolume_;
    volume_ = sourceSample.volume_;
    relativeNote_ = sourceSample.relativeNote_;
    panning_ = sourceSample.panning_;
    finetune_ = sourceSample.finetune_;

    unsigned totalLength = length_ + INTERPOLATION_SPACER * 2;
    data_ = std::make_unique<SHORT[]>( totalLength );
    memcpy( data_.get(),sourceSample.data_.get(),totalLength * sizeof( SHORT ) );
}
