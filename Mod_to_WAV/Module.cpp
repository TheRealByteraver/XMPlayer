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
#include "virtualfile.h"

Module::Module()
{
    // initialize channels' panning
    for ( int i = 0; i < PLAYER_MAX_CHANNELS; i++ ) 
        defaultPanPositions_[i] = PANNING_CENTER;

    // sample nr 0 is always a dummy sample
    samples_[0] = std::make_unique<Sample>( SampleHeader() );

    // instrument nr 0 is always a dummy instrument
    instruments_[0] = std::make_unique< Instrument >( InstrumentHeader() );
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
            << samples_[sampleNr]->getName()
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
    assert( isLoaded() == false );

    VirtualFile virtualFile( fileName_ );
    if ( virtualFile.getIOError() != NO_ERROR )
        return -1;

    int result = -1;
    if ( !isLoaded() ) 
        result = loadS3mFile( virtualFile );
    if ( !isLoaded() ) 
        result = loadXmFile( virtualFile );
    if ( !isLoaded() ) 
        result = loadItFile( virtualFile );
    if ( !isLoaded() ) 
        result = loadModFile( virtualFile );
    return result;
}

