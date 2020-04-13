/*
Thanks must go to:
- Jeffrey Lim (Pulse) for creating the awesome Impulse Tracker
- Johannes - Jojo - Schultz (Saga Musix) for helping me out with all kinds of
  questions related to the .IT format.
- Tammo Hinrichs for the .IT sample decompression routines itsex.c which I
  used in my .IT loader. Thanks for explaining how the algorithm works! See 
  itsex.h for details.



Notes from OpenMPT forums:

Impulse Tracker will have issues with files exceeding its originally specified
ranges, however most other players have no problems with the following:
- 256 rows per pattern or more (OpenMPT and I think XMPlay cap this at 1024)
- 240 distinct patterns (could in theory be 254, but is less for historical 
  reasons, I guess)
- 200 samples (could again be 255, but this is probably another historical 
  limit)

*/

#include <climits>
#if CHAR_BIT != 8 
This code requires a byte to be 8 bits wide
#endif

#define NOMINMAX
#include <windows.h>
#include <conio.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <memory>
#include <algorithm>

#include "Module.h"
#include "virtualfile.h"
#include "itsex.h"

#define debug_it_show_instruments
//#define debug_it_show_patterns
//#define debug_it_play_samples

#define IT_DEBUG_SHOW_MAX_CHN 13

#include <bitset>
#include <iomanip>

const int IT_MAX_CHANNELS                       = 64;
const int IT_MAX_PATTERNS                       = 200;
const int IT_MAX_SONG_LENGTH                    = MAX_PATTERNS;
const int IT_MIN_PATTERN_ROWS                   = 32;
const int IT_MAX_PATTERN_ROWS                   = 200;
const int IT_MAX_SAMPLES                        = 99;
const int IT_MAX_INSTRUMENTS                    = 100; // ?
const int IT_MAX_NOTE                           = 119; // 9 octaves: (C-0 -> B-9)

const int IT_STEREO_FLAG                        = 1;
const int IT_VOL0_OPT_FLAG                      = 2;
const int IT_INSTRUMENT_MODE                    = 4;
const int IT_LINEAR_FREQUENCIES_FLAG            = 8;
const int IT_OLD_EFFECTS_MODE                   = 16;
const int IT_GEF_LINKED_EFFECT_MEMORY           = 32;
const int IT_USE_MIDI_PITCH_CONTROLLER          = 64;
const int IT_REQUEST_MIDI_CONFIG                = 128;

const int IT_SONG_MESSAGE_FLAG                  = 1;
const int IT_MIDI_CONFIG_EMBEDDED               = 8;

const int IT_DOS_FILENAME_LENGTH                = 12;
const int IT_SONG_NAME_LENGTH                   = 26;
const int IT_INST_NAME_LENGTH                   = 26;
const int IT_MAX_ENVELOPE_NODES                 = 25;
const int IT_ENVELOPE_END_MARKER                = 0xFF;
const int IT_ENVELOPE_ENABLED_FLAG              = 1;
const int IT_ENVELOPE_IS_LOOPED_FLAG            = 2;
const int IT_ENVELOPE_IS_SUSTAINED_FLAG         = 4;
const int IT_ENVELOPE_CARRY_ENABLED_FLAG        = 8;
const int IT_ENVELOPE_IS_FOR_FILTER_FLAG        = 128;

// sample flags
const int IT_SMP_NAME_LENGTH                    = 26;
const int IT_SMP_ASSOCIATED_WITH_HEADER         = 1;
const int IT_SMP_IS_16_BIT                      = 2;
const int IT_SMP_IS_STEREO                      = 4; // not supported by Impulse Trckr
const int IT_SMP_IS_COMPRESSED                  = 8;
const int IT_SMP_LOOP_ON                        = 16;
const int IT_SMP_SUSTAIN_LOOP_ON                = 32;
const int IT_SMP_PINGPONG_LOOP_ON               = 64;
const int IT_SMP_PINGPONG_SUSTAIN_LOOP_ON       = 128;
const int IT_SMP_USE_DEFAULT_PANNING            = 128;
const int IT_SMP_MAX_GLOBAL_VOLUME              = 64;
const int IT_SMP_MAX_VOLUME                     = 64;
const int IT_SIGNED_SAMPLE_DATA                 = 1;

// pattern flags
const int IT_END_OF_SONG_MARKER                 = 255;
const int IT_MARKER_PATTERN                     = 254;
const int IT_PATTERN_CHANNEL_MASK_AVAILABLE     = 128;
const int IT_PATTERN_END_OF_ROW_MARKER          = 0;
const int IT_PATTERN_NOTE_PRESENT               = 1;
const int IT_PATTERN_INSTRUMENT_PRESENT         = 2;
const int IT_PATTERN_VOLUME_COLUMN_PRESENT      = 4;
const int IT_PATTERN_COMMAND_PRESENT            = 8;
const int IT_PATTERN_LAST_NOTE_IN_CHANNEL       = 16;
const int IT_PATTERN_LAST_INST_IN_CHANNEL       = 32;
const int IT_PATTERN_LAST_VOLC_IN_CHANNEL       = 64;
const int IT_PATTERN_LAST_COMMAND_IN_CHANNEL    = 128;
const int IT_NOTE_CUT                           = 254;
const int IT_KEY_OFF                            = 255;

// constants for decoding the volume column
const int IT_VOLUME_COLUMN_UNDEFINED              = 213;
const int IT_VOLUME_COLUMN_VIBRATO                = 203;
const int IT_VOLUME_COLUMN_TONE_PORTAMENTO        = 193;
const int IT_VOLUME_COLUMN_SET_PANNING            = 128;
const int IT_VOLUME_COLUMN_PORTAMENTO_UP          = 114;
const int IT_VOLUME_COLUMN_PORTAMENTO_DOWN        = 104;
const int IT_VOLUME_COLUMN_VOLUME_SLIDE_DOWN      = 94;
const int IT_VOLUME_COLUMN_VOLUME_SLIDE_UP        = 84;
const int IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_DOWN = 74;
const int IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_UP   = 64;
const int IT_VOLUME_COLUMN_SET_VOLUME             = 0;

#pragma pack (1) 
struct ItFileHeader {
    char            tag[4];         // "IMPM"
    char            songName[IT_SONG_NAME_LENGTH];
    std::uint16_t   phiLight;       // pattern row hilight info (ignore)
    std::uint16_t   songLength;
    std::uint16_t   nrInstruments;
    std::uint16_t   nrSamples;
    std::uint16_t   nrPatterns;
    std::uint16_t   createdWTV;
    std::uint16_t   compatibleWTV;
    std::uint16_t   flags;
    std::uint16_t   special;
    std::uint8_t    globalVolume;   // 0..128
    std::uint8_t    mixingAmplitude;// 0..128
    std::uint8_t    initialSpeed;
    std::uint8_t    initialBpm;     // tempo
    std::uint8_t    panningSeparation;// 0..128, 128 == max separation
    std::uint8_t    pitchWheelDepth;// for MIDI controllers
    std::uint16_t   messageLength;
    std::uint32_t   messageOffset;
    std::uint32_t   reserved;
    std::uint8_t    defaultPanning[IT_MAX_CHANNELS];// 0..64, 100 = surround, +128 == disabled channel
    std::uint8_t    defaultVolume[IT_MAX_CHANNELS]; // 0..64
};

/*
    follows:
    - orderlist, songLength bytes long. Max ptn nr == 199. 255 = end of song, 254 = marker ptn
    - instrument Offset list (nr of instruments): 32 bit
    - sample Header Offset list (nr of samples) : 32 bit
    - pattern Offset list (nr of patterns)      : 32 bit
*/

/*
    --> std::uint8_t    sampleForNote[240]:
    Note-Sample/Keyboard Table.
    Each note of the instrument is first converted to a sample number
    and a note (C-0 -> B-9). These are stored as note/sample pairs
    (note first, range 0->119 for C-0 to B-9, sample ranges from
    1-99, 0=no sample
*/
struct ItNoteSampleMap {
    std::uint8_t    note;
    std::uint8_t    sampleNr;
};

struct ItOldEnvelopeNodePoint {
    std::uint8_t    tickIndex;      // 0 .. 255
    std::uint8_t    magnitude;      // 0..64 
};

struct ItOldInstHeader {
    char            tag[4];         // "IMPI"
    char            fileName[IT_DOS_FILENAME_LENGTH];
    char            asciiz;
    std::uint8_t    flag;
    std::uint8_t    volumeLoopStart;// these are node numbers
    std::uint8_t    volumeLoopEnd;
    std::uint8_t    sustainLoopStart;
    std::uint8_t    sustainLoopEnd;
    std::uint16_t   reserved;
    std::uint16_t   fadeOut;        // 0..64
    std::uint8_t    NNA;            // New Note Action
    std::uint8_t    DNC;            // duplicate Note Check
    std::uint16_t   trackerVersion;
    std::uint8_t    nrSamples;       // nr of samples in instrument
    std::uint8_t    reserved2;
    char            name[IT_INST_NAME_LENGTH];
    std::uint8_t    reserved3[6];
    ItNoteSampleMap sampleForNote[120];
    std::uint8_t    volumeEnvelope[200]; // format: tick,magnitude (0..64), FF == end
    ItOldEnvelopeNodePoint nodes[IT_MAX_ENVELOPE_NODES];
};

struct ItNewEnvelopeNodePoint {
    char            magnitude;      // 0..64 for volume, -32 .. +32 for panning or pitch
    std::uint16_t   tickIndex;      // 0 .. 9999
};

struct ItNewEnvelope {
    std::uint8_t    flag;           // bit0: env on/off,bit1: loop on/off,bit2:susLoop on/off
    std::uint8_t    nrNodes;        // nr of node points
    std::uint8_t    loopStart;
    std::uint8_t    loopEnd;
    std::uint8_t    sustainLoopStart;
    std::uint8_t    sustainLoopEnd;
    ItNewEnvelopeNodePoint  nodes[IT_MAX_ENVELOPE_NODES];
    std::uint8_t    reserved;
};

struct ItNewInstHeader {
    char            tag[4];         // "IMPI"
    char            fileName[IT_DOS_FILENAME_LENGTH];
    char            asciiz;
    std::uint8_t    NNA;            // 0 = Cut,1 = continue, 2 = note off,3 = note fade
    std::uint8_t    dupCheckType;   // 0 = off,1 = note,2 = Sample,3 = Instr.
    std::uint8_t    dupCheckAction; // 0 = cut, 1 = note off,2 = note fade
    std::uint16_t   fadeOut;        // 0..128
    std::uint8_t    pitchPanSeparation; // -32 .. +32
    std::uint8_t    pitchPanCenter; // C-0 .. B-9 <=> 0..119
    std::uint8_t    globalVolume;   // 0..128
    std::uint8_t    defaultPanning; // 0..64, don't use if bit 7 is set
    std::uint8_t    randVolumeVariation; // expressed in percent
    std::uint8_t    randPanningVariation;// not implemented
    std::uint16_t   trackerVersion;
    std::uint8_t    nrSamples;
    std::uint8_t    reserved;
    char            name[IT_INST_NAME_LENGTH];
    std::uint8_t    initialFilterCutOff;
    std::uint8_t    initialFilterResonance;
    std::uint8_t    midiChannel;
    std::uint8_t    midiProgram;
    std::uint16_t   midiBank;
    ItNoteSampleMap sampleForNote[120];
    ItNewEnvelope   volumeEnvelope;
    ItNewEnvelope   panningEnvelope;
    ItNewEnvelope   pitchFltrEnvelope;
};

struct ItSampleHeader {
    char            tag[4];         // "IMPS"
    char            fileName[IT_DOS_FILENAME_LENGTH];
    char            asciiz;
    std::uint8_t    globalVolume;   // 0..64
    std::uint8_t    flag;
    std::uint8_t    volume;         // 
    char            name[IT_SMP_NAME_LENGTH];
    std::uint8_t    convert;        // bit0 set: signed smp data (off = unsigned)
    std::uint8_t    defaultPanning; // 0..64, bit 7 set == use default panning
    std::uint32_t   length;
    std::uint32_t   loopStart;
    std::uint32_t   loopEnd;
    std::uint32_t   c5Speed;
    std::uint32_t   sustainLoopStart;
    std::uint32_t   sustainLoopEnd;
    std::uint32_t   samplePointer;
    std::uint8_t    vibratoSpeed;   // 0..64
    std::uint8_t    vibratoDepth;   // 0..64
    std::uint8_t    vibratoWaveForm;// 0 = sine,1 = ramp down,2 = square,3 = random
    std::uint8_t    vibratoRate;    // 0..64
};

struct ItPatternHeader {
    std::uint16_t   dataSize;       // excluding this header
    std::uint16_t   nRows;          // 32..200
    std::uint32_t   reserved;
    //std::uint8_t    data[65536 - 8];
};

#pragma pack (8) 

// small class with functions that show debug information
class ItDebugShow {
public:
    static void fileHeader( ItFileHeader& itFileHeader );
    static void oldInstHeader( ItOldInstHeader& itOldInstHeader );
    static void newInstHeader( ItNewInstHeader& itNewInstHeader );
    static void sampleHeader( ItSampleHeader& itSampleHeader );
    static void pattern( Pattern& Pattern );
};

int getItActiveChannels( VirtualFile& itFile,bool( &channelIsUsed )[IT_MAX_CHANNELS] ); // fwd

int Module::loadItFile( VirtualFile& moduleFile )
{
    moduleFile.absSeek( 0 );
    VirtualFile& itFile = moduleFile;

    ItFileHeader itFileHeader;
    itFile.read( &itFileHeader,sizeof( itFileHeader ) );
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;

    songTitle_.assign( itFileHeader.songName,IT_SONG_NAME_LENGTH );
    trackerTag_.assign( itFileHeader.tag,4 );

    // some very basic checking
    if ( (trackerTag_ != "IMPM")
        //|| (itFileHeader.id != 0x1A)
        //|| (itFileHeader.sampleDataType < 1)
        //|| (itFileHeader.sampleDataType > 2)
        ) {
        if( showDebugInfo_ )
            std::cout << "\nIMPM tag not found or file is too small, exiting.";
        return 0;
    }

    trackerType_ = TRACKER_IT;
    useLinearFrequencies_ = (itFileHeader.flags & IT_LINEAR_FREQUENCIES_FLAG) != 0;
    isCustomRepeat_ = false;
    //minPeriod_ = 56;    // periods[9 * 12 - 1]
    //maxPeriod_ = 27392; // periods[0]
    panningStyle_ = PANNING_STYLE_IT;
    bool isIt215Compression = itFileHeader.compatibleWTV >= 0x215;

    // read pattern order list
    songLength_ = itFileHeader.songLength;
    songRestartPosition_ = 0;
    if ( itFileHeader.songLength > MAX_PATTERNS ) {
        songLength_ = MAX_PATTERNS;
        if ( showDebugInfo_ )
            std::cout
                << "\nReducing song length from " << itFileHeader.songLength
                << " to " << MAX_PATTERNS << "!\n";
    }
    for ( unsigned i = 0; i < songLength_; i++ ) {
        unsigned char patternNr;
        itFile.read( &patternNr,sizeof( unsigned char ) );
        patternTable_[i] = patternNr;
    }
    itFile.absSeek( sizeof( ItFileHeader ) + itFileHeader.songLength );
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;

    // read the file offset pointers to the instrument, sample and pattern data
    unsigned instHdrPtrs[IT_MAX_INSTRUMENTS];
    unsigned smpHdrPtrs[IT_MAX_SAMPLES];
    unsigned ptnHdrPtrs[IT_MAX_PATTERNS];
    for ( unsigned i = 0; i < itFileHeader.nrInstruments; i++ )
        itFile.read( &(instHdrPtrs[i]),sizeof( std::uint32_t ) );
    for ( unsigned i = 0; i < itFileHeader.nrSamples; i++ )
        itFile.read( &(smpHdrPtrs[i]),sizeof( std::uint32_t ) );
    for ( unsigned i = 0; i < itFileHeader.nrPatterns; i++ )
        itFile.read( &(ptnHdrPtrs[i]),sizeof( std::uint32_t ) );

    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;
    if ( showDebugInfo_ ) {
        ItDebugShow::fileHeader( itFileHeader );
        std::cout << "\n\nOrder list: \n";
        for ( int i = 0; i < itFileHeader.songLength; i++ )
            std::cout << std::setw( 4 ) << (unsigned)patternTable_[i];
        std::cout << std::hex << "\n\nInstrument header pointers: \n";
        for ( int i = 0; i < itFileHeader.nrInstruments; i++ )
            std::cout << std::setw( 8 ) << instHdrPtrs[i];
        std::cout << "\n\nSample header pointers: \n";
        for ( int i = 0; i < itFileHeader.nrSamples; i++ )
            std::cout << std::setw( 8 ) << smpHdrPtrs[i];
        std::cout << "\n\nPattern header pointers: \n";
        for ( int i = 0; i < itFileHeader.nrPatterns; i++ )
            std::cout << std::setw( 8 ) << ptnHdrPtrs[i];
    }
    defaultTempo_ = itFileHeader.initialSpeed;
    defaultBpm_ = itFileHeader.initialBpm;

    // load instruments
    nrInstruments_ = itFileHeader.nrInstruments;
    if ( itFileHeader.flags & IT_INSTRUMENT_MODE ) {
        for ( unsigned instNr = 1; instNr <= nrInstruments_; instNr++ ) {
            itFile.absSeek( instHdrPtrs[instNr - 1] );
            if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) 
                return 0;
            int result = loadItInstrument( itFile,instNr,itFileHeader.createdWTV );
            if ( result ) 
                return 0;
        }
    }
    // load samples
    nrSamples_ = itFileHeader.nrSamples;
    bool convertToInstrument = !(itFileHeader.flags & IT_INSTRUMENT_MODE);
    if ( convertToInstrument )
        nrInstruments_ = nrSamples_;
    for ( unsigned sampleNr = 1; sampleNr <= nrSamples_; sampleNr++ ) {
        itFile.absSeek( smpHdrPtrs[sampleNr - 1] );
        int result = loadItSample(
            itFile,
            sampleNr,
            convertToInstrument,
            isIt215Compression
        );
        if ( result ) 
            return 0;
    }
    // Check nr of channels here by analysing the patterns:
    bool patternIsUsed[IT_MAX_PATTERNS];
    bool channelIsUsed[IT_MAX_CHANNELS];
    memset( patternIsUsed,false,sizeof( bool ) * IT_MAX_PATTERNS );
    memset( channelIsUsed,false,sizeof( bool ) * IT_MAX_CHANNELS );
    for ( int i = 0; i < itFileHeader.songLength;i++ ) { 
        unsigned usedPattern = patternTable_[i];
        if( usedPattern < IT_MAX_PATTERNS )
            patternIsUsed[usedPattern] = true;
    }
    for ( int patternNr = 0; patternNr < itFileHeader.nrPatterns; patternNr++ ) {
        if ( !patternIsUsed[patternNr] ) 
            continue;
        unsigned offset = ptnHdrPtrs[patternNr];
        if ( offset ) {
            itFile.absSeek( offset );
            int result = getItActiveChannels( itFile,channelIsUsed );
            if ( result )
                return 0;
            if ( showDebugInfo_ ) {
                std::cout
                    << "\n" << std::setw( 3 ) << patternNr << ":";
                for ( int i = 0; i < IT_MAX_CHANNELS;i++ ) {
                    std::cout << (channelIsUsed[i] ? "Y" : "N");
                }
            }
        }
    }
    /*
        We could of course remap unused channels .S3M style but this is not the
        way the Impulse tracker or the .IT format works so we simple keep the
        highest channels as the nr of channels (+ 1 because 1st channel is 0).
    */
    nrChannels_ = 0;
    for ( int i = IT_MAX_CHANNELS - 1; i >= 0; i-- )
        if ( channelIsUsed[i] ) {
            nrChannels_ = i + 1;
            break;
        }
    
    if ( showDebugInfo_ )
        std::cout
            << "\nDetected " << nrChannels_ << " nr of channels.";

    // load patterns
    for ( int patternNr = 0; patternNr < itFileHeader.nrPatterns; patternNr++ ) {
        if ( !patternIsUsed[patternNr] )
            continue;
        unsigned offset = ptnHdrPtrs[patternNr];
        if ( offset ) {
            itFile.absSeek( offset );
            int result = loadItPattern( itFile,patternNr );
            if ( result ) 
                return 0;
        }
    }
    isLoaded_ = true;
    return 0;
}

// file pointer must be at the correct offset
// returns non-zero on error
// instrument numbers are 1-based
int Module::loadItInstrument(
    VirtualFile& itFile,
    int instrumentNr,
    unsigned createdWTV ) 
{
    InstrumentHeader instrumentHeader;
    if ( createdWTV < 0x200 ) { 

        // *******************************************
        // Too bad, it is an old .IT instrument header
        // *******************************************

        ItOldInstHeader itInstHeader;
        itFile.read( &itInstHeader,sizeof( ItOldInstHeader ) );
        if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) {
            if ( showDebugInfo_ )
                std::cout 
                    << "\nMissing data while loading instrument headers, "
                    << "exiting.\n";
            return -1;
        }
        if ( showDebugInfo_ ) {
#ifdef debug_it_show_instruments
            std::cout << "\nInstrument nr " << instrumentNr << " Header: ";
            ItDebugShow::oldInstHeader( itInstHeader );
#endif
        }
        instrumentHeader.name.assign( itInstHeader.name,IT_INST_NAME_LENGTH );
        for ( int n = 0; n < IT_MAX_NOTE; n++ ) {
            instrumentHeader.sampleForNote[n].note =
                itInstHeader.sampleForNote[n].note;
            instrumentHeader.sampleForNote[n].sampleNr =
                itInstHeader.sampleForNote[n].sampleNr;
        }
        // take care of the volume envelope:
        instrumentHeader.volumeEnvelope.loopStart       = itInstHeader.volumeLoopStart;
        instrumentHeader.volumeEnvelope.loopEnd         = itInstHeader.volumeLoopEnd;
        instrumentHeader.volumeEnvelope.sustainStart    = itInstHeader.sustainLoopStart;
        instrumentHeader.volumeEnvelope.sustainEnd      = itInstHeader.sustainLoopEnd;

        // copy envelope node points:
        unsigned char& nrNodes = instrumentHeader.volumeEnvelope.nrNodes = 0;
        for ( int i = 0; i < IT_MAX_ENVELOPE_NODES; i++ ) {
            instrumentHeader.volumeEnvelope.nodes[i].x =
                itInstHeader.nodes[i].tickIndex;
            instrumentHeader.volumeEnvelope.nodes[i].y =
                itInstHeader.nodes[i].magnitude;
            if ( itInstHeader.nodes[i].tickIndex != 0xFF )
                nrNodes++; // ??? To check!
        }
        nrNodes = std::min( nrNodes,(unsigned char)IT_MAX_ENVELOPE_NODES );

        // take care of flags:
        unsigned char flags = 0;
        if ( itInstHeader.flag & IT_ENVELOPE_ENABLED_FLAG )
            flags |= ENVELOPE_IS_ENABLED_FLAG;

        if ( itInstHeader.flag & IT_ENVELOPE_IS_LOOPED_FLAG )
            flags |= ENVELOPE_IS_LOOPED_FLAG;

        if ( itInstHeader.flag & IT_ENVELOPE_IS_SUSTAINED_FLAG )
            flags |= ENVELOPE_IS_SUSTAINED_FLAG;

        instrumentHeader.volumeEnvelope.setFlags( flags );

        // copy the rest of the instrument info
        instrumentHeader.name.assign( itInstHeader.name,IT_INST_NAME_LENGTH );
        instrumentHeader.nnaType = itInstHeader.NNA;
        instrumentHeader.dctType = itInstHeader.DNC;
        instrumentHeader.dcaType = DCA_CUT;

        // I believe the FadeOut starts at 512 in the old instrument format:
        instrumentHeader.volumeFadeOut = itInstHeader.fadeOut * 128; // range: 0 .. 128 

        // not available in the old .IT format:
        instrumentHeader.initialFilterCutoff = 0;
        instrumentHeader.initialFilterResonance = 0;
        instrumentHeader.pitchPanSeparation = 0; // ?
        instrumentHeader.pitchPanCenter = 0; // ?
        instrumentHeader.globalVolume = 128;
        instrumentHeader.defaultPanning = 0; // ?
        instrumentHeader.randVolumeVariation = 0;
        instrumentHeader.randPanningVariation = 0;
    }
    else {

        // **********************************
        // It is a new instrument header yay!
        // **********************************

        ItNewInstHeader itInstHeader;
        itFile.read( &itInstHeader,sizeof( ItNewInstHeader ) );
        if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) {
            if ( showDebugInfo_ )
                std::cout
                    << "\nMissing data while loading instrument headers, "
                    << "exiting.\n";            
            return -1;
        }
        if ( showDebugInfo_ ) {
#ifdef debug_it_show_instruments
            std::cout << "\nInstrument nr " << instrumentNr << " Header: ";
            ItDebugShow::newInstHeader( itInstHeader );
#endif
        }
        // Load the instrument data into our own format.
        // we start with the sample for note table:
        for ( int n = 0; n < IT_MAX_NOTE; n++ ) { 
            instrumentHeader.sampleForNote[n].note =
                itInstHeader.sampleForNote[n].note;
            instrumentHeader.sampleForNote[n].sampleNr =
                itInstHeader.sampleForNote[n].sampleNr;
        }
        // copy all three envelopes in one loop. First we declare some pointers
        // to the envelopes so we can access them in the loop:
        ItNewEnvelope* srcEnvs[] = {
            &(itInstHeader.volumeEnvelope),
            &(itInstHeader.panningEnvelope),
            &(itInstHeader.pitchFltrEnvelope)
        };
        Envelope* destEnvs[] = {
            &(instrumentHeader.volumeEnvelope),
            &(instrumentHeader.panningEnvelope),
            &(instrumentHeader.pitchFltrEnvelope)
        };

        const int nrEnvelopes = 3;
        for ( int envNr = 0; envNr < nrEnvelopes; envNr++ ) {

            // create some shorthand references:
            Envelope& destEnv = *(destEnvs[envNr]);
            ItNewEnvelope& srcEnv = *(srcEnvs[envNr]);

            // copy envelope node points:
            destEnv.nrNodes = std::min( (int)(srcEnv.nrNodes),IT_MAX_ENVELOPE_NODES );
            for ( int i = 0; i < destEnv.nrNodes; i++ ) {
                destEnv.nodes[i].x = srcEnv.nodes[i].tickIndex;
                if (envNr != 1 )
                    destEnv.nodes[i].y = srcEnv.nodes[i].magnitude;
                else
                    // panning envelope in .IT has range of -32 .. +32
                    destEnv.nodes[i].y = 
                    (signed char)(srcEnv.nodes[i].magnitude) + 32;
            }
            // copy other envelope parameters:
            destEnv.sustainStart = srcEnv.sustainLoopStart;
            destEnv.sustainEnd = srcEnv.sustainLoopEnd;
            destEnv.loopStart = srcEnv.loopStart;
            destEnv.loopEnd = srcEnv.loopEnd;

            // take care of flags:
            unsigned char flags = 0;
            if ( srcEnv.flag & IT_ENVELOPE_ENABLED_FLAG )
                flags |= ENVELOPE_IS_ENABLED_FLAG;

            if ( srcEnv.flag & IT_ENVELOPE_IS_LOOPED_FLAG )
                flags |= ENVELOPE_IS_LOOPED_FLAG;

            if ( srcEnv.flag & IT_ENVELOPE_IS_SUSTAINED_FLAG )
                flags |= ENVELOPE_IS_SUSTAINED_FLAG;

            if ( (srcEnv.flag & IT_ENVELOPE_IS_FOR_FILTER_FLAG) && (envNr == 3) )
                flags |= ENVELOPE_FOR_FILTER_FLAG;

            if ( (srcEnv.flag & IT_ENVELOPE_CARRY_ENABLED_FLAG) && (envNr == 3) )
                flags |= ENVELOPE_CARRY_ENABLED_FLAG;

            destEnv.setFlags( flags );
        }
        // copy the rest of the instrument info
        instrumentHeader.name.assign( itInstHeader.name,IT_INST_NAME_LENGTH );
        instrumentHeader.nnaType                = itInstHeader.NNA;
        instrumentHeader.dctType                = itInstHeader.dupCheckType;
        instrumentHeader.dcaType                = itInstHeader.dupCheckAction;
        instrumentHeader.initialFilterCutoff    = itInstHeader.initialFilterCutOff;
        instrumentHeader.initialFilterResonance = itInstHeader.initialFilterResonance;

        // I believe the FadeOut starts at 1024 in the new instrument format:
        instrumentHeader.volumeFadeOut          = itInstHeader.fadeOut * 64;
        instrumentHeader.pitchPanSeparation     = itInstHeader.pitchPanSeparation;
        instrumentHeader.pitchPanCenter         = itInstHeader.pitchPanCenter;
        instrumentHeader.globalVolume           = itInstHeader.globalVolume;
        instrumentHeader.defaultPanning         = itInstHeader.defaultPanning;
        instrumentHeader.randVolumeVariation    = itInstHeader.randVolumeVariation;
        instrumentHeader.randPanningVariation   = itInstHeader.randPanningVariation;
    }
    // tell the envelope functions we want IT style envelope processing:
    instrumentHeader.volumeEnvelope.setEnvelopeStyle( itEnvelopeStyle );

    // create the instrument
    instruments_[instrumentNr] = std::make_unique <Instrument>( instrumentHeader );
    return 0;
}

// file pointer must be at the correct offset
// returns non-zero on error
// sample numbers are 1-based
int Module::loadItSample(
    VirtualFile& itFile,
    int sampleNr,
    bool convertToInstrument,
    bool isIt215Compression )
{
    ItSampleHeader itSampleHeader;
    if ( itFile.read( &itSampleHeader,sizeof( itSampleHeader ) ) ) 
        return -1;
    bool is16BitSample = (itSampleHeader.flag & IT_SMP_IS_16_BIT) != 0;
    bool isCompressed = (itSampleHeader.flag & IT_SMP_IS_COMPRESSED) != 0;
    bool isStereoSample = (itSampleHeader.flag & IT_SMP_IS_STEREO) != 0;
    if ( showDebugInfo_ ) {
        std::cout << "\n\nSample header nr " << sampleNr << ":";
        ItDebugShow::sampleHeader( itSampleHeader );
    }
    if ( !(itSampleHeader.flag & IT_SMP_ASSOCIATED_WITH_HEADER) )
        return 0; // nothing to load: a bit drastic maybe (song message lost)

    SampleHeader sample;
    //samples_[sampleNr] = new Sample;
    sample.name.append( itSampleHeader.name,IT_SMP_NAME_LENGTH );
    sample.length = itSampleHeader.length;
    sample.repeatOffset = itSampleHeader.loopStart;
    if ( sample.repeatOffset > sample.length ) sample.repeatOffset = 0;
    sample.repeatLength = itSampleHeader.loopEnd - itSampleHeader.loopStart;
    if ( sample.repeatOffset + sample.repeatLength >= sample.length )
        sample.repeatLength = sample.length - sample.repeatOffset - 1;
    sample.sustainRepeatStart = itSampleHeader.sustainLoopStart;

    if ( sample.sustainRepeatStart > sample.length ) // ?
        sample.sustainRepeatStart = 0;
    sample.sustainRepeatEnd = itSampleHeader.sustainLoopEnd;
    if ( sample.sustainRepeatEnd > sample.length ) // ?
        sample.sustainRepeatEnd = sample.length - 1;

    sample.isRepeatSample =
        (itSampleHeader.flag & IT_SMP_LOOP_ON) != 0;
    sample.isPingpongSample =
        (itSampleHeader.flag & IT_SMP_PINGPONG_LOOP_ON) != 0;
    sample.isSustainedSample =
        (itSampleHeader.flag & IT_SMP_SUSTAIN_LOOP_ON) != 0;
    sample.isSustainedPingpongSample =
        (itSampleHeader.flag & IT_SMP_PINGPONG_SUSTAIN_LOOP_ON) != 0;
    sample.globalVolume = itSampleHeader.globalVolume;
    if ( sample.globalVolume > IT_SMP_MAX_GLOBAL_VOLUME )
        sample.globalVolume = IT_SMP_MAX_GLOBAL_VOLUME;
    sample.volume = itSampleHeader.volume;
    if ( sample.volume > IT_SMP_MAX_VOLUME )
        sample.volume = IT_SMP_MAX_VOLUME;

    // itSampleHeader.defaultPanning & IT_SMP_USE_DEFAULT_PANNING // TODO!!

    unsigned panning = itSampleHeader.defaultPanning & 0x7F;
    if ( panning > 64 ) 
        panning = 64;
    panning <<= 2;
    if ( panning > 255 ) 
        panning = 255;
    sample.panning = panning;

    sample.vibratoDepth = itSampleHeader.vibratoDepth;
    sample.vibratoRate = itSampleHeader.vibratoRate;
    sample.vibratoSpeed = itSampleHeader.vibratoSpeed;
    sample.vibratoWaveForm = itSampleHeader.vibratoWaveForm & 0x3;

    // finetune + relative note recalc
    unsigned int itPeriod = ((unsigned)8363 * periods[5 * 12]) / itSampleHeader.c5Speed;
    unsigned j;
    for ( j = 0; j < MAXIMUM_NOTES; j++ ) {
        if ( itPeriod >= periods[j] ) 
            break;
    }
    if ( j < MAXIMUM_NOTES ) {
        sample.relativeNote = j - (5 * 12);
        sample.finetune = (int)round(
            ((double)(133 - j) - 12.0 * log2( (double)itPeriod / 13.375 ))
            * 128.0 ) - 128;
    } 
    else {
        sample.relativeNote = 0;
        sample.finetune = 0;
    }
    if ( showDebugInfo_ )
        std::cout
            << "\nrelative note       : "
            << noteStrings[5 * 12 + sample.relativeNote]
            << "\nfinetune:           : " << sample.finetune << "\n";

    // Now take care of the sample data:
    unsigned    dataLength = sample.length;
    
    if ( is16BitSample ) 
        dataLength <<= 1;
     
    if( isStereoSample )
        dataLength <<= 1;

    itFile.absSeek( itSampleHeader.samplePointer );
    std::unique_ptr<unsigned char[]> buffer = 
        std::make_unique<unsigned char[]>( dataLength );

    if ( !isCompressed ) {
        if ( itFile.read( buffer.get(),dataLength ) ) 
            return 0;
    } 
    else {
        // decompress sample here
        ItSex itSex( isIt215Compression );
        if ( is16BitSample )
            itSex.decompress16( itFile,buffer.get(),sample.length );
        else
            itSex.decompress8( itFile,buffer.get(),sample.length );

        if ( isStereoSample ) {
            if ( is16BitSample )
                itSex.decompress16( itFile,buffer.get() + dataLength / 2,sample.length );
            else
                itSex.decompress8( itFile,buffer.get() + dataLength / 2,sample.length );
        }
    }

    // Load the sample:
    sample.data = (std::int16_t *)buffer.get();
    bool unsignedData = (itSampleHeader.convert & IT_SIGNED_SAMPLE_DATA) == 0;    

    sample.dataType =   (unsignedData   ? 0 : SAMPLEDATA_IS_SIGNED_FLAG) |
                        (is16BitSample  ? SAMPLEDATA_IS_16BIT_FLAG : 0) |
                        (isStereoSample ? SAMPLEDATA_IS_STEREO_FLAG : 0);

    samples_[sampleNr] = std::make_unique<Sample>( sample );

    // if the file is in sample mode, convert it to instrument mode
    // and create an instrument for each sample:
    if ( convertToInstrument ) {
        InstrumentHeader    instrumentHeader;
        for ( int n = 0; n < MAXIMUM_NOTES; n++ ) {
            instrumentHeader.sampleForNote[n].note = n;
            instrumentHeader.sampleForNote[n].sampleNr = sampleNr;
        }
        instrumentHeader.name = sample.name;
        instruments_[sampleNr] = std::make_unique <Instrument>( instrumentHeader );
    }
    if ( showDebugInfo_ && !isStereoSample ) {
#ifdef debug_it_play_samples
        playSampleNr( sampleNr );
#endif
    }
    return 0;
}

// Pattern decoder helper functions (effect remapping):
void decodeItVolumeColumn( Effect& target,unsigned char volc )
{
    // table to convert volume column portamento to normal portamento
    const unsigned char itVolcPortaTable[] = {
        0x0,0x1,0x4,0x8,0x10,0x20,0x40,0x60,0x80,0xFF
    };
    target.effect = 0;
    target.argument = 0;
    if ( volc >= IT_VOLUME_COLUMN_UNDEFINED ) 
        return; // nothing to do

    if ( volc >= IT_VOLUME_COLUMN_VIBRATO ) {
        target.effect = VIBRATO;
        target.argument = volc - IT_VOLUME_COLUMN_VIBRATO;
    } 
    else if ( volc >= IT_VOLUME_COLUMN_TONE_PORTAMENTO ) {
        target.effect = TONE_PORTAMENTO;
        unsigned idx = volc - IT_VOLUME_COLUMN_TONE_PORTAMENTO;
        target.argument = itVolcPortaTable[idx];
    } 
    else if ( volc >= IT_VOLUME_COLUMN_SET_PANNING ) {
        target.effect = SET_FINE_PANNING;
        unsigned panning = volc - IT_VOLUME_COLUMN_SET_PANNING;
        panning <<= 2;
        if ( panning > 255 ) panning = 255;
        target.argument = panning;
    } 
    else if ( volc > IT_VOLUME_COLUMN_PORTAMENTO_UP ) {// to check: one or zero based?
        target.effect = PORTAMENTO_UP;
        target.argument = (volc - IT_VOLUME_COLUMN_PORTAMENTO_UP) << 2;
    } 
    else if ( volc > IT_VOLUME_COLUMN_PORTAMENTO_DOWN ) {// to check: one or zero based?
        target.effect = PORTAMENTO_DOWN;
        target.argument = (volc - IT_VOLUME_COLUMN_PORTAMENTO_DOWN) << 2;
    } 
    else if ( volc > IT_VOLUME_COLUMN_VOLUME_SLIDE_DOWN ) {
        target.effect = VOLUME_SLIDE;
        target.argument = volc - IT_VOLUME_COLUMN_VOLUME_SLIDE_DOWN;
    } 
    else if ( volc > IT_VOLUME_COLUMN_VOLUME_SLIDE_UP ) {
        target.effect = VOLUME_SLIDE;
        target.argument = (volc - IT_VOLUME_COLUMN_VOLUME_SLIDE_UP) << 4;
    } 
    else if ( volc > IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_DOWN ) {
        target.effect = EXTENDED_EFFECTS;
        target.argument = volc - IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_DOWN;
        target.argument |= FINE_VOLUME_SLIDE_DOWN << 4;
    } 
    else if ( volc > IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_UP ) {
        target.effect = EXTENDED_EFFECTS;
        target.argument = volc - IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_UP;
        target.argument |= FINE_VOLUME_SLIDE_UP << 4;
    } 
    else {// IT_VOLUME_COLUMN_SET_VOLUME
        target.effect = SET_VOLUME;
        target.argument = volc;
    }
}

void remapItEffects( Effect& remapFx )
{
    switch ( remapFx.effect ) {
        case 1:  // A: set Speed
        {
            remapFx.effect = SET_TEMPO;
            if ( !remapFx.argument ) {
                remapFx.effect = NO_EFFECT;
                remapFx.argument = NO_EFFECT;
            }
            break;
        }
        case 2: // B
        {
            remapFx.effect = POSITION_JUMP;
            break;
        }
        case 3: // C
        {
            remapFx.effect = PATTERN_BREAK;
            break;
        }
        case 4: // D: all kinds of (fine) volume slide
        {
            remapFx.effect = VOLUME_SLIDE; // default
            // check if the command argument is legal:
            unsigned slide1 = remapFx.argument >> 4;
            unsigned slide2 = remapFx.argument & 0xF;
            if ( slide1 & slide2 ) {
                // these are fine slides:
                if ( (slide1 == 0xF) || (slide2 == 0xF) )
                    break;
                // illegal volume slide effect:
                else {                     
                    remapFx.effect = NO_EFFECT;
                    remapFx.argument = 0;
                }
            }
            break;
        }
        case 5: // E: all kinds of (extra) (fine) portamento down
        {
            remapFx.effect = PORTAMENTO_DOWN;
            break;
        }
        case 6: // F: all kinds of (extra) (fine) portamento up
        {
            remapFx.effect = PORTAMENTO_UP;
            break;
        }
        case 7: // G
        {
            remapFx.effect = TONE_PORTAMENTO;
            break;
        }
        case 8: // H
        {
            remapFx.effect = VIBRATO;
            break;
        }
        case 9: // I
        {
            remapFx.effect = TREMOR;
            break;
        }
        case 10: // J
        {
            remapFx.effect = ARPEGGIO;
            break;
        }
        case 11: // K
        {
            remapFx.effect = VIBRATO_AND_VOLUME_SLIDE;
            break;
        }
        case 12: // L
        {
            remapFx.effect = TONE_PORTAMENTO_AND_VOLUME_SLIDE;
            break;
        }
        // effects 'M' and 'N': set chn vol, chn vol slide
        case 15: // O
        {
            remapFx.effect = SET_SAMPLE_OFFSET;
            break;
        }
        // effect 'P': panning slide
        case 17: // Q
        {
            remapFx.effect = MULTI_NOTE_RETRIG; // retrig + volslide
            break;
        }
        case 18: // R
        {
            remapFx.effect = TREMOLO;
            break;
        }
        case 19: // extended effects 'S'
        {
            remapFx.effect = EXTENDED_EFFECTS;
            break;
        }
        case 20: // T
        {
            remapFx.effect = SET_BPM;
            if ( remapFx.argument < 0x20 )
            {
                remapFx.effect = NO_EFFECT;
                remapFx.argument = NO_EFFECT;
            }
            break;
        }
        case 21: // U 
        {
            remapFx.effect = FINE_VIBRATO;
            break;
        }
        case 22: // V
        {
            remapFx.effect = SET_GLOBAL_VOLUME;
            break;
        }
        case 23: // W
        {
            remapFx.effect = GLOBAL_VOLUME_SLIDE;
            break;
        }
        case 24: // X
        {
            remapFx.effect = SET_FINE_PANNING;
            break;
        }
        case 25: // Y
        {
            remapFx.effect = PANBRELLO;
            break;
        }
        default: // unknown effect command
        {
            remapFx.effect = NO_EFFECT;
            remapFx.argument = NO_EFFECT;
            break;
        }
    }
}

// file pointer must be at the correct offset
// returns non-zero on error
// pattern numbers are 0-based
int Module::loadItPattern( VirtualFile& itFile,int patternNr )
{
    unsigned char   masks[IT_MAX_CHANNELS];
    Note            prevRow[IT_MAX_CHANNELS];
    unsigned char   prevVolc[IT_MAX_CHANNELS];
    ItPatternHeader itPatternHeader;

    memset( &masks,0,sizeof( masks ) );
    memset( &prevRow,0,sizeof( prevRow ) );
    memset( &prevVolc,255,sizeof( prevVolc ) );

    if ( itFile.read( &itPatternHeader,sizeof( ItPatternHeader ) ) ) 
        return -1;
    if ( itPatternHeader.nRows > IT_MAX_PATTERN_ROWS ||
        itPatternHeader.nRows < IT_MIN_PATTERN_ROWS ) {
        if ( showDebugInfo_ )
            std::cout 
                << "\nPattern " << patternNr << " has more than "
                << IT_MAX_PATTERN_ROWS << " rows: " << itPatternHeader.nRows
                << "! Exiting.\n";
        return -1;
    }
    std::vector<Note> patternData( nrChannels_ * itPatternHeader.nRows );
    //std::vector<Note>::iterator iNote = patternData.begin();

    // start decoding:
    unsigned char *source = (unsigned char *)itFile.getSafePointer( itPatternHeader.dataSize );
    if ( source == nullptr ) {
        return -1; // DEBUG
    }
    for ( unsigned rowNr = 0; rowNr < itPatternHeader.nRows; ) {
        unsigned char pack;
        if ( itFile.read( &pack,sizeof( unsigned char ) ) ) 
            return -1;
        if ( pack == IT_PATTERN_END_OF_ROW_MARKER ) {
            rowNr++;
            continue;
        }
        unsigned channelNr = (pack - 1) & 63;
        unsigned char& mask = masks[channelNr];
        Note note;
        unsigned char volc;
        if ( pack & IT_PATTERN_CHANNEL_MASK_AVAILABLE )
            if ( itFile.read( &mask,sizeof( unsigned char ) ) ) 
                return -1;

        if ( mask & IT_PATTERN_NOTE_PRESENT ) {
            unsigned char n;
            if ( itFile.read( &n,sizeof( unsigned char ) ) ) 
                return -1;
            if ( n == IT_KEY_OFF ) n = KEY_OFF;
            else if ( n == IT_NOTE_CUT ) n = KEY_NOTE_CUT;
            else if ( n > IT_MAX_NOTE ) n = KEY_NOTE_FADE;
            else n++;
            note.note = n;
            prevRow[channelNr].note = n;
        } 
        else 
            note.note = 0;

        if ( mask & IT_PATTERN_INSTRUMENT_PRESENT ) {
            unsigned char inst;
            if ( itFile.read( &inst,sizeof( unsigned char ) ) ) 
                return -1;
            note.instrument = inst;
            prevRow[channelNr].instrument = inst;
        } 
        else 
            note.instrument = 0;

        if ( mask & IT_PATTERN_VOLUME_COLUMN_PRESENT ) {
            if ( itFile.read( &volc,sizeof( unsigned char ) ) ) 
                return -1;
            prevVolc[channelNr] = volc;
        } 
        else 
            volc = 255;

        if ( mask & IT_PATTERN_COMMAND_PRESENT ) {
            unsigned char fx;
            unsigned char fxArg;
            if ( itFile.read( &fx,sizeof( unsigned char ) ) ) 
                return -1;
            if ( itFile.read( &fxArg,sizeof( unsigned char ) ) ) 
                return -1;
            note.effects[1].effect = fx;
            note.effects[1].argument = fxArg;
            prevRow[channelNr].effects[1].effect = fx;
            prevRow[channelNr].effects[1].argument = fxArg;
        } 
        else {
            note.effects[1].effect = NO_EFFECT;
            note.effects[1].argument = 0;
        }

        if ( mask & IT_PATTERN_LAST_NOTE_IN_CHANNEL )
            note.note = prevRow[channelNr].note;

        if ( mask & IT_PATTERN_LAST_INST_IN_CHANNEL )
            note.instrument = prevRow[channelNr].instrument;

        if ( mask & IT_PATTERN_LAST_VOLC_IN_CHANNEL )
            volc = prevVolc[channelNr];

        if ( mask & IT_PATTERN_LAST_COMMAND_IN_CHANNEL ) {
            note.effects[1].effect = prevRow[channelNr].effects[1].effect;
            note.effects[1].argument = prevRow[channelNr].effects[1].argument;
        }

        decodeItVolumeColumn( note.effects[0],volc );
        remapItEffects( note.effects[1] );

        //if ( channelNr < nrChannels_ ) 
            patternData[rowNr * nrChannels_ + channelNr] = note;
    }
    if ( showDebugInfo_ ) {
#ifdef debug_it_show_patterns
#define IT_DEBUG_SHOW_MAX_CHN 13
        _getch();
        std::cout << "\nPattern nr " << patternNr << ":\n";
        ItDebugShow::pattern( *(patterns_[patternNr]) );
#endif
    }
    patterns_[patternNr] = std::make_unique < Pattern >
        ( nrChannels_,itPatternHeader.nRows,patternData );
    return 0;
}

// this function quickly scans through the patterns and checks which channels
// are actually used. We use the result of this function to determine the 
// number of channels in the song.
int getItActiveChannels( VirtualFile& itFile,bool (&channelIsUsed)[IT_MAX_CHANNELS] )
{
    unsigned char   masks[IT_MAX_CHANNELS];
    ItPatternHeader itPatternHeader;
    memset( &masks,0,sizeof( unsigned char ) * IT_MAX_CHANNELS );

    if ( itFile.read( &itPatternHeader,sizeof( ItPatternHeader ) ) != VIRTFILE_NO_ERROR )
        return -1;
    if ( itPatternHeader.nRows > IT_MAX_PATTERN_ROWS ||
        itPatternHeader.nRows < IT_MIN_PATTERN_ROWS ) {
        return -1;
    }
    // start decoding:
    unsigned char* source = (unsigned char*)itFile.getSafePointer( itPatternHeader.dataSize );
    if ( source == nullptr ) {
        return -1; 
    }
    for ( unsigned rowNr = 0; rowNr < itPatternHeader.nRows; ) {
        unsigned char pack;
        if ( itFile.read( &pack,sizeof( unsigned char ) ) != VIRTFILE_NO_ERROR )
            return -1;
        if ( pack == IT_PATTERN_END_OF_ROW_MARKER ) {
            rowNr++;
            continue;
        }
        unsigned channelNr = (pack - 1) & 63;
        channelIsUsed[channelNr] = true;
        unsigned char& mask = masks[channelNr];
        unsigned skip = 0;
        if ( pack & IT_PATTERN_CHANNEL_MASK_AVAILABLE ) {
            if ( itFile.read( &mask,sizeof( unsigned char ) ) != VIRTFILE_NO_ERROR )
                return -1;
        }
        if ( mask & IT_PATTERN_NOTE_PRESENT ) 
            skip++;
        if ( mask & IT_PATTERN_INSTRUMENT_PRESENT ) 
            skip++;
        if ( mask & IT_PATTERN_VOLUME_COLUMN_PRESENT ) 
            skip++;
        if ( mask & IT_PATTERN_COMMAND_PRESENT ) 
            skip += 2;
        if ( itFile.relSeek( skip ) != VIRTFILE_NO_ERROR )
            return -1;
    }
    return 0;
}


// DEBUG helper functions that write verbose output to the screen:

void ItDebugShow::fileHeader( ItFileHeader& itFileHeader )
{
    std::cout << "\nIT Header: "
        << "\nTag                         : " << itFileHeader.tag[0]
        << itFileHeader.tag[1] << itFileHeader.tag[2] << itFileHeader.tag[3]
        << "\nSong Name                   : " << itFileHeader.songName
        << "\nPhilight                    : " << itFileHeader.phiLight
        << "\nSong length                 : " << itFileHeader.songLength
        << "\nNr of instruments           : " << itFileHeader.nrInstruments
        << "\nNr of samples               : " << itFileHeader.nrSamples
        << "\nNr of patterns              : " << itFileHeader.nrPatterns << std::hex
        << "\nCreated w/ tracker version  : 0x" << itFileHeader.createdWTV
        << "\nCompat. w/ tracker version  : 0x" << itFileHeader.compatibleWTV
        << "\nFlags                       : 0x" << itFileHeader.flags
        << "\nSpecial                     : " << itFileHeader.special << std::dec
        << "\nGlobal volume               : " << (unsigned)itFileHeader.globalVolume
        << "\nMixing amplitude            : " << (unsigned)itFileHeader.mixingAmplitude
        << "\nInitial speed               : " << (unsigned)itFileHeader.initialSpeed
        << "\nInitial bpm                 : " << (unsigned)itFileHeader.initialBpm
        << "\nPanning separation (0..128) : " << (unsigned)itFileHeader.panningSeparation
        << "\nPitch wheel depth           : " << (unsigned)itFileHeader.pitchWheelDepth
        << "\nLength of song message      : " << itFileHeader.messageLength
        << "\nOffset of song message      : " << itFileHeader.messageOffset
        << "\n"
        << "\nDefault volume for each channel: \n";
    for ( int i = 0; i < IT_MAX_CHANNELS; i++ )
        std::cout << std::setw( 3 ) << (unsigned)itFileHeader.defaultVolume[i] << "|";
    std::cout << "\n\nDefault panning for each channel: \n";
    for ( int i = 0; i < IT_MAX_CHANNELS; i++ )
        std::cout << std::setw( 3 ) << (unsigned)itFileHeader.defaultPanning[i] << "|";
}

void ItDebugShow::oldInstHeader( ItOldInstHeader& itOldInstHeader )
{
    std::cout 
        << "\nInstrument headertag: " << itOldInstHeader.tag[0]
        << itOldInstHeader.tag[1] << itOldInstHeader.tag[2] << itOldInstHeader.tag[3]
        << "\nInstrument filename : " << itOldInstHeader.fileName << std::hex
        << "\nflag                : " << (unsigned)itOldInstHeader.flag << std::dec
        << "\nVolume loop start   : " << (unsigned)itOldInstHeader.volumeLoopStart
        << "\nVolume loop end     : " << (unsigned)itOldInstHeader.volumeLoopEnd
        << "\nSustain loop start  : " << (unsigned)itOldInstHeader.sustainLoopStart
        << "\nSustain loop end    : " << (unsigned)itOldInstHeader.sustainLoopEnd
        << "\nFade out            : " << itOldInstHeader.fadeOut << std::hex
        << "\nNew Note Action     : " << (unsigned)itOldInstHeader.NNA
        << "\nDupl. Note Action   : " << (unsigned)itOldInstHeader.DNC
        << "\nTracker version     : " << itOldInstHeader.trackerVersion
        << "\nNr of samples       : " << (unsigned)itOldInstHeader.nrSamples
        << "\nInstrument name     : " << itOldInstHeader.name
        << "\n\n"
        << "Note -> Sample table: \n" << std::dec;
    for ( int i = 0; i < 120; i++ )
        std::cout << std::setw( 4 ) << (unsigned)itOldInstHeader.sampleForNote[i].sampleNr;
    std::cout << "\n\nVolume envelope points: \n";
    for ( int i = 0; i < 200; i++ )
        std::cout << std::setw( 4 ) << (unsigned)itOldInstHeader.volumeEnvelope[i];
    std::cout << "\n\nEnvelope node points (?): \n";
    for ( int i = 0; i < IT_MAX_ENVELOPE_NODES; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itOldInstHeader.nodes[i].tickIndex << ","
        << std::setw( 2 ) << (unsigned)itOldInstHeader.nodes[i].magnitude << " ";
    std::cout << "\n\n";
}

void ItDebugShow::newInstHeader( ItNewInstHeader& itNewInstHeader )
{
    std::cout 
        << "\nInstrument headertag: " << itNewInstHeader.tag[0]
        << itNewInstHeader.tag[1] << itNewInstHeader.tag[2] << itNewInstHeader.tag[3]
        << "\nInstrument filename : " << itNewInstHeader.fileName 
        << std::hex
        << "\nNew Note Action     : " << (unsigned)itNewInstHeader.NNA
        << "\nDup check type      : " << (unsigned)itNewInstHeader.dupCheckType
        << "\nDup check Action    : " << (unsigned)itNewInstHeader.dupCheckAction 
        << std::dec
        << "\nFade out (0..128)   : " << (unsigned)itNewInstHeader.fadeOut    
        << "\nPitch Pan separation: " << (int     )itNewInstHeader.pitchPanSeparation // -32 .. +32
        << "\nPitch pan center    : " << (unsigned)itNewInstHeader.pitchPanCenter // C-0 .. B-9 <=> 0..119
        << "\nGlobal volume (128) : " << (unsigned)itNewInstHeader.globalVolume   // 0..128
        << "\nDefault panning     : " << (unsigned)itNewInstHeader.defaultPanning // 0..64, don't use if bit 7 is set
        << "\nRandom vol variation: " << (unsigned)itNewInstHeader.randVolumeVariation // expressed in percent
        << "\nRandom pan variation: " << (unsigned)itNewInstHeader.randPanningVariation 
        << std::hex
        << "\nTracker version     : " << itNewInstHeader.trackerVersion 
        << std::dec
        << "\nNr of samples       : " << (unsigned)itNewInstHeader.nrSamples
        << "\nInstrument name     : " << itNewInstHeader.name
        << "\nInit. filter cut off: " << (unsigned)itNewInstHeader.initialFilterCutOff
        << "\nInit. filter reson. : " << (unsigned)itNewInstHeader.initialFilterResonance
        << "\nMidi channel        : " << (unsigned)itNewInstHeader.midiChannel
        << "\nMidi program        : " << (unsigned)itNewInstHeader.midiProgram
        << "\nMidi bank           : " << (unsigned)itNewInstHeader.midiBank
        << "\n\n"
        << "Note -> Sample table: \n" << std::dec;
    for ( int i = 0; i < 120; i++ )
        std::cout << std::setw( 4 ) << (unsigned)itNewInstHeader.sampleForNote[i].sampleNr;
    std::cout << "\n\nVolume envelope: "
        << "\nFlag                : " << (unsigned)itNewInstHeader.volumeEnvelope.flag
        << "\nNr of nodes         : " << (unsigned)itNewInstHeader.volumeEnvelope.nrNodes
        << "\nLoop start          : " << (unsigned)itNewInstHeader.volumeEnvelope.loopStart
        << "\nLoop end            : " << (unsigned)itNewInstHeader.volumeEnvelope.loopEnd
        << "\nSustain loop start  : " << (unsigned)itNewInstHeader.volumeEnvelope.sustainLoopStart
        << "\nSustain loop end    : " << (unsigned)itNewInstHeader.volumeEnvelope.sustainLoopEnd
        << "\nEnvelope node points: \n" << std::dec;
    for ( int i = 0; i < IT_MAX_ENVELOPE_NODES; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itNewInstHeader.volumeEnvelope.nodes[i].magnitude << ":"
        << std::setw( 4 ) << (unsigned)itNewInstHeader.volumeEnvelope.nodes[i].tickIndex << " ";
    std::cout << "\n\nPanning envelope: "
        << "\nFlag                : " << (unsigned)itNewInstHeader.panningEnvelope.flag
        << "\nNr of nodes         : " << (unsigned)itNewInstHeader.panningEnvelope.nrNodes
        << "\nLoop start          : " << (unsigned)itNewInstHeader.panningEnvelope.loopStart
        << "\nLoop end            : " << (unsigned)itNewInstHeader.panningEnvelope.loopEnd
        << "\nSustain loop start  : " << (unsigned)itNewInstHeader.panningEnvelope.sustainLoopStart
        << "\nSustain loop end    : " << (unsigned)itNewInstHeader.panningEnvelope.sustainLoopEnd
        << "\nEnvelope node points: \n" << std::dec;
    for ( int i = 0; i < IT_MAX_ENVELOPE_NODES; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itNewInstHeader.panningEnvelope.nodes[i].magnitude << ":"
        << std::setw( 4 ) << (unsigned)itNewInstHeader.panningEnvelope.nodes[i].tickIndex << " ";
    std::cout << "\n\nPitch envelope: "
        << "\nFlag                : " << (unsigned)itNewInstHeader.pitchFltrEnvelope.flag
        << "\nNr of nodes         : " << (unsigned)itNewInstHeader.pitchFltrEnvelope.nrNodes
        << "\nLoop start          : " << (unsigned)itNewInstHeader.pitchFltrEnvelope.loopStart
        << "\nLoop end            : " << (unsigned)itNewInstHeader.pitchFltrEnvelope.loopEnd
        << "\nSustain loop start  : " << (unsigned)itNewInstHeader.pitchFltrEnvelope.sustainLoopStart
        << "\nSustain loop end    : " << (unsigned)itNewInstHeader.pitchFltrEnvelope.sustainLoopEnd
        << "\nEnvelope node points: \n" << std::dec;
    for ( int i = 0; i < IT_MAX_ENVELOPE_NODES; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itNewInstHeader.pitchFltrEnvelope.nodes[i].magnitude << ":"
        << std::setw( 4 ) << (unsigned)itNewInstHeader.pitchFltrEnvelope.nodes[i].tickIndex << " ";
    std::cout << "\n\n";
}

void ItDebugShow::sampleHeader( ItSampleHeader& itSampleHeader )
{
    bool    is16BitSample = (itSampleHeader.flag & IT_SMP_IS_16_BIT) != 0;
    bool    isCompressed = (itSampleHeader.flag & IT_SMP_IS_COMPRESSED) != 0;
    bool    isStereoSample = (itSampleHeader.flag & IT_SMP_IS_STEREO) != 0;
    std::cout
        << "\nTag                 : " << itSampleHeader.tag[0]
        << itSampleHeader.tag[1] << itSampleHeader.tag[2] << itSampleHeader.tag[3]
        << "\nDos filename        : " << itSampleHeader.fileName
        << "\nGlobal volume       : " << (unsigned)itSampleHeader.globalVolume
        << "\nFlags               : " << (unsigned)itSampleHeader.flag
        << "\nAssociated w/ header: " << ((itSampleHeader.flag & IT_SMP_ASSOCIATED_WITH_HEADER) ? "yes" : "no")
        << "\n16 bit sample       : " << (is16BitSample ? "yes" : "no")
        << "\nStereo sample       : " << (isStereoSample ? "yes" : "no")
        << "\nCompressed sample   : " << (isCompressed ? "yes" : "no")
        << "\nLoop                : " << ((itSampleHeader.flag & IT_SMP_LOOP_ON) ? "on" : "off")
        << "\nSustain loop        : " << ((itSampleHeader.flag & IT_SMP_SUSTAIN_LOOP_ON) ? "on" : "off")
        << "\nPingpong loop       : " << ((itSampleHeader.flag & IT_SMP_PINGPONG_LOOP_ON) ? "on" : "off")
        << "\nPingpong sustain    : " << ((itSampleHeader.flag & IT_SMP_PINGPONG_SUSTAIN_LOOP_ON) ? "on" : "off")
        << "\nVolume              : " << (unsigned)itSampleHeader.volume
        << "\nName                : " << itSampleHeader.name
        << "\nConvert (1 = signed): " << (unsigned)itSampleHeader.convert        // bit0 set: signed smp data (off = unsigned)
        << "\nDefault Panning     : " << (unsigned)itSampleHeader.defaultPanning // 0..64, bit 7 set == use default panning
        << "\nLength              : " << itSampleHeader.length
        << "\nLoop start          : " << itSampleHeader.loopStart
        << "\nLoop end            : " << itSampleHeader.loopEnd << std::hex
        << "\nC5 speed            : " << itSampleHeader.c5Speed << std::dec
        << "\nSustain loop start  : " << itSampleHeader.sustainLoopStart
        << "\nSustain loop end    : " << itSampleHeader.sustainLoopEnd << std::hex
        << "\nSample data pointer : " << itSampleHeader.samplePointer << std::dec
        << "\nVibrato speed       : " << (unsigned)itSampleHeader.vibratoSpeed   // 0..64
        << "\nVibrato depth       : " << (unsigned)itSampleHeader.vibratoDepth   // 0..64
        << "\nVibrato wave form   : " << (unsigned)itSampleHeader.vibratoWaveForm// 0 = sine,1 = ramp down,2 = square,3 = random
        << "\nVibrato speed       : " << (unsigned)itSampleHeader.vibratoRate;   // 0..64
}

void ItDebugShow::pattern( Pattern& pattern )
{
    const int nChannels_ = 32; // TEMP DEBUG!
    for ( unsigned rowNr = 0; rowNr < pattern.getnRows(); rowNr++ ) {
        std::cout << "\n";
        for ( int channelNr = 0; channelNr < IT_DEBUG_SHOW_MAX_CHN; channelNr++ ) {
            // colors in console requires weird shit in windows
            HANDLE hStdin = GetStdHandle( STD_INPUT_HANDLE );
            HANDLE hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
            CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
            GetConsoleScreenBufferInfo( hStdout,&csbiInfo );
            // **************************************************
            Note noteData = pattern.getNote( rowNr * nChannels_ + channelNr );
            unsigned note = noteData.note;
            unsigned instrument = noteData.instrument;

            // display note
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
            std::cout << "|";
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTCYAN );

            if ( note <= MAXIMUM_NOTES ) std::cout << noteStrings[note];
            else if ( note == KEY_OFF ) std::cout << "===";
            else if ( note == KEY_NOTE_CUT ) std::cout << "^^^";
            else std::cout << "--\\"; // KEY_NOTE_FADE
            /*
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
            std::cout << std::setw( 5 ) << noteToPeriod( note,channel.pSample->getFinetune() );

            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTBLUE );
            std::cout << "," << std::setw( 5 ) << channel.period;

            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTMAGENTA );
            std::cout << "," << std::setw( 5 ) << channel.portaDestPeriod;
            */
            // display instrument
            SetConsoleTextAttribute( hStdout,FOREGROUND_YELLOW );
            if ( instrument )
                std::cout << std::dec << std::setw( 2 ) << instrument;
            else 
                std::cout << "  ";
            /*
            // display volume column
            SetConsoleTextAttribute( hStdout,FOREGROUND_GREEN | FOREGROUND_INTENSITY );
            if ( iNote->effects[0].effect )
            std::cout << std::hex << std::uppercase
            << std::setw( 1 ) << iNote->effects[0].effect
            << std::setw( 2 ) << iNote->effects[0].argument;
            else std::cout << "   ";
            */
            /*
            // display volume:
            SetConsoleTextAttribute( hStdout,FOREGROUND_GREEN | FOREGROUND_INTENSITY );
            std::cout << std::hex << std::uppercase
            << std::setw( 2 ) << channel.volume;
            */

            /*
            // effect
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
            for ( unsigned fxloop = 1; fxloop < MAX_EFFECT_COLUMNS; fxloop++ ) {
            if ( noteData.effects[fxloop].effect )
            std::cout
            << std::hex << std::uppercase
            << std::setw( 2 ) << noteData.effects[fxloop].effect;
            else std::cout << "--";
            SetConsoleTextAttribute( hStdout,FOREGROUND_BROWN );
            std::cout
            << std::setw( 2 ) << (noteData.effects[fxloop].argument)
            << std::dec;
            }
            */
            SetConsoleTextAttribute( hStdout,FOREGROUND_LIGHTGRAY );
        }
    }
}
