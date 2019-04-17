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

//using namespace std;

const char *noteStrings[2 + MAXIMUM_NOTES] = {
    "---",
    "C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0",
    "C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1",
    "C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2",
    "C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3",
    "C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4",
    "C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5",
    "C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6",
    "C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7",
    "C-8","C#8","D-8","D#8","E-8","F-8","F#8","G-8","G#8","A-8","A#8","B-8",
    "C-9","C#9","D-9","D#9","E-9","F-9","F#9","G-9","G#9","A-9","A#9","B-9",
    "C-A","C#A","D-A","D#A","E-A","F-A","F#A","G-A","G#A","A-A","A#A","B-A",
    "==="
};



Module::Module()
{
    fileName_.clear();
    songTitle_.clear();
    trackerTag_.clear();
    trackerType_ = TRACKER_IT;
    showDebugInfo_ = false;
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
            patterns_[i] = nullptr; 
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
            samples_[i] = nullptr;
    }
    // create an empty instrument
    {
        InstrumentHeader instrumentHeader;
        instrumentHeader.name = "empty instrument";
        //instrumentHeader.nSamples = 1;
        // getSample( 0 ) will return empty sample
        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
            instrumentHeader.sampleForNote[i].note = i;
            instrumentHeader.sampleForNote[i].sampleNr = 0;
        }
        emptyInstrument_.load( instrumentHeader );
        for ( int i = 0; i < MAX_INSTRUMENTS; i++ )
            instruments_[i] = nullptr;
    }
}

void Module::playSampleNr( int sampleNr )
{
    if ( !samples_[sampleNr] ) {
        /*
        if ( showDebugInfo_ )
            std::cout 
                << "\nSample " << sampleNr << " is empty"
                << ", hit any key to continue...";
            _getch();
        */
        return;
    }
    if ( showDebugInfo_ )
        std::cout
            << "\nPlaying sample " << sampleNr
            << ", hit any key to continue...";
            /*
            << "\nSample " << sampleNr << ": name     = "
            << samples_[sampleNr]->getName().c_str()
            << "\nSample " << sampleNr << ": length   = "
            << samples_[sampleNr]->getLength()
            << "\nSample " << sampleNr << ": rep ofs  = "
            << samples_[sampleNr]->getRepeatOffset()
            << "\nSample " << sampleNr << ": rep len  = "
            << samples_[sampleNr]->getRepeatLength()
            << "\nSample " << sampleNr << ": volume   = "
            << samples_[sampleNr]->getVolume()
            << "\nSample " << sampleNr << ": finetune = "
            << samples_[sampleNr]->getFinetune()            
            */            
    if ( samples_[sampleNr]->getData() ) {
        HWAVEOUT        hWaveOut;
        WAVEFORMATEX    waveFormatEx;
        MMRESULT        result;
        WAVEHDR         waveHdr;
        waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
        waveFormatEx.nChannels = 1;
        waveFormatEx.nSamplesPerSec = 8363; // frequency
        waveFormatEx.wBitsPerSample = 16;
        waveFormatEx.nBlockAlign = waveFormatEx.nChannels *
            (waveFormatEx.wBitsPerSample >> 3);
        waveFormatEx.nAvgBytesPerSec = waveFormatEx.nSamplesPerSec *
            waveFormatEx.nBlockAlign;
        waveFormatEx.cbSize = 0;

        result = waveOutOpen( &hWaveOut,WAVE_MAPPER,&waveFormatEx,
            0,0,CALLBACK_NULL );
        if ( result != MMSYSERR_NOERROR ) {
            if ( showDebugInfo_ )
                std::cout << "\nError opening wave mapper!\n";
        } 
        else {
            int retry = 0;
            if ( (sampleNr == 1) && showDebugInfo_ )
                std::cout << "\nWave mapper successfully opened!\n";
            waveHdr.dwBufferLength = samples_[sampleNr]->getLength() *
                waveFormatEx.nBlockAlign;
            waveHdr.lpData = (LPSTR)(samples_[sampleNr]->getData());
            waveHdr.dwFlags = 0;
            result = waveOutPrepareHeader( hWaveOut,&waveHdr,
                sizeof( WAVEHDR ) );
            while ( (result != MMSYSERR_NOERROR) && (retry < 10) ) {
                retry++;
                if ( showDebugInfo_ )
                    std::cout << "\nError preparing wave mapper header!";
                if ( showDebugInfo_ ) {
                    switch ( result ) {
                        case MMSYSERR_INVALHANDLE:
                        {
                            std::cout 
                                << "\nSpecified device handle is invalid.";
                            break;
                        }
                        case MMSYSERR_NODRIVER:
                        {
                            std::cout 
                                << "\nNo device driver is present.";
                            break;
                        }
                        case MMSYSERR_NOMEM:
                        {
                            std::cout 
                                << "\nUnable to allocate or lock memory.";
                            break;
                        }
                        default:
                        {
                            std::cout 
                                << "\nOther unknown error " << result;
                        }
                    }
                }
                Sleep( 1 );
                result = waveOutPrepareHeader( hWaveOut,&waveHdr,
                    sizeof( WAVEHDR ) );
            }
            result = waveOutWrite( hWaveOut,&waveHdr,sizeof( WAVEHDR ) );
            retry = 0;
            while ( (result != MMSYSERR_NOERROR) && (retry < 10) ) {
                retry++;
                if ( showDebugInfo_ )
                    std::cout << "\nError writing to wave mapper!";
                if ( showDebugInfo_ ) {
                    switch ( result ) {
                        case MMSYSERR_INVALHANDLE:
                        {
                            std::cout 
                                << "\nSpecified device handle is invalid.";
                            break;
                        }
                        case MMSYSERR_NODRIVER:
                        {
                            std::cout 
                                << "\nNo device driver is present.";
                            break;
                        }
                        case MMSYSERR_NOMEM:
                        {
                            std::cout 
                                << "\nUnable to allocate or lock memory.";
                            break;
                        }
                        case WAVERR_UNPREPARED:
                        {
                            std::cout 
                                << "\nThe data block pointed to by the pwh"
                                << " parameter hasn't been prepared.";
                            break;
                        }
                        default:
                        {
                            std::cout 
                                << "\nOther unknown error " << result;
                        }
                    }
                }
                result = waveOutWrite( hWaveOut,&waveHdr,sizeof( WAVEHDR ) );
                Sleep( 10 );
            }
            _getch();
            waveOutUnprepareHeader( hWaveOut,&waveHdr,sizeof( WAVEHDR ) );
            waveOutReset( hWaveOut );
            waveOutClose( hWaveOut );
        }
    }
}

int Module::loadFile() {
    int result = -1;
    if ( !isLoaded() ) result = loadS3mFile();
    if ( !isLoaded() ) result = loadXmFile();
    if ( !isLoaded() ) result = loadItFile();
    if ( !isLoaded() ) result = loadModFile();
    return result;
}

Module::~Module()
{
    //    cout << "\nDefault Module destructor called.";
    for ( int i = 0; i < MAX_SAMPLES; i++ ) delete samples_[i];
    for ( int i = 0; i < MAX_INSTRUMENTS; i++ ) delete instruments_[i];
    for ( int i = 0; i < MAX_PATTERNS; i++ ) delete patterns_[i];
    return;
}
