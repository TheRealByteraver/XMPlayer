#include <iostream>
#include <cstring>

#include "constants.h"
#include "instrument.h"

Instrument::Instrument( const InstrumentHeader &instrumentHeader ) 
{
    assert( instrumentHeader.volumeEnvelope.nrPoints <= MAX_ENVELOPE_POINTS );
    assert( instrumentHeader.panningEnvelope.nrPoints <= MAX_ENVELOPE_POINTS );
    assert( instrumentHeader.pitchFltrEnvelope.nrPoints <= MAX_ENVELOPE_POINTS );

    assert( instrumentHeader.volumeEnvelope.sustain <= instrumentHeader.volumeEnvelope.nrPoints );
    assert( instrumentHeader.volumeEnvelope.loopStart <= instrumentHeader.volumeEnvelope.nrPoints );
    assert( instrumentHeader.volumeEnvelope.loopEnd <= instrumentHeader.volumeEnvelope.nrPoints );

    name_ = instrumentHeader.name;
    for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
        sampleForNote_[i] = instrumentHeader.sampleForNote[i];
    }
    nSamples_           = instrumentHeader.nSamples;
    volumeEnvelope_     = instrumentHeader.volumeEnvelope;
    panningEnvelope_    = instrumentHeader.panningEnvelope;
    pitchFltrEnvelope_  = instrumentHeader.pitchFltrEnvelope;
    volumeFadeOut_      = instrumentHeader.volumeFadeOut;
    vibrato_            = instrumentHeader.vibratoConfig;
}
