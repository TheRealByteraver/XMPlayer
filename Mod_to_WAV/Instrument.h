#pragma once



/*
unsigned char   pitchPanSeparation; // -32 .. +32
unsigned char   pitchPanCenter; // C-0 .. B-9 <=> 0..119
unsigned char   globalVolume;   // 0..128
unsigned char   defaultPanning; // 0..64, don't use if bit 7 is set
unsigned char   randVolumeVariation; // expressed in percent
unsigned char   randPanningVariation;// not implemented

*/



#include <string>
#include <cassert>

#include "constants.h"

/*
    range of x is normally 16 bit
    range of y is normally 7 bit (0..64)

*/
class EnvelopePoint {
public:
    unsigned short  x = 0; // tickIndex
    unsigned char   y = 0; // magnitude
};

/*
    **********
    **********
    **********

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

    

    * Simple Sustain (XM): 
    The sustain value is an index in the envelope. If sustain is active,
    the volume should be kept at the sustain volume point until the key 
    is released (key off command).
    The envelope is processed normally but progress stops at the sustain
    point when that point is reached AND if sustain is active of course.
    
    If envelope loop is active, the sustain point can be:
    - before/ at the beginning of the loop: envelope loop is only used after
    key off
    - in the middle of the loop: loop only continues after key off
    - at the end of the loop (last point): envelope stops looping on key off
    and the rest of the envelope is processed
    - beyond the end of the loop: sustain has no effect

    * envelope loop: 
    Points after the loop end are not used, unless sustain point is at the
    same point of the loop end (see above)

    * fadeout: whenever the key off event occurs, the fadeout starts. The 
    envelope is still being processed though

    * Sustain loop (IT):
    - The sustain loop takes precedence over the envelope loop. 
    - When the sample is fired, envelope processing starts at the beginning
    of the envelope right up to the sustain loop end, then jumps back to the
    sustain loop start.
    - As soon as the key off event happens, the envelope position jumps to
    the envelope loop start (not the sustain loop start mind you).
    - If there is a sustain loop but no envelope loop, the sustain loop acts
    as en envelope loop: on key off the rest of the envelope is processed,
    and the fadeout only starts at the end of the envelope (!!!)
    - If there is an envelope loop but no sustain loop, the points after 
    the envelope loop end are ignored, a key off event only starts the 
    fadeout but the envelope keeps looping until the end of the fadeout

    From the MPT help file:

    **********************
    ## Envelope Loop: Enables the envelope loop. Envelope loops cannot be 
    stopped by Note Off events.
    ## Envelope Sustain: Enables the envelope sustain loop (or sustain point 
    in the XM format). Sustain loops are exited as soon as a Note Off event 
    occours.
    ## Envelope Carry: When triggering a new note, the envelope is not 
    re-played from the beginning, but rather “carried on”. Since some of 
    Impulse Tracker’s audio drivers ignore envelope carry if the NNA is set 
    to “Note Cut” (and some players replicate this behaviour), it is 
    recommended to always choose some other NNA value when using Envelope 
    Carry.

    In the XM format, it does not matter if the envelope loop is reached before
    the sustain point or vice versa: whichever of the two is reached first is 
    used for looping the envelope.

    When using IT compatible playback, though, the sustain loop is always 
    considered before the envelope loop, so if the sustain loop is placed 
    beyond the envelope loop, the envelope loop is ignored at first. As soon as
    the sustain loop is exited (by means of a Note Off event), playback is 
    resumed in the envelope loop.
    **********************

*/
class Envelope {
public:
    // this function will modify framePos if needed (loop, sustain)
    int         getEnvelopeVal( unsigned& framePos,bool keyIsReleased ) const
    {
        // exit if envelope is not used / enabled
        if ( !isEnabled() )
            return MAX_VOLUME;

        unsigned xSustain = nodes[sustainStart].x;
        unsigned xLoopStart = nodes[loopStart].x;
        unsigned xLoopEnd = nodes[loopEnd].x;
        unsigned xFinal = nodes[(nrNodes == 0) ? 0 : (nrNodes - 1)].x;

        if ( keyIsReleased == false ) {

            // if we reached the sustain point then hold it there:
            if ( isSustained() && (framePos >= xSustain) ) {
                framePos = xSustain;
                return nodes[sustainStart].y;
            }

            // if not, check if we are past the envelope loop end.
            // if so, go to the start of the loop:
            if ( isLooped() && (framePos >= xLoopEnd) )  
                framePos = xLoopStart + xLoopEnd - framePos;
            
            // now interpolate the envelope value based on the
            // points before and after our position:
            int idx1 = 0;
            int idx2 = 0;
            for ( int i = 0; i < nrNodes; i++ ) {
                if ( framePos <= nodes[i].x )
                    break;
                idx1 = idx2;
                idx2 = i;
            }
            // interpolate value based on prev. & next points:
            int x1 = nodes[idx1].x;
            int y1 = nodes[idx1].y;
            int x2 = nodes[idx2].x;
            int y2 = nodes[idx2].y;

            if ( framePos > (unsigned)x2 ) { // avoid overshoot of last point
                framePos = x2;
                return y2;
            }
            if ( x2 == x1 ) // avoid division by zero :)
                return y1;

            return y1 + ((framePos - x1) * (y2 - y1)) / (x2 - x1);
        }
        else { // key was released
            // check if we are past the envelope loop end.
            // if so, go to the start of the loop, except if
            // the sustain point coincides with the Loop end point,
            // in which case we progress through the rest of the envelope
            if ( isLooped() && (framePos >= xLoopEnd) ) {
                if ( xSustain != xLoopStart )
                    framePos = xLoopStart + xLoopEnd - framePos; // loop envelope
            }

            // now interpolate the envelope value based on the
            // points before and after our position:
            int idx1 = 0;
            int idx2 = 0;
            for ( int i = 0; i < nrNodes; i++ ) {
                if ( framePos <= nodes[i].x )
                    break;
                idx1 = idx2;
                idx2 = i;
            }
            // interpolate value based on prev. & next points:
            int x1 = nodes[idx1].x;
            int y1 = nodes[idx1].y;
            int x2 = nodes[idx2].x;
            int y2 = nodes[idx2].y;

            if ( framePos > (unsigned)x2 ) { // avoid overshoot of last point
                framePos = x2;
                return y2;
            }
            if ( x2 == x1 ) // avoid division by zero :)
                return y1;

            return y1 + ((framePos - x1) * (y2 - y1)) / (x2 - x1);
        }
    }


    int         getPrecedingNode( unsigned frameNr ) {
        // safety (a single node envelope is not really an envelope but ok):
        if ( nrNodes <= 1 )
            return 0;

        int nodeNr = 0;
        for ( ; nodeNr < nrNodes; nodeNr++ ) {

            // find our position in the envelope
            if ( frameNr >= nodes[nrNodes].x )
                break;
        }
        // check if we went past the last node:
        if ( nodeNr >= nrNodes )
            nodeNr--;
        return nodeNr;
    }
    int         getInterpolatedVal( unsigned frameNr ) 
    {
        int idx1 = getPrecedingNode( frameNr );

        // check if we arrived at the last envelope node:
        if ( idx1 == (nrNodes - 1) )
            return nodes[idx1].x;

        int idx2 = idx1 + 1;

        // interpolate value based on prev. & next points:
        int x1 = nodes[idx1].x;
        int y1 = nodes[idx1].y;
        int x2 = nodes[idx2].x;
        int y2 = nodes[idx2].y;

        if ( x2 == x1 ) // avoid division by zero :)
            return y1;

        return y1 + ((frameNr - x1) * (y2 - y1)) / (x2 - x1);
    }
    int         getXmEnvelopeVal( unsigned& frameNr,bool keyIsReleased )
    {
        // safety (a single node envelope is not really an envelope but ok):
        if ( nrNodes <= 1 )
            return nodes[0].y;

        bool checkSustain = isSustained() && (!keyIsReleased);

        // get the node we just passed:
        int nodeNr = getPrecedingNode( frameNr );



        if ( isLooped() ) { 

            if ( !checkSustain ) { 

                // loop the envelope before and after key off, till fade out
                if ( nodeNr >= loopEnd )
                    frameNr = nodes[loopStart].x + frameNr - nodes[loopEnd].x;
                return getInterpolatedVal( frameNr );
            }
            // loop is sustained:
            else { 
                if ( nodeNr >= sustainStart ) { 
                    frameNr = nodes[sustainStart].x;
                    return nodes[sustainStart].y;
                }

                if ( nodeNr >= loopEnd )
                    frameNr = nodes[loopStart].x + frameNr - nodes[loopEnd].x;
                return getInterpolatedVal( frameNr );
            }           
        }
        // no loop in envelope:
        else { 
        }



















    }

    void        setFlags( unsigned char flags ) 
    { 
        flags_ = flags & 
            (   ENVELOPE_IS_ENABLED_FLAG |
                ENVELOPE_IS_SUSTAINED_FLAG |
                ENVELOPE_IS_LOOPED_FLAG );
    }
    void        enable() 
    {
        flags_ |= ENVELOPE_IS_ENABLED_FLAG;
    }
    void        disable()
    {
        flags_ &= 0xFFFFFFFF - ENVELOPE_IS_ENABLED_FLAG;
    }
    void        disableLoop() 
    {
        if ( isLooped() )
            flags_ ^= ENVELOPE_IS_LOOPED_FLAG;
    }
    void        disableSustain()
    {
        if ( isSustained() )
            flags_ ^= ENVELOPE_IS_SUSTAINED_FLAG;
    }
    bool        isEnabled() const
    {
        return (flags_ & ENVELOPE_IS_ENABLED_FLAG) != 0;
    }
    bool        isSustained() const
    {
        return (flags_ & ENVELOPE_IS_SUSTAINED_FLAG) != 0;
    }
    bool        isLooped() const
    {
        return (flags_ & ENVELOPE_IS_LOOPED_FLAG) != 0;
    }

public:
    EnvelopePoint   nodes[MAX_ENVELOPE_POINTS];
    unsigned char   align = 0;            // 32bit-align the next values
    unsigned char   nrNodes = 0;     
    unsigned char   sustainStart = 0;     // sustainStart
    unsigned char   sustainEnd = 0;
    unsigned char   loopStart = 0;
    unsigned char   loopEnd = 0;

private:
    unsigned char   flags_ = 0;
};

class VibratoConfig {
public:
    unsigned        type = 0;
    unsigned        sweep = 0;
    unsigned        depth = 0;
    unsigned        rate = 0;
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
    unsigned char   nnaType = NNA_NOTE_CUT;
    unsigned char   dctType = DCT_OFF;
    unsigned char   dcaType = DCA_CUT;
    unsigned char   initialFilterCutoff = 0;
    unsigned char   initialFilterResonance = 0;

    // copied directly from .IT instrument specification:
    unsigned char   pitchPanSeparation = 0;  // -32 .. +32
    unsigned char   pitchPanCenter = 0;      // C-0 .. B-9 <=> 0..119
    unsigned char   globalVolume = 128;      // 0..128
    unsigned char   defaultPanning = 128;    // 0..64, don't use if bit 7 is set
    unsigned char   randVolumeVariation = 0; // expressed in percent
    unsigned char   randPanningVariation = 0;// not implemented

    unsigned        nrSamples = 0;
    NoteSampleMap   sampleForNote[MAXIMUM_NOTES];
    unsigned        volumeFadeOut = 0;
    Envelope        volumeEnvelope;
    Envelope        panningEnvelope;
    Envelope        pitchFltrEnvelope;
    VibratoConfig   vibratoConfig;
};

class Instrument {
public:
    Instrument( const InstrumentHeader& instrumentHeader );
    std::string     getName() const { return name_; }
    unsigned        getNrSamples() const { return nrSamples_; }
    unsigned        getNoteForNote( unsigned n )  const
    {
        assert( n < MAXIMUM_NOTES );  // has no effect ??
        if ( n >= MAXIMUM_NOTES ) return 0;
        return sampleForNote_[n].note;
    }
    unsigned        getSampleForNote( unsigned n ) const
    {
        assert( n < MAXIMUM_NOTES );  // has no effect ??
        if ( n >= MAXIMUM_NOTES ) return 0;
        return sampleForNote_[n].sampleNr;
    }
    unsigned char   getNnaType() const { return nnaType_; }
    unsigned char   getDctType() const { return dctType_; }
    unsigned char   getDcaType() const { return dcaType_; }
    const Envelope& getVolumeEnvelope() const { return volumeEnvelope_; }
    const Envelope& getPanningEnvelope() const { return panningEnvelope_; }
    const Envelope& getPitchFltrEnvelope() const { return pitchFltrEnvelope_; }
    const VibratoConfig&  getVibrato() const { return vibrato_; }

private:
    std::string     name_;
    unsigned char   nnaType_;
    unsigned char   dctType_;
    unsigned char   dcaType_;
    unsigned char   initialFilterCutoff_;
    unsigned char   initialFilterResonance_;

    // copied directly from .IT instrument specification:
    unsigned char   pitchPanSeparation_;  // -32 .. +32
    unsigned char   pitchPanCenter_;      // C-0 .. B-9 <=> 0..119
    unsigned char   globalVolume_;        // 0..128
    unsigned char   defaultPanning_;      // 0..64, don't use if bit 7 is set
    unsigned char   randVolumeVariation_; // expressed in percent
    unsigned char   randPanningVariation_;// not implemented

    unsigned        nrSamples_;
    NoteSampleMap   sampleForNote_[MAXIMUM_NOTES];
    unsigned        volumeFadeOut_;
    VibratoConfig   vibrato_;
    Envelope        volumeEnvelope_;
    Envelope        panningEnvelope_;
    Envelope        pitchFltrEnvelope_;
};
