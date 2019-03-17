#include <cstdio>
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <sys/stat.h>

#include "Module.h"

using namespace std;

Module::Module()
{
    fileName_.clear();
    songTitle_.clear();
    trackerTag_.clear();
    trackerType_ = TRACKER_IT;
    isLoaded_ = false;
    useLinearFrequencies_ = true;
    isCustomRepeat_ = false;
    minPeriod_ = 14;
    maxPeriod_ = 27392;
    panningStyle_ = PANNING_STYLE_IT;
    nChannels_ = 16;
    nInstruments_ = 1;
    nSamples_ = 1;
    nPatterns_ = 1;
    defaultTempo_ = 6;
    defaultBpm_ = 125;
    songLength_ = 1;
    songRestartPosition_ = 0;
    for ( int i = 0; i < PLAYER_MAX_CHANNELS; i++ )
    {
        defaultPanPositions_[i] = PANNING_CENTER;
    }
    // create an empty pattern of PLAYER_MAX_CHANNELS channels, 64 rows
    {
        Note *patternData = new Note[PLAYER_MAX_CHANNELS * 64];
        emptyPattern_.initialise( PLAYER_MAX_CHANNELS,64,patternData );
        for ( int i = 0; i < MAX_PATTERNS; i++ )
        {
            patterns_[i] = nullptr; //&emptyPattern_;
            patternTable_[i] = 0;
        }
    }
    // create an empty sample of 'sampleLength' bytes long
    {
        const unsigned sampleLength = 0x100;
        SampleHeader sampleHeader;
        sampleHeader.name = "empty sample";
        sampleHeader.length = sampleLength;
        sampleHeader.repeatLength = sampleLength;
        sampleHeader.volume = 64;
        SHORT sampleData[sampleLength];
        sampleHeader.data = (SHORT *)&sampleData;
        memset( sampleHeader.data,0,sampleLength * sizeof( SHORT ) );
        emptySample_.load( sampleHeader );
        for ( int i = 0; i < MAX_SAMPLES; i++ )
            samples_[i] = nullptr; //&emptySample_;
    }
    // create an empty instrument
    {
        InstrumentHeader instrumentHeader;
        instrumentHeader.name = "empty instrument";
        instrumentHeader.nSamples = 1;
        // getSample( 0 ) will return empty sample
        for ( int i = 0; i < MAXIMUM_NOTES; i++ )
            instrumentHeader.sampleForNote[i] = 0; 
        emptyInstrument_.load( instrumentHeader );
        for ( int i = 0; i < MAX_INSTRUMENTS; i++ )
            instruments_[i] = nullptr; //&emptyInstrument_;
    }
}

Module::~Module() {
//    cout << "\nDefault Module destructor called.";
    for ( int i = 0; i < MAX_SAMPLES; i++ ) delete samples_[i];
    for ( int i = 0; i < MAX_INSTRUMENTS; i++ ) delete instruments_[i];
    for ( int i = 0; i < MAX_PATTERNS; i++ ) delete patterns_[i];
    return;
}

int Module::loadFile() {
    int result;
    if ( !isLoaded() ) result = loadS3mFile();
    if ( !isLoaded() ) result = loadXmFile();
    if ( !isLoaded() ) result = loadItFile();
    if ( !isLoaded() ) result = loadModFile();
    return result;
}
