#pragma once

#include <memory>
#include <cassert>
#include <vector>

#include "constants.h"

/*
    todo:
    - compress the pattern for smaller memory footprint   
*/

// These type definitions are needed by the replay routines:
class Effect {
public:
    unsigned char   effect;
    unsigned char   argument;
};

class Note {
public:
    Note() {
        note = 0;
        instrument = 0;
        for ( int i = 0; i < MAX_EFFECT_COLUMNS; i++ ) {
            effects[i].effect = 0;
            effects[i].argument = 0;
        }
    }
public:
    unsigned char   note;
    unsigned char   instrument;
    Effect          effects[MAX_EFFECT_COLUMNS];
};

class Pattern {
public:
    Pattern( unsigned nChannels,unsigned nRows, const std::vector<Note>& data ) :
        nChannels_( nChannels ),
        nRows_( nRows )
    {
        size_ = nChannels * nRows;
        assert( size_ > 0 );
        data_ = data;  // no pattern compression for now
    }
    unsigned        getnRows() 
    { 
        return nRows_; 
    }
    Note            getNote( unsigned n ) 
    { 
        assert( n < size_ );
        return data_[n]; 
    }    
    //- returns a pointer to the beginning of the pattern if row exceeds the
    //  maximum nr of rows in this particular pattern.    
    const Note* getRow( unsigned row )
    {
        assert( size_ > 0 );
        if ( row < nRows_ )
            return &(data_[row * nChannels_]);
        else
            return &(data_[0]);
    }

private:
    unsigned                nChannels_;
    unsigned                nRows_;
    unsigned                size_;
    std::vector<Note>       data_;
};


