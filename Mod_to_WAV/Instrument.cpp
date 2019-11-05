#include <iostream>
#include <cstring>

#include "constants.h"
#include "instrument.h"

Instrument::Instrument( const InstrumentHeader &instrumentHeader ) {
    name_ = instrumentHeader.name;
    for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
        sampleForNote_[i] = instrumentHeader.sampleForNote[i];
    }
    for (int i = 0; i < 12; i++) {
        volumeEnvelope_ [i].x = instrumentHeader.volumeEnvelope [i].x;
        panningEnvelope_[i].x = instrumentHeader.panningEnvelope[i].x;
        volumeEnvelope_ [i].y = instrumentHeader.volumeEnvelope [i].y;
        panningEnvelope_[i].y = instrumentHeader.panningEnvelope[i].y;
    }

    nSamples_           = instrumentHeader.nSamples;
    nVolumePoints_      = instrumentHeader.nVolumePoints;
    volumeSustain_      = instrumentHeader.volumeSustain;
    volumeLoopStart_    = instrumentHeader.volumeLoopStart;
    volumeLoopEnd_      = instrumentHeader.volumeLoopEnd;
    volumeType_         = instrumentHeader.volumeType;
    volumeFadeOut_      = instrumentHeader.volumeFadeOut;
    nPanningPoints_     = instrumentHeader.nPanningPoints;
    panningSustain_     = instrumentHeader.panningSustain;
    panningLoopStart_   = instrumentHeader.panningLoopStart;
    panningLoopEnd_     = instrumentHeader.panningLoopEnd;
    panningType_        = instrumentHeader.panningType;
    vibratoType_        = instrumentHeader.vibratoType;
    vibratoSweep_       = instrumentHeader.vibratoSweep;
    vibratoDepth_       = instrumentHeader.vibratoDepth;
    vibratoRate_        = instrumentHeader.vibratoRate;                      
}
