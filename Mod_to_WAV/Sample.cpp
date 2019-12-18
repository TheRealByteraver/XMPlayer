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
        datalength_ = 2 * INTERPOLATION_SPACER + SAMPLEDATA_EXTENSION;
        datalength_ += 16;
        datalength_ &= 0xFFFFFFF0;
        data_ = std::make_unique<std::int16_t[]>( datalength_ );
        memset( data_.get(),0,datalength_ * sizeof( std::int16_t ) );
        return;
    }

    // load the sample
    length_ = sampleHeader.length;
    repeatOffset_ = sampleHeader.repeatOffset;
    repeatLength_ = sampleHeader.repeatLength;

    // MUST BE HERE!!!!!!!!!!
    repeatEnd_ = (sampleHeader.isRepeatSample ?
        (repeatOffset_ + repeatLength_) : sampleHeader.length);
    // !!!!!!!!!!!!!!!!!!

    if ( sampleHeader.isRepeatSample )
        flags_ |= SMP_REPEAT_FLAG;

    if ( sampleHeader.isPingpongSample )
        flags_ |= SMP_PINGPONG_FLAG;

    if ( sampleHeader.isSustainedSample )
        flags_ |= SMP_SUSTAIN_FLAG;

    if ( sampleHeader.isSustainedPingpongSample )
        flags_ |= SMP_PINGPONG_SUSTAIN_FLAG;

    if ( sampleHeader.isUsed )
        flags_ |= SMP_ISUSED_FLAG; // not used

    volume_ = sampleHeader.volume;
    relativeNote_ = sampleHeader.relativeNote;
    panning_ = sampleHeader.panning;
    finetune_ = sampleHeader.finetune;
    //c4Speed_          = sampleHeader.c4Speed;

    bool isUnsigned = (sampleHeader.dataType & SAMPLEDATA_IS_SIGNED_FLAG) == 0;
    bool is16Bit = (sampleHeader.dataType & SAMPLEDATA_IS_16BIT_FLAG) != 0;
    bool isStereo = (sampleHeader.dataType & SAMPLEDATA_IS_STEREO_FLAG) != 0;

    if( isStereo )
        flags_ |= SMP_IS_STEREO_FLAG;


    if ( isStereo ) std::cout << "\n!!! STEREO SAMPLE !!!\n";

    // allocate memory for 16 bit version of sample + some spare space
    datalength_ = length_ + 2 * INTERPOLATION_SPACER + SAMPLEDATA_EXTENSION;
    if ( isStereo )
        datalength_ += length_ + 2 * INTERPOLATION_SPACER;
    datalength_ += 16;
    datalength_ &= 0xFFFFFFF0;
    data_ = std::make_unique<std::int16_t[]>( datalength_ );


    unsigned nrSamples = length_;
    if ( isStereo )
        nrSamples *= 2;

    std::int16_t*   source16 = sampleHeader.data;
    signed char*    source8 = (signed char*)sampleHeader.data;       
    std::int16_t*   leftSource16 = source16;
    signed char*    leftSource8 = source8;
    std::int16_t*   rightSource16 = leftSource16 + length_;
    signed char*    rightSource8 = leftSource8 + length_;

    // convert from unsigned to signed:
    if ( isUnsigned ) {
        if ( is16Bit ) { // 16 bit unsigned data
            for ( unsigned i = 0; i < nrSamples;i++ ) {
                source16[i] = source16[i] ^ 0x8000;
            }
        }
        else { // 8 bit unsigned data
            for ( unsigned i = 0; i < nrSamples;i++ ) {
                source8[i] = source8[i] ^ 0x80;
            }
        }
    }

    // convert from left + right to 16 bit interleaved stereo and copy data:
    if ( isStereo ) { 
        std::int16_t* dest16 = data_.get() +  2 * INTERPOLATION_SPACER;
        if ( is16Bit ) { 
            for ( unsigned i = 0; i < length_;i++ ) {
                dest16[i * 2] = leftSource16[i];
                dest16[i * 2 + 1] = rightSource16[i];
            }
        } 
        else { // 8 bit data            
            for ( unsigned i = 0; i < length_;i++ ) {
                dest16[i * 2] = leftSource8[i] << 8;
                dest16[i * 2 + 1] = rightSource8[i] << 8;
            }
        }
    }
    // Mono sample. Just copy data and scale 8bit to 16 bit:
    else { 
        std::int16_t* dest16 = data_.get() + INTERPOLATION_SPACER;
        if ( is16Bit ) { 
            for ( unsigned i = 0; i < length_;i++ ) {
                dest16[i] = source16[i];
            }
        } 
        else { 
            for ( unsigned i = 0; i < length_;i++ ) {
                dest16[i] = source8[i] << 8;
            }
        }
    }

    // TO review (stereo samples):

    if ( INTERPOLATION_SPACER ) {
        std::int16_t* iData = data_.get() + INTERPOLATION_SPACER;
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
        std::int16_t* iData = data_.get() + INTERPOLATION_SPACER;
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
    flags_ = sourceSample.flags_;
    globalVolume_ = sourceSample.globalVolume_;
    volume_ = sourceSample.volume_;
    relativeNote_ = sourceSample.relativeNote_;
    panning_ = sourceSample.panning_;
    finetune_ = sourceSample.finetune_;

    data_ = std::make_unique<std::int16_t[]>( sourceSample.datalength_ );
    memcpy( data_.get(),sourceSample.data_.get(),sourceSample.datalength_ * sizeof( std::int16_t ) );
}
