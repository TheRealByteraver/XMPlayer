//#include <iostream>
//#include <cstring>

#include <cassert>

#include "constants.h"
#include "instrument.h"

Instrument::Instrument( const InstrumentHeader &instrumentHeader ) 
{
    assert( instrumentHeader.volumeEnvelope.nrPoints <= MAX_ENVELOPE_POINTS );
    assert( instrumentHeader.panningEnvelope.nrPoints <= MAX_ENVELOPE_POINTS );
    assert( instrumentHeader.pitchFltrEnvelope.nrPoints <= MAX_ENVELOPE_POINTS );

    assert( instrumentHeader.volumeEnvelope.sustainStart <= instrumentHeader.volumeEnvelope.nrPoints );
    assert( instrumentHeader.volumeEnvelope.sustainEnd <= instrumentHeader.volumeEnvelope.nrPoints );
    assert( instrumentHeader.volumeEnvelope.loopStart <= instrumentHeader.volumeEnvelope.nrPoints );
    assert( instrumentHeader.volumeEnvelope.loopEnd <= instrumentHeader.volumeEnvelope.nrPoints );

    assert( instrumentHeader.panningEnvelope.sustainStart <= instrumentHeader.panningEnvelope.nrPoints );
    assert( instrumentHeader.panningEnvelope.sustainEnd <= instrumentHeader.panningEnvelope.nrPoints );
    assert( instrumentHeader.panningEnvelope.loopStart <= instrumentHeader.panningEnvelope.nrPoints );
    assert( instrumentHeader.panningEnvelope.loopEnd <= instrumentHeader.panningEnvelope.nrPoints );

    assert( instrumentHeader.pitchFltrEnvelope.sustainStart <= instrumentHeader.pitchFltrEnvelope.nrPoints );
    assert( instrumentHeader.pitchFltrEnvelope.sustainEnd <= instrumentHeader.pitchFltrEnvelope.nrPoints );
    assert( instrumentHeader.pitchFltrEnvelope.loopStart <= instrumentHeader.pitchFltrEnvelope.nrPoints );
    assert( instrumentHeader.pitchFltrEnvelope.loopEnd <= instrumentHeader.pitchFltrEnvelope.nrPoints );

    name_                   = instrumentHeader.name;
    nnaType_                = instrumentHeader.nnaType;
    dctType_                = instrumentHeader.dctType;
    dcaType_                = instrumentHeader.dcaType;
    initialFilterCutoff_    = instrumentHeader.initialFilterCutoff;
    initialFilterResonance_ = instrumentHeader.initialFilterResonance;

    pitchPanSeparation_     = instrumentHeader.pitchPanSeparation;
    pitchPanCenter_         = instrumentHeader.pitchPanCenter;
    globalVolume_           = instrumentHeader.globalVolume;
    defaultPanning_         = instrumentHeader.defaultPanning;
    randVolumeVariation_    = instrumentHeader.randVolumeVariation;
    randPanningVariation_   = instrumentHeader.randPanningVariation;

    nrSamples_              = instrumentHeader.nrSamples;
    for ( int i = 0; i < MAXIMUM_NOTES; i++ ) 
        sampleForNote_[i] = instrumentHeader.sampleForNote[i];
    
    volumeFadeOut_          = instrumentHeader.volumeFadeOut;
    vibrato_ = instrumentHeader.vibratoConfig;
    volumeEnvelope_         = instrumentHeader.volumeEnvelope;
    panningEnvelope_        = instrumentHeader.panningEnvelope;
    pitchFltrEnvelope_      = instrumentHeader.pitchFltrEnvelope;
}
