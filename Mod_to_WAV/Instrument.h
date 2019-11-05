#pragma once

#include <string>
#include <cassert>

#include "constants.h"

class EnvelopePoint {
public:
    unsigned        x = 0;
    unsigned        y = 0;
};

struct NoteSampleMap {
    unsigned char   note = 0;
    unsigned        sampleNr = 0;
};

class InstrumentHeader {
public:
    InstrumentHeader()
    {
        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) 
            sampleForNote[i].note = i;
    }
    std::string     name;
    unsigned        nSamples = 0;
    NoteSampleMap   sampleForNote[MAXIMUM_NOTES];
    EnvelopePoint   volumeEnvelope[12];  // only 12 envelope points?
    unsigned        nVolumePoints = 0;
    unsigned        volumeSustain = 0;
    unsigned        volumeLoopStart = 0;
    unsigned        volumeLoopEnd = 0;
    unsigned        volumeType = 0;
    unsigned        volumeFadeOut = 0;
    EnvelopePoint   panningEnvelope[12]; // only 12 envelope points?
    unsigned        nPanningPoints = 0;
    unsigned        panningSustain = 0;
    unsigned        panningLoopStart = 0;
    unsigned        panningLoopEnd = 0;
    unsigned        panningType = 0;
    unsigned        vibratoType = 0;
    unsigned        vibratoSweep = 0;
    unsigned        vibratoDepth = 0;
    unsigned        vibratoRate = 0;
};

class Instrument {
public:
    Instrument( const InstrumentHeader& instrumentHeader );
    std::string     getName() { return name_; }
    unsigned        getnSamples() { return nSamples_; }
    unsigned        getNoteForNote( unsigned n )
    {
        assert( n < MAXIMUM_NOTES );  // has no effect
        if ( n >= MAXIMUM_NOTES ) return 0;
        return sampleForNote_[n].note;
    }
    unsigned        getSampleForNote( unsigned n )
    {
        assert( n < MAXIMUM_NOTES );  // has no effect
        if ( n >= MAXIMUM_NOTES ) return 0;
        return sampleForNote_[n].sampleNr;
    }

    EnvelopePoint   getVolumeEnvelope( unsigned p ) { return volumeEnvelope_[p]; }
    unsigned        getnVolumePoints() { return nVolumePoints_; }
    unsigned        getVolumeSustain() { return volumeSustain_; }
    unsigned        getVolumeLoopStart() { return volumeLoopStart_; }
    unsigned        getVolumeLoopEnd() { return volumeLoopEnd_; }
    unsigned        getVolumeType() { return volumeType_; }
    unsigned        getVolumeFadeOut() { return volumeFadeOut_; }
    EnvelopePoint   getPanningEnvelope( unsigned p ) { return panningEnvelope_[p]; }
    unsigned        getnPanningPoints() { return nPanningPoints_; }
    unsigned        getPanningSustain() { return panningSustain_; }
    unsigned        getPanningLoopStart() { return panningLoopStart_; }
    unsigned        getPanningLoopEnd() { return panningLoopEnd_; }
    unsigned        getPanningType() { return panningType_; }
    unsigned        getVibratoType() { return vibratoType_; }
    unsigned        getVibratoSweep() { return vibratoSweep_; }
    unsigned        getVibratoDepth() { return vibratoDepth_; }
    unsigned        getVibratoRate() { return vibratoRate_; }

private:
    std::string     name_;
    unsigned        nSamples_;
    NoteSampleMap   sampleForNote_[MAXIMUM_NOTES];
    EnvelopePoint   volumeEnvelope_[12];
    unsigned        nVolumePoints_;
    unsigned        volumeSustain_;
    unsigned        volumeLoopStart_;
    unsigned        volumeLoopEnd_;
    unsigned        volumeType_;
    unsigned        volumeFadeOut_;
    EnvelopePoint   panningEnvelope_[12];
    unsigned        nPanningPoints_;
    unsigned        panningSustain_;
    unsigned        panningLoopStart_;
    unsigned        panningLoopEnd_;
    unsigned        panningType_;
    unsigned        vibratoType_;
    unsigned        vibratoSweep_;
    unsigned        vibratoDepth_;
    unsigned        vibratoRate_;
};
