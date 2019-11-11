#pragma once

#include <string>
#include <cassert>

#include "constants.h"

/*
    range of x is normally 16 bit
    range of y is normally 7 bit (0..64)

*/
class EnvelopePoint {
public:
    unsigned short  x = 0;
    unsigned char   y = 0;
};

/*
    * Sustain: 
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
*/
class Envelope {
public:
    EnvelopePoint   points[MAX_ENVELOPE_POINTS];
    unsigned char   align = 0; // 32bit-align the next values
    unsigned char   nrPoints = 0;
    unsigned char   sustain = 0;
    unsigned char   loopStart = 0;
    unsigned char   loopEnd = 0;
public:
    // this function will modify framePos if needed (loop, sustain)
    int         getEnvelopeVal( unsigned& framePos,bool keyIsReleased ) const
    {
        // exit if envelope is not used / enabled
        if ( !isEnabled() )
            return MAX_VOLUME;

        unsigned xSustain = points[sustain].x;
        unsigned xLoopStart = points[loopStart].x;
        unsigned xLoopEnd = points[loopEnd].x;
        unsigned xFinal = points[(nrPoints == 0) ? 0 : (nrPoints - 1)].x;

        if ( keyIsReleased == false ) {

            // if we reached the sustain point then hold it there:
            if ( isSustained() && (framePos >= xSustain) ) {
                framePos = xSustain;
                return points[sustain].y;
            }

            // if not, check if we are past the envelope loop end.
            // if so, go to the start of the loop:
            if ( isLooped() && (framePos >= xLoopEnd) )  
                framePos = xLoopStart + xLoopEnd - framePos;
            
            // now interpolate the envelope value based on the
            // points before and after our position:
            int idx1 = 0;
            int idx2 = 0;
            for ( int i = 0; i < nrPoints; i++ ) {
                if ( framePos <= points[i].x )
                    break;
                idx1 = idx2;
                idx2 = i;
            }
            // interpolate value based on prev. & next points:
            int x1 = points[idx1].x;
            int y1 = points[idx1].y;
            int x2 = points[idx2].x;
            int y2 = points[idx2].y;

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
            for ( int i = 0; i < nrPoints; i++ ) {
                if ( framePos <= points[i].x )
                    break;
                idx1 = idx2;
                idx2 = i;
            }
            // interpolate value based on prev. & next points:
            int x1 = points[idx1].x;
            int y1 = points[idx1].y;
            int x2 = points[idx2].x;
            int y2 = points[idx2].y;

            if ( framePos > (unsigned)x2 ) { // avoid overshoot of last point
                framePos = x2;
                return y2;
            }
            if ( x2 == x1 ) // avoid division by zero :)
                return y1;

            return y1 + ((framePos - x1) * (y2 - y1)) / (x2 - x1);
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
    unsigned        nSamples = 0;
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
    unsigned        getnSamples() const { return nSamples_; }
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
    const Envelope& getVolumeEnvelope() const { return volumeEnvelope_; }
    const Envelope& getPanningEnvelope() const { return panningEnvelope_; }
    const Envelope& getPitchFltrEnvelope() const { return pitchFltrEnvelope_; }
    const VibratoConfig&  getVibrato() const { return vibrato_; }

private:
    std::string     name_;
    unsigned        nSamples_;
    NoteSampleMap   sampleForNote_[MAXIMUM_NOTES];
    Envelope        volumeEnvelope_;
    Envelope        panningEnvelope_;
    Envelope        pitchFltrEnvelope_;
    unsigned        volumeFadeOut_;
    VibratoConfig   vibrato_;
};
