/*
    .XM

    - No loop, no sustain:
    ---> follow the envelope till the last point and hold. Key off causes fade
    out, envelope vol remains that of the last point (nothing special here)

    - No loop, sustain:
    ---> follow the envelope till the sustain point. Hold till key off, then
    follow rest of the envelope till the last point, and hold till fade out
    kills the sound

    - Loop, no sustain:
    ---> loop the envelope before and after key off, till fade out

    - Loop, sustain point before beginning of loop:
    ---> Follow envelope till sustain, hold till key off, and continue till
    envelope loop end, then loop to the envelope loop start till fade out

    - Loop, sustain point at beginning of loop:
    ---> see previous item

    - Loop, sustain point in the middle of the loop:
    ---> see previous item

    - Loop, sustain point at end of loop:
    ---> Loop the envelope normally, continue beyond the envelope loop end
    after key off (key off disables loop of envelope)

    - Loop, sustain point beyond end of loop:
    ---> same as "Loop, no sustain" (ignore sustain)

    Bottom line: process envelope and envelope loop normally, hold at sustain
    and continue after key release, except if envelope loop end and sustain
    point coincide, then continue with the envelope part that follows the
    loop end / sustain (if any).

    **********
    **********
    **********

    .IT

    - No loop, no sustain:
    ---> follow the envelope till the end and execute fadeout. key off event
    has no effect, the envelope is always processed fully (!)

    - No loop, sustain point:
    ---> Process envelope till sustain and hold till key off, then process rest
    of envelope, start fadeout only at end of envelope

    - No loop, sustain loop:
    ---> Process envelope till sustain loop end and loop to sustain loop start
    until key off, the proceed till end of envelope, only then start fade out

    - Loop, sustain loop:
    ---> Loop the envelope using the sustain loop point(s) until key off, then
    immediately start fadeout AND jump to the envelope loop start position and
    loop the envelope using the normal envelope loop points. This means
    envelope points beyond the last loop point (sustain or normal) are never
    used.

    - Loop, no sustain:
    ---> Loop envelope, key off event starts fadeout, envelope keeps looping.
    Points beyond loop end are never used.


    Bottom line: process envelope normally, hold / loop at sustain, fade out
    only at the end of the envelope, unless an envelope loop is present, then
    proceed to envelope loop and start fade out immediately (on key off event).

*/

#include <cassert>

#include "constants.h"
#include "instrument.h"

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
}
