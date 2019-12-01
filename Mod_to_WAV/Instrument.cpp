
#include <iostream>
#include <cassert>

#include "constants.h"
#include "instrument.h"

bool Envelope::envelopeStyle_;

Instrument::Instrument( const InstrumentHeader &instrumentHeader ) 
{
    assert( instrumentHeader.volumeEnvelope.nrNodes <= MAX_ENVELOPE_POINTS );
    assert( instrumentHeader.panningEnvelope.nrNodes <= MAX_ENVELOPE_POINTS );
    assert( instrumentHeader.pitchFltrEnvelope.nrNodes <= MAX_ENVELOPE_POINTS );

    assert( instrumentHeader.volumeEnvelope.sustainStart <= instrumentHeader.volumeEnvelope.nrNodes );
    assert( instrumentHeader.volumeEnvelope.sustainEnd <= instrumentHeader.volumeEnvelope.nrNodes );
    assert( instrumentHeader.volumeEnvelope.loopStart <= instrumentHeader.volumeEnvelope.nrNodes );
    assert( instrumentHeader.volumeEnvelope.loopEnd <= instrumentHeader.volumeEnvelope.nrNodes );

    assert( instrumentHeader.panningEnvelope.sustainStart <= instrumentHeader.panningEnvelope.nrNodes );
    assert( instrumentHeader.panningEnvelope.sustainEnd <= instrumentHeader.panningEnvelope.nrNodes );
    assert( instrumentHeader.panningEnvelope.loopStart <= instrumentHeader.panningEnvelope.nrNodes );
    assert( instrumentHeader.panningEnvelope.loopEnd <= instrumentHeader.panningEnvelope.nrNodes );

    assert( instrumentHeader.pitchFltrEnvelope.sustainStart <= instrumentHeader.pitchFltrEnvelope.nrNodes );
    assert( instrumentHeader.pitchFltrEnvelope.sustainEnd <= instrumentHeader.pitchFltrEnvelope.nrNodes );
    assert( instrumentHeader.pitchFltrEnvelope.loopStart <= instrumentHeader.pitchFltrEnvelope.nrNodes );
    assert( instrumentHeader.pitchFltrEnvelope.loopEnd <= instrumentHeader.pitchFltrEnvelope.nrNodes );

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

    if ( volumeEnvelope_.isEnabled() ) {
        std::cout
            << "\nVol env nodes: ";
        for ( int i = 0; i < volumeEnvelope_.nrNodes; i++ )
            std::cout << std::dec
            << (unsigned)volumeEnvelope_.nodes[i].x
            << ","
            << (unsigned)volumeEnvelope_.nodes[i].y
            << " ";
    }

}
