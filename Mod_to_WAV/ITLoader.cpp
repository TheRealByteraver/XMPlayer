#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>

#include "Module.h"
#include "virtualfile.h"

#define debug_it_loader
#define debug_it_show_patterns
#define debug_it_play_samples


#ifdef debug_it_loader
#include <bitset>
#include <iomanip>
#endif

#define IT_MAX_CHANNELS                     64
#define IT_MAX_PATTERNS                     200
#define IT_MAX_SONG_LENGTH                  MAX_PATTERNS
#define IT_MAX_SAMPLES                      99
#define IT_MAX_INSTRUMENTS                  100 // ?

#define IT_STEREO_FLAG                      1
#define IT_VOL0_OPT_FLAG                    2
#define IT_INSTRUMENT_MODE                  4
#define IT_LINEAR_FREQUENCIES_FLAG          8
#define IT_OLD_EFFECTS_MODE                 16
#define IT_GEF_LINKED_EFFECT_MEMORY         32
#define IT_USE_MIDI_PITCH_CONTROLLER        64
#define IT_REQUEST_MIDI_CONFIG              128

#define IT_SONG_MESSAGE_FLAG                1
#define IT_MIDI_CONFIG_EMBEDDED             8

// sample flags
#define IT_SMP_ASSOCIATED_WITH_HEADER       1
#define IT_SMP_IS_16_BIT                    2
#define IT_SMP_IS_STEREO                    4 // not supported by Impulse Trckr
#define IT_SMP_IS_COMPRESSED                8
#define IT_SMP_LOOP_ON                      16
#define IT_SMP_SUSTAIN_LOOP_ON              32
#define IT_SMP_PINGPONG_LOOP_ON             64
#define IT_SMP_PINGPONG_SUSTAIN_LOOP_ON     128
#define IT_SIGNED_SAMPLE_DATA               1

// pattern flags
#define IT_END_OF_SONG_MARKER               255
#define IT_MARKER_PATTERN                   254 
#define IT_PATTERN_CHANNEL_MASK_AVAILABLE   128
#define IT_PATTERN_NOTE_PRESENT             1
#define IT_PATTERN_INSTRUMENT_PRESENT       2
#define IT_PATTERN_VOLUME_COLUMN_PRESENT    4
#define IT_PATTERN_COMMAND_PRESENT          8
#define IT_PATTERN_LAST_NOTE_IN_CHANNEL     16
#define IT_PATTERN_LAST_INST_IN_CHANNEL     32
#define IT_PATTERN_LAST_VOLC_IN_CHANNEL     64
#define IT_PATTERN_LAST_COMMAND_IN_CHANNEL  128
                                               
#pragma pack (1) 

struct ItFileHeader {
    char            tag[4];         // "IMPM"
    char            songName[26];
    unsigned short  phiLight;       // pattern row hilight info (ignore)
    unsigned short  songLength;
    unsigned short  nInstruments;
    unsigned short  nSamples;
    unsigned short  nPatterns;
    unsigned short  createdWTV;
    unsigned short  compatibleWTV;
    unsigned short  flags;
    unsigned short  special;
    unsigned char   globalVolume;   // 0..128
    unsigned char   mixingAmplitude;// 0..128
    unsigned char   initialSpeed;
    unsigned char   initialBpm;     // tempo
    unsigned char   panningSeparation;// 0..128, 128 == max separation
    unsigned char   pitchWheelDepth;// for MIDI controllers
    unsigned short  messageLength;
    unsigned        messageOffset;
    unsigned        reserved;
    unsigned char   defaultPanning[IT_MAX_CHANNELS];// 0..64, 100 = surround, +128 == disabled channel
    unsigned char   defaultVolume[IT_MAX_CHANNELS]; // 0..64
};

/*
    follows:
    - orderlist, songLength bytes long. Max ptn nr == 199. 255 = end of song, 254 = marker ptn
    - instrument Offset list (nr of instruments): 32 bit
    - sample Header Offset list (nr of samples) : 32 bit
    - pattern Offset list (nr of patterns)      : 32 bit

*/

/*
    --> unsigned char   sampleForNote[240]:
    Note-Sample/Keyboard Table.
    Each note of the instrument is first converted to a sample number
    and a note (C-0 -> B-9). These are stored as note/sample pairs
    (note first, range 0->119 for C-0 to B-9, sample ranges from
    1-99, 0=no sample
*/
struct ItOldInstHeader {
    char            tag[4];         // "IMPI"
    char            fileName[12];
    char            asciiz;
    unsigned char   flag;
    unsigned char   volumeLoopStart;// these are node numbers
    unsigned char   volumeLoopEnd;
    unsigned char   sustainLoopStart;
    unsigned char   sustainLoopEnd;
    unsigned short  reserved;
    unsigned short  fadeOut;        // 0..64
    unsigned char   NNA;            // New Note Action
    unsigned char   DNC;            // duplicate Note Check
    unsigned short  trackerVersion;
    unsigned char   nSamples;       // nr of samples in instrument
    unsigned char   reserved2;
    char            name[26];
    unsigned char   reserved3[6];
    unsigned char   sampleForNote[240];
    unsigned char   volumeEnvelope[200]; // format: tick,magnitude (0..64), FF == end
    unsigned char   nodes[25 * 2];       // ?
};

struct ItNewEnvelopeNodePoint {
    char            magnitude;      // 0..64 for volume, -32 .. +32 for panning or pitch
    unsigned short  tickIndex;      // 0 .. 9999

};

struct ItNewEnvelope {
    unsigned char   flag;           // bit0: env on/off,bit1: loop on/off,bit2:susLoop on/off
    unsigned char   nNodes;         // nr of node points
    unsigned char   loopStart;
    unsigned char   loopEnd;
    unsigned char   sustainLoopStart;
    unsigned char   sustainLoopEnd;
    ItNewEnvelopeNodePoint  nodes[25];
    unsigned char   reserved;
};

struct ItNewInstHeader {
    char            tag[4];         // "IMPI"
    char            fileName[12];
    char            asciiz;
    unsigned char   NNA;            // 0 = Cut,1 = continue, 2 = note off,3 = note fade
    unsigned char   dupCheckType;   // 0 = off,1 = note,2 = Sample,3 = Instr.
    unsigned char   dupCheckAction; // 0 = cut, 1 = note off,2 = note fade
    unsigned short  fadeOut;        // 0..128
    unsigned char   pitchPanSeparation; // -32 .. +32
    unsigned char   pitchPanCenter; // C-0 .. B-9 <=> 0..119
    unsigned char   globalVolume;   // 0..128
    unsigned char   defaultPanning; // 0..64, don't use if bit 7 is set
    unsigned char   randVolumeVariation; // expressed in percent
    unsigned char   randPanningVariation;// not implemented
    unsigned short  trackerVersion;
    unsigned char   nSamples;
    unsigned char   reserved;
    char            name[26];
    unsigned char   initialFilterCutOff;
    unsigned char   initialFilterResonance;
    unsigned char   midiChannel;
    unsigned char   midiProgram;
    unsigned short  midiBank;
    unsigned char   sampleForNote[240];
    ItNewEnvelope   volumeEnvelope;
    ItNewEnvelope   panningEnvelope;
    ItNewEnvelope   pitchEnvelope;
};

struct ItSampleHeader {
    char            tag[4];         // "IMPS"
    char            fileName[12];
    char            asciiz;
    unsigned char   globalVolume;   // 0..64
    unsigned char   flag;
    unsigned char   volume;         // 
    char            name[26];
    unsigned char   convert;        // bit0 set: signed smp data (off = unsigned)
    unsigned char   defaultPanning; // 0..64, bit 7 set == use default panning
    unsigned        length;
    unsigned        loopStart;
    unsigned        loopEnd;
    unsigned        C5Speed;
    unsigned        sustainLoopStart;
    unsigned        sustainLoopEnd;
    unsigned        samplePointer;
    unsigned char   vibratoSpeed;   // 0..64
    unsigned char   vibratoDepth;   // 0..64
    unsigned char   vibratoWaveForm;// 0 = sine,1 = ramp down,2 = square,3 = random
    unsigned char   vibratoRate;    // 0..64
};

struct ItPatternHeader {
    unsigned short  dataSize;       // excluding this header
    unsigned short  nRows;          // 32..200
    unsigned        reserved;
    unsigned char   data[65536 - 8];
};

#pragma pack (8) 

int Module::loadItFile() {
    VirtualFile itFile( fileName_ );
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;
    ItFileHeader itFileHeader;
    itFile.read( &itFileHeader,sizeof( itFileHeader ) );
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;

    // some very basic checking
    if ( //(fileSize < S3M_MIN_FILESIZE) ||
        (! ((itFileHeader.tag[0] == 'I') &&
            (itFileHeader.tag[1] == 'M') &&
            (itFileHeader.tag[2] == 'P') &&
            (itFileHeader.tag[3] == 'M'))
            )
        //|| (itFileHeader.id != 0x1A)
        //|| (itFileHeader.sampleDataType < 1)
        //|| (itFileHeader.sampleDataType > 2)
        ) {
#ifdef debug_it_loader
        std::cout << std::endl
            << "IMPM tag not found or file is too small, exiting.";
#endif
        return 0;
    }
    songTitle_ = "";
    trackerTag_ = "";
    for ( int i = 0; i < 26; i++ ) songTitle_ += itFileHeader.songName[i];
    for( int i = 0; i < 4; i++ ) trackerTag_ += itFileHeader.tag[i];
    trackerType_ = TRACKER_IT;
    useLinearFrequencies_ = (itFileHeader.flags & IT_LINEAR_FREQUENCIES_FLAG) != 0;
    isCustomRepeat_ = false;
    //minPeriod_ = 56;    // periods[9 * 12 - 1]
    //maxPeriod_ = 27392; // periods[0]
    panningStyle_ = PANNING_STYLE_IT;

    // read pattern order list
    songLength_ = itFileHeader.songLength;
    if ( itFileHeader.songLength > MAX_PATTERNS )
    {
        songLength_ = MAX_PATTERNS;
#ifdef debug_it_loader
        std::cout << std::endl 
            << "Reducing song length from " << itFileHeader.songLength 
            << " to " << MAX_PATTERNS << "!" << std::endl;
#endif
    }
    for ( unsigned i = 0; i < songLength_; i++ )
    {
        unsigned char patternNr;
        itFile.read( &patternNr,sizeof( unsigned char ) );
        patternTable_[i] = patternNr;
    }
    itFile.absSeek( sizeof( ItFileHeader ) + itFileHeader.songLength );
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;

    // read the pointers to the instrument, sample and pattern data
    unsigned instHdrPtrs[IT_MAX_INSTRUMENTS];
    unsigned smpHdrPtrs[IT_MAX_SAMPLES];
    unsigned ptnHdrPtrs[IT_MAX_PATTERNS];
    for ( unsigned i = 0; i < itFileHeader.nInstruments; i++ )
    {
        unsigned instPtr;
        itFile.read( &instPtr,sizeof( unsigned ) );
        instHdrPtrs[i] = instPtr;
    }
    for ( unsigned i = 0; i < itFileHeader.nSamples; i++ )
    {
        unsigned smpPtr;
        itFile.read( &smpPtr,sizeof( unsigned ) );
        smpHdrPtrs[i] = smpPtr;
    }
    for ( unsigned i = 0; i < itFileHeader.nPatterns; i++ )
    {
        unsigned ptnPtr;
        itFile.read( &ptnPtr,sizeof( unsigned ) );
        ptnHdrPtrs[i] = ptnPtr;
    }
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;
#ifdef debug_it_loader
    std::cout << std::endl << "IT Header: "
        << std::endl << "Tag                         : " << itFileHeader.tag[0]
        << itFileHeader.tag[1] << itFileHeader.tag[2] << itFileHeader.tag[3]
        << std::endl << "Song Name                   : " << itFileHeader.songName
        << std::endl << "Philight                    : " << itFileHeader.phiLight
        << std::endl << "Song length                 : " << itFileHeader.songLength
        << std::endl << "Nr of instruments           : " << itFileHeader.nInstruments
        << std::endl << "Nr of samples               : " << itFileHeader.nSamples
        << std::endl << "Nr of patterns              : " << itFileHeader.nPatterns << std::hex
        << std::endl << "Created w/ tracker version  : 0x" << itFileHeader.createdWTV
        << std::endl << "Compat. w/ tracker version  : 0x" << itFileHeader.compatibleWTV
        << std::endl << "Flags                       : 0x" << itFileHeader.flags
        << std::endl << "Special                     : " << itFileHeader.special << std::dec
        << std::endl << "Global volume               : " << (unsigned)itFileHeader.globalVolume
        << std::endl << "Mixing amplitude            : " << (unsigned)itFileHeader.mixingAmplitude
        << std::endl << "Initial speed               : " << (unsigned)itFileHeader.initialSpeed
        << std::endl << "Initial bpm                 : " << (unsigned)itFileHeader.initialBpm
        << std::endl << "Panning separation (0..128) : " << (unsigned)itFileHeader.panningSeparation
        << std::endl << "Pitch wheel depth           : " << (unsigned)itFileHeader.pitchWheelDepth
        << std::endl << "Length of song message      : " << itFileHeader.messageLength
        << std::endl << "Offset of song message      : " << itFileHeader.messageOffset
        << std::endl
        << std::endl << "Default volume for each channel: "
        << std::endl;
    for ( int i = 0; i < IT_MAX_CHANNELS; i++ )
        std::cout << std::setw( 3 ) << (unsigned)itFileHeader.defaultVolume[i] << "|";
    std::cout << std::endl << std::endl
        << "Default panning for each channel: " << std::endl;
    for ( int i = 0; i < IT_MAX_CHANNELS; i++ )
        std::cout << std::setw( 3 ) << (unsigned)itFileHeader.defaultPanning[i] << "|";
    std::cout << std::endl << std::endl << "Order list: " << std::endl;
    for ( int i = 0; i < itFileHeader.songLength; i++ )
        std::cout << std::setw( 4 ) << (unsigned)patternTable_[i];
    std::cout << std::endl << std::endl << std::hex << "Instrument header pointers: " << std::endl;
    for ( int i = 0; i < itFileHeader.nInstruments; i++ ) 
        std::cout << std::setw( 8 ) << instHdrPtrs[i];
    std::cout << std::endl << std::endl << "Sample header pointers: " << std::endl;
    for ( int i = 0; i < itFileHeader.nSamples; i++ ) 
        std::cout << std::setw( 8 ) << smpHdrPtrs[i];
    std::cout << std::endl << std::endl << "Pattern header pointers: " << std::endl;
    for ( int i = 0; i < itFileHeader.nPatterns; i++ ) 
        std::cout << std::setw( 8 ) << ptnHdrPtrs[i];
#endif

    // load instruments
    for ( int nInst = 1; nInst <= itFileHeader.nInstruments; nInst++ )
    {
        InstrumentHeader instrumentHeader;
        itFile.absSeek( instHdrPtrs[nInst - 1] );
        if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;

        if ( itFileHeader.createdWTV < 0x200 ) // old IT instrument header
        {
            ItOldInstHeader itInstHeader;
            itFile.read( &itInstHeader,sizeof( ItOldInstHeader ) );
            if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) 
            { 
#ifdef debug_it_loader
                std::cout << std::endl
                    << "Missing data while loading instrument headers, exiting."
                << std::endl;
#endif
                return 0;
            }
#ifdef debug_it_loader
            std::cout << std::endl << "Instrument nr " << nInst << " Header: "
                << std::endl << "Instrument headertag: " << itInstHeader.tag[0]
                << itInstHeader.tag[1] << itInstHeader.tag[2] << itInstHeader.tag[3]
                << std::endl << "Instrument filename : " << itInstHeader.fileName << std::hex
                << std::endl << "flag                : " << (unsigned)itInstHeader.flag << std::dec
                << std::endl << "Volume loop start   : " << (unsigned)itInstHeader.volumeLoopStart
                << std::endl << "Volume loop end     : " << (unsigned)itInstHeader.volumeLoopEnd
                << std::endl << "Sustain loop start  : " << (unsigned)itInstHeader.sustainLoopStart
                << std::endl << "Sustain loop end    : " << (unsigned)itInstHeader.sustainLoopEnd
                << std::endl << "Fade out            : " << itInstHeader.fadeOut << std::hex
                << std::endl << "New Note Action     : " << (unsigned)itInstHeader.NNA
                << std::endl << "Dupl. Note Action   : " << (unsigned)itInstHeader.DNC
                << std::endl << "Tracker version     : " << itInstHeader.trackerVersion
                << std::endl << "Nr of samples       : " << (unsigned)itInstHeader.nSamples
                << std::endl << "Instrument name     : " << itInstHeader.name
                << std::endl << std::endl
                << "Note -> Sample table: " << std::endl << std::dec;
            for ( int i = 0; i < 240; i++ )
                std::cout << std::setw( 4 ) << (unsigned)itInstHeader.sampleForNote[i];
            std::cout << std::endl << std::endl
                << "Volume envelope points: " << std::endl;
            for ( int i = 0; i < 200; i++ )
                std::cout << std::setw( 4 ) << (unsigned)itInstHeader.volumeEnvelope[i];
            std::cout << std::endl << std::endl
                << "Envelope node points (?): " << std::endl;
            for ( int i = 0; i < 25; i++ )
                std::cout
                << std::setw( 2 ) << (unsigned)itInstHeader.nodes[i * 2    ] << ","
                << std::setw( 2 ) << (unsigned)itInstHeader.nodes[i * 2 + 1] << " ";
            std::cout << std::endl << std::endl;
#endif

            /*
            
            char            tag[4];         // "IMPI"
            char            fileName[12];
            char            asciiz;
            unsigned char   flag;
            unsigned char   volumeLoopStart;// these are node numbers
            unsigned char   volumeLoopEnd;
            unsigned char   sustainLoopStart;
            unsigned char   sustainLoopEnd;
            unsigned short  reserved;
            unsigned short  fadeOut;        // 0..64
            unsigned char   NNA;            // New Note Action
            unsigned char   DNC;            // duplicate Note Check
            unsigned short  trackerVersion;
            unsigned char   nSamples;       // nr of samples in instrument
            unsigned char   reserved2;
            char            name[26];
            unsigned char   reserved3[6];
            unsigned char   sampleForNote[240];
            unsigned char   volumeEnvelope[200]; // format: tick,magnitude (0..64), FF == end
            unsigned char   nodes[25 * 2];       // ?
            */
            instrumentHeader.name = itInstHeader.name;
            instrumentHeader.nSamples = 0;       // can probably be removed
            instrumentHeader.volumeLoopStart = itInstHeader.volumeLoopStart;
            instrumentHeader.volumeLoopEnd = itInstHeader.volumeLoopEnd;

            //instrumentHeader.sampleForNote =
            /*
            instrumentHeader.nVolumePoints = 
            instrumentHeader.volumeEnvelope =
            instrumentHeader.volumeFadeOut = 
            instrumentHeader.volumeLoopStart = 
            instrumentHeader.volumeLoopEnd = 
            instrumentHeader.volumeSustain = 
            instrumentHeader.volumeType = 
            instrumentHeader.nPanningPoints =
            instrumentHeader.panningEnvelope = 
            instrumentHeader.panningLoopStart = 
            instrumentHeader.panningLoopEnd = 
            instrumentHeader.panningSustain = 
            instrumentHeader.panningType = 
            instrumentHeader.vibratoDepth = 
            instrumentHeader.vibratoRate = 
            instrumentHeader.vibratoSweep = 
            instrumentHeader.vibratoType = 
            */


        } else {
            ItNewInstHeader itInstHeader;
            itFile.read( &itInstHeader,sizeof( ItNewInstHeader ) );
            if ( itFile.getIOError() != VIRTFILE_NO_ERROR )
            {
#ifdef debug_it_loader
                std::cout << std::endl
                    << "Missing data while loading instrument headers, exiting."
                    << std::endl;
#endif
                return 0;
            }
#ifdef debug_it_loader
            std::cout << std::endl << "Instrument nr " << nInst << " Header: "
                << std::endl << "Instrument headertag: " << itInstHeader.tag[0]
                << itInstHeader.tag[1] << itInstHeader.tag[2] << itInstHeader.tag[3]
                << std::endl << "Instrument filename : " << itInstHeader.fileName << std::hex
                << std::endl << "New Note Action     : " << (unsigned)itInstHeader.NNA
                << std::endl << "Dup check type      : " << (unsigned)itInstHeader.dupCheckType
                << std::endl << "Dup check Action    : " << (unsigned)itInstHeader.dupCheckAction << std::dec
                << std::endl << "Fade out (0..128)   : " << (unsigned)itInstHeader.fadeOut        // 0..128
                << std::endl << "Pitch Pan separation: " << (unsigned)itInstHeader.pitchPanSeparation // -32 .. +32
                << std::endl << "Pitch pan center    : " << (unsigned)itInstHeader.pitchPanCenter // C-0 .. B-9 <=> 0..119
                << std::endl << "Global volume (128) : " << (unsigned)itInstHeader.globalVolume   // 0..128
                << std::endl << "Default panning     : " << (unsigned)itInstHeader.defaultPanning // 0..64, don't use if bit 7 is set
                << std::endl << "Random vol variation: " << (unsigned)itInstHeader.randVolumeVariation // expressed in percent
                << std::endl << "Random pan variation: " << (unsigned)itInstHeader.randPanningVariation << std::hex
                << std::endl << "Tracker version     : " << itInstHeader.trackerVersion << std::dec
                << std::endl << "Nr of samples       : " << (unsigned)itInstHeader.nSamples
                << std::endl << "Instrument name     : " << itInstHeader.name
                << std::endl << "Init. filter cut off: " << (unsigned)itInstHeader.initialFilterCutOff
                << std::endl << "Init. filter reson. : " << (unsigned)itInstHeader.initialFilterResonance
                << std::endl << "Midi channel        : " << (unsigned)itInstHeader.midiChannel
                << std::endl << "Midi program        : " << (unsigned)itInstHeader.midiProgram
                << std::endl << "Midi bank           : " << (unsigned)itInstHeader.midiBank
            << std::endl << std::endl
                << "Note -> Sample table: " << std::endl << std::dec;
            for ( int i = 0; i < 240; i++ )
                std::cout << std::setw( 4 ) << (unsigned)itInstHeader.sampleForNote[i];
            std::cout << std::endl << std::endl
                << "Volume envelope: "
                << std::endl << "Flag                : " << (unsigned)itInstHeader.volumeEnvelope.flag
                << std::endl << "Nr of nodes         : " << (unsigned)itInstHeader.volumeEnvelope.nNodes
                << std::endl << "Loop start          : " << (unsigned)itInstHeader.volumeEnvelope.loopStart
                << std::endl << "Loop end            : " << (unsigned)itInstHeader.volumeEnvelope.loopEnd
                << std::endl << "Sustain loop start  : " << (unsigned)itInstHeader.volumeEnvelope.sustainLoopStart
                << std::endl << "Sustain loop end    : " << (unsigned)itInstHeader.volumeEnvelope.sustainLoopEnd
                << std::endl << "Envelope node points: " << std::endl << std::dec;
            for ( int i = 0; i < 25; i++ )
                std::cout
                << std::setw( 2 ) << (unsigned)itInstHeader.volumeEnvelope.nodes[i].magnitude << ":"
                << std::setw( 4 ) << (unsigned)itInstHeader.volumeEnvelope.nodes[i].tickIndex << " ";
            std::cout << std::endl << std::endl
                << "Panning envelope: "
                << std::endl << "Flag                : " << (unsigned)itInstHeader.panningEnvelope.flag
                << std::endl << "Nr of nodes         : " << (unsigned)itInstHeader.panningEnvelope.nNodes
                << std::endl << "Loop start          : " << (unsigned)itInstHeader.panningEnvelope.loopStart
                << std::endl << "Loop end            : " << (unsigned)itInstHeader.panningEnvelope.loopEnd
                << std::endl << "Sustain loop start  : " << (unsigned)itInstHeader.panningEnvelope.sustainLoopStart
                << std::endl << "Sustain loop end    : " << (unsigned)itInstHeader.panningEnvelope.sustainLoopEnd
                << std::endl << "Envelope node points: " << std::endl << std::dec;
            for ( int i = 0; i < 25; i++ )
                std::cout
                << std::setw( 2 ) << (unsigned)itInstHeader.panningEnvelope.nodes[i].magnitude << ":"
                << std::setw( 4 ) << (unsigned)itInstHeader.panningEnvelope.nodes[i].tickIndex << " ";
            std::cout << std::endl << std::endl
                << "Pitch envelope: "
                << std::endl << "Flag                : " << (unsigned)itInstHeader.pitchEnvelope.flag
                << std::endl << "Nr of nodes         : " << (unsigned)itInstHeader.pitchEnvelope.nNodes
                << std::endl << "Loop start          : " << (unsigned)itInstHeader.pitchEnvelope.loopStart
                << std::endl << "Loop end            : " << (unsigned)itInstHeader.pitchEnvelope.loopEnd
                << std::endl << "Sustain loop start  : " << (unsigned)itInstHeader.pitchEnvelope.sustainLoopStart
                << std::endl << "Sustain loop end    : " << (unsigned)itInstHeader.pitchEnvelope.sustainLoopEnd
                << std::endl << "Envelope node points: " << std::endl << std::dec;
            for ( int i = 0; i < 25; i++ )
                std::cout
                << std::setw( 2 ) << (unsigned)itInstHeader.pitchEnvelope.nodes[i].magnitude << ":"
                << std::setw( 4 ) << (unsigned)itInstHeader.pitchEnvelope.nodes[i].tickIndex << " ";
            std::cout << std::endl << std::endl;
#endif
        }
    }
    isLoaded_ = false;
    return 0;
}
