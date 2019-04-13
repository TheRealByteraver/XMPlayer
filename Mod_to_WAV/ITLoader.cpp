/*
Thanks must go to:
- Jeffrey Lim (Pulse) for creating the awesome Impulse Tracker
- Johannes - Jojo - Schultz (Saga Musix) for helping me out with all kinds of
  questions related to the .IT format.
- Tammo Hinrichs for the .IT sample decompression routines itsex.c which I
  used in my .IT loader. Thanks for explaining how the algorithm works! See 
  itsex.h for details.
*/

#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>

#include "Module.h"
#include "virtualfile.h"
#include "itsex.h"

#define debug_it_loader
#define debug_it_show_instruments
#define debug_it_show_patterns
//#define debug_it_play_samples

#include <bitset>
#include <iomanip>

extern const char *noteStrings[2 + MAXIMUM_NOTES];

#define IT_MAX_CHANNELS                         64
#define IT_MAX_PATTERNS                         200
#define IT_MAX_SONG_LENGTH                      MAX_PATTERNS
#define IT_MIN_PATTERN_ROWS                     32
#define IT_MAX_PATTERN_ROWS                     200
#define IT_MAX_SAMPLES                          99
#define IT_MAX_INSTRUMENTS                      100 // ?
#define IT_MAX_NOTE                             119 // 9 octaves: (C-0 -> B-9)

#define IT_STEREO_FLAG                          1
#define IT_VOL0_OPT_FLAG                        2
#define IT_INSTRUMENT_MODE                      4
#define IT_LINEAR_FREQUENCIES_FLAG              8
#define IT_OLD_EFFECTS_MODE                     16
#define IT_GEF_LINKED_EFFECT_MEMORY             32
#define IT_USE_MIDI_PITCH_CONTROLLER            64
#define IT_REQUEST_MIDI_CONFIG                  128

#define IT_SONG_MESSAGE_FLAG                    1
#define IT_MIDI_CONFIG_EMBEDDED                 8

#define IT_DOS_FILENAME_LENGTH                  12
#define IT_SONG_NAME_LENGTH                     26
#define IT_INST_NAME_LENGTH                     26

// sample flags
#define IT_SMP_NAME_LENGTH                      26
#define IT_SMP_ASSOCIATED_WITH_HEADER           1
#define IT_SMP_IS_16_BIT                        2
#define IT_SMP_IS_STEREO                        4 // not supported by Impulse Trckr
#define IT_SMP_IS_COMPRESSED                    8
#define IT_SMP_LOOP_ON                          16
#define IT_SMP_SUSTAIN_LOOP_ON                  32
#define IT_SMP_PINGPONG_LOOP_ON                 64
#define IT_SMP_PINGPONG_SUSTAIN_LOOP_ON         128
#define IT_SMP_USE_DEFAULT_PANNING              128
#define IT_SMP_MAX_GLOBAL_VOLUME                64
#define IT_SMP_MAX_VOLUME                       64
#define IT_SIGNED_SAMPLE_DATA                   1

// pattern flags
#define IT_END_OF_SONG_MARKER                   255
#define IT_MARKER_PATTERN                       254 
#define IT_PATTERN_CHANNEL_MASK_AVAILABLE       128
#define IT_PATTERN_END_OF_ROW_MARKER            0
#define IT_PATTERN_NOTE_PRESENT                 1
#define IT_PATTERN_INSTRUMENT_PRESENT           2
#define IT_PATTERN_VOLUME_COLUMN_PRESENT        4
#define IT_PATTERN_COMMAND_PRESENT              8
#define IT_PATTERN_LAST_NOTE_IN_CHANNEL         16
#define IT_PATTERN_LAST_INST_IN_CHANNEL         32
#define IT_PATTERN_LAST_VOLC_IN_CHANNEL         64
#define IT_PATTERN_LAST_COMMAND_IN_CHANNEL      128
#define IT_NOTE_CUT                             254
#define IT_KEY_OFF                              255

/*
// constants for decoding the volume column
#define IT_VOLUME_COLUMN_UNDEFINED              213
#define IT_VOLUME_COLUMN_VIBRATO                203
#define IT_VOLUME_COLUMN_TONE_PORTAMENTO        193
#define IT_VOLUME_COLUMN_SET_PANNING            128
#define IT_VOLUME_COLUMN_PORTAMENTO_UP          114
#define IT_VOLUME_COLUMN_PORTAMENTO_DOWN        104
#define IT_VOLUME_COLUMN_VOLUME_SLIDE_DOWN      94
#define IT_VOLUME_COLUMN_VOLUME_SLIDE_UP        84
#define IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_DOWN 74
#define IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_UP   64
#define IT_VOLUME_COLUMN_SET_VOLUME             0
*/


#pragma pack (1) 
struct ItFileHeader {
    char            tag[4];         // "IMPM"
    char            songName[IT_SONG_NAME_LENGTH];
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
    char            fileName[IT_DOS_FILENAME_LENGTH];
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
    char            name[IT_INST_NAME_LENGTH];
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
    char            fileName[IT_DOS_FILENAME_LENGTH];
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
    char            name[IT_INST_NAME_LENGTH];
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
    char            fileName[IT_DOS_FILENAME_LENGTH];
    char            asciiz;
    unsigned char   globalVolume;   // 0..64
    unsigned char   flag;
    unsigned char   volume;         // 
    char            name[IT_SMP_NAME_LENGTH];
    unsigned char   convert;        // bit0 set: signed smp data (off = unsigned)
    unsigned char   defaultPanning; // 0..64, bit 7 set == use default panning
    unsigned        length;
    unsigned        loopStart;
    unsigned        loopEnd;
    unsigned        c5Speed;
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
    //unsigned char   data[65536 - 8];
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

int Module::loadItFile()
{
    isLoaded_ = false;
    VirtualFile itFile( fileName_ );
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;
    ItFileHeader itFileHeader;
    itFile.read( &itFileHeader,sizeof( itFileHeader ) );
    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;

    // some very basic checking
    if ( //(fileSize < S3M_MIN_FILESIZE) ||
        (!((itFileHeader.tag[0] == 'I') &&
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
    for ( int i = 0; i < 4; i++ ) trackerTag_ += itFileHeader.tag[i];
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

    // read the file offset pointers to the instrument, sample and pattern data
    unsigned instHdrPtrs[IT_MAX_INSTRUMENTS];
    unsigned smpHdrPtrs[IT_MAX_SAMPLES];
    unsigned ptnHdrPtrs[IT_MAX_PATTERNS];
    for ( unsigned i = 0; i < itFileHeader.nInstruments; i++ )
        itFile.read( &(instHdrPtrs[i]),sizeof( unsigned ) );
    for ( unsigned i = 0; i < itFileHeader.nSamples; i++ )
        itFile.read( &(smpHdrPtrs[i]),sizeof( unsigned ) );
    for ( unsigned i = 0; i < itFileHeader.nPatterns; i++ )
        itFile.read( &(ptnHdrPtrs[i]),sizeof( unsigned ) );

    if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;
#ifdef debug_it_loader
    ItDebugShow::fileHeader( itFileHeader );
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
    defaultTempo_ = itFileHeader.initialSpeed;
    defaultBpm_ = itFileHeader.initialBpm;

    // load instruments
    nInstruments_ = itFileHeader.nInstruments;
    if ( itFileHeader.flags & IT_INSTRUMENT_MODE )
    {
        for (
            int instrumentNr = 1;
            instrumentNr <= itFileHeader.nInstruments;
            instrumentNr++ )
        {
            itFile.absSeek( instHdrPtrs[instrumentNr - 1] );
            if ( itFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;
            int result = loadItInstrument( itFile,instrumentNr,itFileHeader.createdWTV );
            if ( result ) return 0;
        }
    }

    // load samples
    nSamples_ = itFileHeader.nSamples;
    bool convertToInstrument = !(itFileHeader.flags & IT_INSTRUMENT_MODE);
    if ( convertToInstrument ) nInstruments_ = nSamples_;
    for ( unsigned sampleNr = 1; sampleNr <= nSamples_; sampleNr++ )
    {
        itFile.absSeek( smpHdrPtrs[sampleNr - 1] );
        int result = loadItSample(
            itFile,
            sampleNr,
            convertToInstrument,
            isIt215Compression
        );
        if ( result ) return 0;
    }

    // load patterns
    for ( int patternNr = 0; patternNr < itFileHeader.nPatterns; patternNr++ )
    {
        unsigned offset = ptnHdrPtrs[patternNr];
        if ( offset )
        {
            itFile.absSeek( offset );
            int result = loadItPattern( itFile,patternNr );
            if ( result ) return 0;
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
    if ( createdWTV < 0x200 ) // old IT instrument header
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
            return -1;
        }
#ifdef debug_it_loader
#ifdef debug_it_show_instruments
        std::cout << std::endl << "Instrument nr " << instrumentNr << " Header: ";
        ItDebugShow::oldInstHeader( itInstHeader );
#endif
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
        //instrumentHeader.nSamples = 0;       // can probably be removed
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
            return -1;
        }
#ifdef debug_it_loader
#ifdef debug_it_show_instruments
        std::cout << std::endl << "Instrument nr " << instrumentNr << " Header: ";
        ItDebugShow::newInstHeader( itInstHeader );
#endif
#endif
    }
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
    if ( itFile.read( &itSampleHeader,sizeof( itSampleHeader ) ) ) return -1;
    bool    is16bitData = (itSampleHeader.flag & IT_SMP_IS_16_BIT) != 0;
    bool    isCompressed = (itSampleHeader.flag & IT_SMP_IS_COMPRESSED) != 0;
    bool    isStereoSample = (itSampleHeader.flag & IT_SMP_IS_STEREO) != 0;
#ifdef debug_it_loader
    std::cout << std::endl << std::endl << "Sample header nr " << sampleNr << ":";
    ItDebugShow::sampleHeader( itSampleHeader );
#endif
    if ( itSampleHeader.flag & IT_SMP_ASSOCIATED_WITH_HEADER )
    {
        if ( isStereoSample // || (itSampleHeader.flag & IT_SMP_IS_COMPRESSED) 
            )
        {
#ifdef debug_it_loader
            std::cout << std::endl << std::endl
                << "Sample header nr " << sampleNr << ": "
                << "Stereo samples are not supported yet!";
#endif
            return -1;
        }
        SampleHeader sample;
        samples_[sampleNr] = new Sample;

        //for ( int i = 0; i < IT_SMP_NAME_LENGTH; i++ ) sample.name += itSampleHeader.name[i];
        sample.name.append( itSampleHeader.name,IT_SMP_NAME_LENGTH );
        //std::cout << "\n!!! " << sample.name << "!!!\n";


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
        if ( panning > 64 ) panning = 64;
        panning <<= 2;
        if ( panning > 255 ) panning = 255;
        sample.panning = panning;

        sample.vibratoDepth = itSampleHeader.vibratoDepth;
        sample.vibratoRate = itSampleHeader.vibratoRate;
        sample.vibratoSpeed = itSampleHeader.vibratoSpeed;
        sample.vibratoWaveForm = itSampleHeader.vibratoWaveForm & 0x3;

        // finetune + relative note recalc
        unsigned int itPeriod = ((unsigned)8363 * periods[5 * 12]) / itSampleHeader.c5Speed;
        unsigned j;
        for ( j = 0; j < MAXIMUM_NOTES; j++ ) {
            if ( itPeriod >= periods[j] ) break;
        }
        if ( j < MAXIMUM_NOTES ) {
            sample.relativeNote = j - (5 * 12);
            sample.finetune = (int)round(
                ((double)(133 - j) - 12.0 * log2( (double)itPeriod / 13.375 ))
                * 128.0 ) - 128;
        } else {
            sample.relativeNote = 0;
            sample.finetune = 0;
        }
#ifdef debug_it_loader
        std::cout
            << std::endl << "relative note       : "
            << noteStrings[5 * 12 + sample.relativeNote]
            << std::endl << "finetune:           : " << sample.finetune
            << std::endl;
#endif

        // Now take care of the sample data:
        unsigned    dataLength = sample.length;
        if ( is16bitData ) {
            sample.dataType = SAMPLEDATA_SIGNED_16BIT;
            dataLength <<= 1;
        } else {
            sample.dataType = SAMPLEDATA_SIGNED_8BIT;
        }
        itFile.absSeek( itSampleHeader.samplePointer );
        unsigned char *buffer = new unsigned char[dataLength];

        if ( !isCompressed )
        {
            if ( itFile.read( buffer,dataLength ) ) return 0;
        } else {
            // decompress sample here
            ItSex itSex( isIt215Compression );
            if ( is16bitData )
                itSex.itsex_decompress16( itFile,buffer,sample.length );
            else
                itSex.itsex_decompress8( itFile,buffer,sample.length );
        }
        sample.data = (SHORT *)buffer;

        // convert unsigned to signed sample data:
        if ( (itSampleHeader.convert & IT_SIGNED_SAMPLE_DATA) == 0 )
        {
            if ( is16bitData )
            {
                SHORT *data = sample.data;
                for ( unsigned i = 0; i < sample.length; i++ ) data[i] ^= 0x8000;
            } else {
                char *data = (char *)sample.data;
                for ( unsigned i = 0; i < sample.length; i++ ) data[i] ^= 0x80;
            }
        }

        samples_[sampleNr]->load( sample );
        delete buffer;

        // if the file is in sample mode, convert it to instrument mode
        // and create an instrument for each sample:
        if ( convertToInstrument )
        {
            instruments_[sampleNr] = new Instrument;
            InstrumentHeader    instrumentHeader;
            for ( int n = 0; n < IT_MAX_NOTE; n++ )
            {
                instrumentHeader.sampleForNote[n] = sampleNr;
            }
            instrumentHeader.name = sample.name;
            //std::cout << "\ninstrumentheader.name: " << instrumentHeader.name << "#\n";
            instruments_[sampleNr]->load( instrumentHeader );
        }
#ifdef debug_it_loader
#ifdef debug_it_play_samples
        std::cout << "\nSample " << sampleNr << ": name     = " << samples_[sampleNr]->getName().c_str();
        if ( !samples_[sampleNr] ) _getch();
        if ( samples_[sampleNr] )
        {
            HWAVEOUT        hWaveOut;
            WAVEFORMATEX    waveFormatEx;
            MMRESULT        result;
            WAVEHDR         waveHdr;

            std::cout << "\nSample " << sampleNr << ": length   = " << samples_[sampleNr]->getLength();
            std::cout << "\nSample " << sampleNr << ": rep ofs  = " << samples_[sampleNr]->getRepeatOffset();
            std::cout << "\nSample " << sampleNr << ": rep len  = " << samples_[sampleNr]->getRepeatLength();
            std::cout << "\nSample " << sampleNr << ": volume   = " << samples_[sampleNr]->getVolume();
            std::cout << "\nSample " << sampleNr << ": finetune = " << samples_[sampleNr]->getFinetune();

            if ( samples_[sampleNr]->getData() )
            {

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
                    std::cout << "\nError opening wave mapper!\n";
                } else {
                    int retry = 0;
                    if ( sampleNr == 1 ) std::cout << "\nWave mapper successfully opened!\n";
                    waveHdr.dwBufferLength = samples_[sampleNr]->getLength() *
                        waveFormatEx.nBlockAlign;
                    waveHdr.lpData = (LPSTR)(samples_[sampleNr]->getData());
                    waveHdr.dwFlags = 0;

                    result = waveOutPrepareHeader( hWaveOut,&waveHdr,
                        sizeof( WAVEHDR ) );
                    while ( (result != MMSYSERR_NOERROR) && (retry < 10) ) {
                        retry++;
                        std::cout << "\nError preparing wave mapper header!";
                        switch ( result ) {
                            case MMSYSERR_INVALHANDLE:
                            {
                                std::cout << "\nSpecified device handle is invalid.";
                                break;
                            }
                            case MMSYSERR_NODRIVER:
                            {
                                std::cout << "\nNo device driver is present.";
                                break;
                            }
                            case MMSYSERR_NOMEM:
                            {
                                std::cout << "\nUnable to allocate or lock memory.";
                                break;
                            }
                            default:
                            {
                                std::cout << "\nOther unknown error " << result;
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
                        std::cout << "\nError writing to wave mapper!";
                        switch ( result ) {
                            case MMSYSERR_INVALHANDLE:
                            {
                                std::cout << "\nSpecified device handle is invalid.";
                                break;
                            }
                            case MMSYSERR_NODRIVER:
                            {
                                std::cout << "\nNo device driver is present.";
                                break;
                            }
                            case MMSYSERR_NOMEM:
                            {
                                std::cout << "\nUnable to allocate or lock memory.";
                                break;
                            }
                            case WAVERR_UNPREPARED:
                            {
                                std::cout << "\nThe data block pointed to by the pwh parameter hasn't been prepared.";
                                break;
                            }
                            default:
                            {
                                std::cout << "\nOther unknown error " << result;
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
#endif
#endif
    }
    return 0;
}

// constants for decoding the volume column
#define IT_VOLUME_COLUMN_UNDEFINED              213
#define IT_VOLUME_COLUMN_VIBRATO                203
#define IT_VOLUME_COLUMN_TONE_PORTAMENTO        193
#define IT_VOLUME_COLUMN_SET_PANNING            128            
#define IT_VOLUME_COLUMN_PORTAMENTO_UP          114
#define IT_VOLUME_COLUMN_PORTAMENTO_DOWN        104
#define IT_VOLUME_COLUMN_VOLUME_SLIDE_DOWN      94
#define IT_VOLUME_COLUMN_VOLUME_SLIDE_UP        84
#define IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_DOWN 74
#define IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_UP   64
#define IT_VOLUME_COLUMN_SET_VOLUME             0

// Pattern decoder helper functions (effect remapping):
void decodeVolumeColumn( Effect& target,unsigned char volc )
{
    // table to convert volume column portamento to normal portamento
    const unsigned char itVolcPortaTable[] = {
        0x0,0x1,0x4,0x8,0x10,0x20,0x40,0x60,0x80,0xFF
    };
    target.effect = 0;
    target.argument = 0;
    if ( volc >= IT_VOLUME_COLUMN_UNDEFINED ) return; // nothing to do

    if ( volc >= IT_VOLUME_COLUMN_VIBRATO )
    {
        target.effect = VIBRATO;
        target.argument = volc - IT_VOLUME_COLUMN_VIBRATO;
    } else if ( volc >= IT_VOLUME_COLUMN_TONE_PORTAMENTO )
    {
        target.effect = TONE_PORTAMENTO;
        unsigned idx = volc - IT_VOLUME_COLUMN_TONE_PORTAMENTO;
        target.argument = itVolcPortaTable[idx];
    } else if ( volc >= IT_VOLUME_COLUMN_SET_PANNING )
    {
        target.effect = SET_FINE_PANNING;
        unsigned panning = volc - IT_VOLUME_COLUMN_SET_PANNING;
        panning <<= 2;
        if ( panning > 255 ) panning = 255;
        target.argument = panning;
    } else if ( volc > IT_VOLUME_COLUMN_PORTAMENTO_UP ) // to check: one or zero based?
    {
        target.effect = PORTAMENTO_UP;
        target.argument = (volc - IT_VOLUME_COLUMN_PORTAMENTO_UP) << 2;
    } else if ( volc > IT_VOLUME_COLUMN_PORTAMENTO_DOWN ) // to check: one or zero based?
    {
        target.effect = PORTAMENTO_DOWN;
        target.argument = (volc - IT_VOLUME_COLUMN_PORTAMENTO_DOWN) << 2;
    } else if ( volc > IT_VOLUME_COLUMN_VOLUME_SLIDE_DOWN )
    {
        target.effect = VOLUME_SLIDE;
        target.argument = volc - IT_VOLUME_COLUMN_VOLUME_SLIDE_DOWN;
    } else if ( volc > IT_VOLUME_COLUMN_VOLUME_SLIDE_UP )
    {
        target.effect = VOLUME_SLIDE;
        target.argument = (volc - IT_VOLUME_COLUMN_VOLUME_SLIDE_UP) << 4;
    } else if ( volc > IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_DOWN )
    {
        target.effect = EXTENDED_EFFECTS;
        target.argument = volc - IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_DOWN;
        target.argument |= FINE_VOLUME_SLIDE_DOWN << 4;
    } else if ( volc > IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_UP )
    {
        target.effect = EXTENDED_EFFECTS;
        target.argument = volc - IT_VOLUME_COLUMN_FINE_VOLUME_SLIDE_UP;
        target.argument |= FINE_VOLUME_SLIDE_UP << 4;
    } else // IT_VOLUME_COLUMN_SET_VOLUME
    {
        target.effect = SET_VOLUME;
        target.argument = volc;
    }
}

void remapEffects( Effect& remapFx )
{
    switch ( remapFx.effect ) {
        case 1:  // A: set Speed
        {
            remapFx.effect = SET_TEMPO;
            if ( !remapFx.argument )
            {
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
int Module::loadItPattern(
    VirtualFile& itFile,
    int patternNr )
{
    nChannels_ = 32; // = IT_MAX_CHANNELS; // TODO TO FIX!
    boolean         channelIsUsed[IT_MAX_CHANNELS];
    unsigned char   masks[IT_MAX_CHANNELS];
    Note            prevRow[IT_MAX_CHANNELS];
    unsigned char   prevVolc[IT_MAX_CHANNELS];
    memset( &channelIsUsed,false,sizeof( channelIsUsed ) );
    memset( &masks,0,sizeof( masks ) );
    memset( &prevRow,0,sizeof( prevRow ) );
    memset( &prevVolc,255,sizeof( prevVolc ) );

    Note            *iNote,*patternData;
    ItPatternHeader itPatternHeader;
    patterns_[patternNr] = new Pattern;

    if ( itFile.read( &itPatternHeader,sizeof( ItPatternHeader ) ) ) return -1;
    if ( itPatternHeader.nRows > IT_MAX_PATTERN_ROWS ||
        itPatternHeader.nRows < IT_MIN_PATTERN_ROWS )
    {
#ifdef debug_it_loader
        std::cout << std::endl
            << "Pattern " << patternNr << " has more than "
            << IT_MAX_PATTERN_ROWS << " rows: " << itPatternHeader.nRows
            << "! Exiting." << std::endl;
#endif
        return -1;
    }

    patternData = new Note[nChannels_ * itPatternHeader.nRows];
    patterns_[patternNr]->initialise( nChannels_,itPatternHeader.nRows,patternData );
    iNote = patternData;

    // start decoding:
    unsigned char *source = (unsigned char *)itFile.getSafePointer( itPatternHeader.dataSize );
    if ( source == nullptr )
    {
        return -1; // DEBUG
    }

    for ( unsigned rowNr = 0; rowNr < itPatternHeader.nRows; )
    {
        unsigned char pack;
        if ( itFile.read( &pack,sizeof( unsigned char ) ) ) return -1;
        if ( pack == IT_PATTERN_END_OF_ROW_MARKER )
        {
            rowNr++;
            continue;
        }
        unsigned channelNr = (pack - 1) & 63;
        unsigned char& mask = masks[channelNr];
        Note note;
        unsigned char volc;
        if ( pack & IT_PATTERN_CHANNEL_MASK_AVAILABLE )
            if ( itFile.read( &mask,sizeof( unsigned char ) ) ) return -1;

        if ( mask & IT_PATTERN_NOTE_PRESENT )
        {
            unsigned char n;
            if ( itFile.read( &n,sizeof( unsigned char ) ) ) return -1;
            if ( n == IT_KEY_OFF ) n = KEY_OFF;
            else if ( n == IT_NOTE_CUT ) n = KEY_NOTE_CUT;
            else if ( n > IT_MAX_NOTE ) n = KEY_NOTE_FADE;
            else n++;

            note.note = n;
            prevRow[channelNr].note = n;
        } else note.note = 0;

        if ( mask & IT_PATTERN_INSTRUMENT_PRESENT )
        {
            unsigned char inst;
            if ( itFile.read( &inst,sizeof( unsigned char ) ) ) return -1;
            note.instrument = inst;
            prevRow[channelNr].instrument = inst;
        } else note.instrument = 0;

        if ( mask & IT_PATTERN_VOLUME_COLUMN_PRESENT )
        {
            if ( itFile.read( &volc,sizeof( unsigned char ) ) ) return -1;
            prevVolc[channelNr] = volc;
        } else volc = 255;

        if ( mask & IT_PATTERN_COMMAND_PRESENT )
        {
            unsigned char fx;
            unsigned char fxArg;
            if ( itFile.read( &fx,sizeof( unsigned char ) ) ) return -1;
            if ( itFile.read( &fxArg,sizeof( unsigned char ) ) ) return -1;
            note.effects[1].effect = fx;
            note.effects[1].argument = fxArg;
            prevRow[channelNr].effects[1].effect = fx;
            prevRow[channelNr].effects[1].argument = fxArg;
        } else {
            note.effects[1].effect = NO_EFFECT;
            note.effects[1].argument = 0;
        }

        if ( mask & IT_PATTERN_LAST_NOTE_IN_CHANNEL )
            note.note = prevRow[channelNr].note;

        if ( mask & IT_PATTERN_LAST_INST_IN_CHANNEL )
            note.instrument = prevRow[channelNr].instrument;

        if ( mask & IT_PATTERN_LAST_VOLC_IN_CHANNEL )
            volc = prevVolc[channelNr];

        if ( mask & IT_PATTERN_LAST_COMMAND_IN_CHANNEL )
        {
            note.effects[1].effect = prevRow[channelNr].effects[1].effect;
            note.effects[1].argument = prevRow[channelNr].effects[1].argument;
        }
        // decode volume column
        decodeVolumeColumn( note.effects[0],volc );

        // remap main effect column:
        remapEffects( note.effects[1] );

        if ( channelNr < nChannels_ )
        {
            patternData[rowNr * nChannels_ + channelNr] = note;
        }
    }
#ifdef debug_it_loader
#ifdef debug_it_show_patterns
#define IT_DEBUG_SHOW_MAX_CHN 13
    _getch();
    std::cout << std::endl << "Pattern nr " << patternNr << ":" << std::endl;
    ItDebugShow::pattern( *(patterns_[patternNr]) );
#endif
#endif  
    return 0;
}






// DEBUG helper functions that write verbose output to the screen:

void ItDebugShow::fileHeader( ItFileHeader& itFileHeader )
{
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
}

void ItDebugShow::oldInstHeader( ItOldInstHeader& itOldInstHeader )
{
    std::cout //<< std::endl << "Instrument nr " << instrumentNr << " Header: "
        << std::endl << "Instrument headertag: " << itOldInstHeader.tag[0]
        << itOldInstHeader.tag[1] << itOldInstHeader.tag[2] << itOldInstHeader.tag[3]
        << std::endl << "Instrument filename : " << itOldInstHeader.fileName << std::hex
        << std::endl << "flag                : " << (unsigned)itOldInstHeader.flag << std::dec
        << std::endl << "Volume loop start   : " << (unsigned)itOldInstHeader.volumeLoopStart
        << std::endl << "Volume loop end     : " << (unsigned)itOldInstHeader.volumeLoopEnd
        << std::endl << "Sustain loop start  : " << (unsigned)itOldInstHeader.sustainLoopStart
        << std::endl << "Sustain loop end    : " << (unsigned)itOldInstHeader.sustainLoopEnd
        << std::endl << "Fade out            : " << itOldInstHeader.fadeOut << std::hex
        << std::endl << "New Note Action     : " << (unsigned)itOldInstHeader.NNA
        << std::endl << "Dupl. Note Action   : " << (unsigned)itOldInstHeader.DNC
        << std::endl << "Tracker version     : " << itOldInstHeader.trackerVersion
        << std::endl << "Nr of samples       : " << (unsigned)itOldInstHeader.nSamples
        << std::endl << "Instrument name     : " << itOldInstHeader.name
        << std::endl << std::endl
        << "Note -> Sample table: " << std::endl << std::dec;
    for ( int i = 0; i < 240; i++ )
        std::cout << std::setw( 4 ) << (unsigned)itOldInstHeader.sampleForNote[i];
    std::cout << std::endl << std::endl
        << "Volume envelope points: " << std::endl;
    for ( int i = 0; i < 200; i++ )
        std::cout << std::setw( 4 ) << (unsigned)itOldInstHeader.volumeEnvelope[i];
    std::cout << std::endl << std::endl
        << "Envelope node points (?): " << std::endl;
    for ( int i = 0; i < 25; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itOldInstHeader.nodes[i * 2] << ","
        << std::setw( 2 ) << (unsigned)itOldInstHeader.nodes[i * 2 + 1] << " ";
    std::cout << std::endl << std::endl;
}

void ItDebugShow::newInstHeader( ItNewInstHeader& itNewInstHeader )
{
    std::cout //<< std::endl << "Instrument nr " << instrumentNr << " Header: "
        << std::endl << "Instrument headertag: " << itNewInstHeader.tag[0]
        << itNewInstHeader.tag[1] << itNewInstHeader.tag[2] << itNewInstHeader.tag[3]
        << std::endl << "Instrument filename : " << itNewInstHeader.fileName << std::hex
        << std::endl << "New Note Action     : " << (unsigned)itNewInstHeader.NNA
        << std::endl << "Dup check type      : " << (unsigned)itNewInstHeader.dupCheckType
        << std::endl << "Dup check Action    : " << (unsigned)itNewInstHeader.dupCheckAction << std::dec
        << std::endl << "Fade out (0..128)   : " << (unsigned)itNewInstHeader.fadeOut        // 0..128
        << std::endl << "Pitch Pan separation: " << (unsigned)itNewInstHeader.pitchPanSeparation // -32 .. +32
        << std::endl << "Pitch pan center    : " << (unsigned)itNewInstHeader.pitchPanCenter // C-0 .. B-9 <=> 0..119
        << std::endl << "Global volume (128) : " << (unsigned)itNewInstHeader.globalVolume   // 0..128
        << std::endl << "Default panning     : " << (unsigned)itNewInstHeader.defaultPanning // 0..64, don't use if bit 7 is set
        << std::endl << "Random vol variation: " << (unsigned)itNewInstHeader.randVolumeVariation // expressed in percent
        << std::endl << "Random pan variation: " << (unsigned)itNewInstHeader.randPanningVariation << std::hex
        << std::endl << "Tracker version     : " << itNewInstHeader.trackerVersion << std::dec
        << std::endl << "Nr of samples       : " << (unsigned)itNewInstHeader.nSamples
        << std::endl << "Instrument name     : " << itNewInstHeader.name
        << std::endl << "Init. filter cut off: " << (unsigned)itNewInstHeader.initialFilterCutOff
        << std::endl << "Init. filter reson. : " << (unsigned)itNewInstHeader.initialFilterResonance
        << std::endl << "Midi channel        : " << (unsigned)itNewInstHeader.midiChannel
        << std::endl << "Midi program        : " << (unsigned)itNewInstHeader.midiProgram
        << std::endl << "Midi bank           : " << (unsigned)itNewInstHeader.midiBank
        << std::endl << std::endl
        << "Note -> Sample table: " << std::endl << std::dec;
    for ( int i = 0; i < 240; i++ )
        std::cout << std::setw( 4 ) << (unsigned)itNewInstHeader.sampleForNote[i];
    std::cout << std::endl << std::endl
        << "Volume envelope: "
        << std::endl << "Flag                : " << (unsigned)itNewInstHeader.volumeEnvelope.flag
        << std::endl << "Nr of nodes         : " << (unsigned)itNewInstHeader.volumeEnvelope.nNodes
        << std::endl << "Loop start          : " << (unsigned)itNewInstHeader.volumeEnvelope.loopStart
        << std::endl << "Loop end            : " << (unsigned)itNewInstHeader.volumeEnvelope.loopEnd
        << std::endl << "Sustain loop start  : " << (unsigned)itNewInstHeader.volumeEnvelope.sustainLoopStart
        << std::endl << "Sustain loop end    : " << (unsigned)itNewInstHeader.volumeEnvelope.sustainLoopEnd
        << std::endl << "Envelope node points: " << std::endl << std::dec;
    for ( int i = 0; i < 25; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itNewInstHeader.volumeEnvelope.nodes[i].magnitude << ":"
        << std::setw( 4 ) << (unsigned)itNewInstHeader.volumeEnvelope.nodes[i].tickIndex << " ";
    std::cout << std::endl << std::endl
        << "Panning envelope: "
        << std::endl << "Flag                : " << (unsigned)itNewInstHeader.panningEnvelope.flag
        << std::endl << "Nr of nodes         : " << (unsigned)itNewInstHeader.panningEnvelope.nNodes
        << std::endl << "Loop start          : " << (unsigned)itNewInstHeader.panningEnvelope.loopStart
        << std::endl << "Loop end            : " << (unsigned)itNewInstHeader.panningEnvelope.loopEnd
        << std::endl << "Sustain loop start  : " << (unsigned)itNewInstHeader.panningEnvelope.sustainLoopStart
        << std::endl << "Sustain loop end    : " << (unsigned)itNewInstHeader.panningEnvelope.sustainLoopEnd
        << std::endl << "Envelope node points: " << std::endl << std::dec;
    for ( int i = 0; i < 25; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itNewInstHeader.panningEnvelope.nodes[i].magnitude << ":"
        << std::setw( 4 ) << (unsigned)itNewInstHeader.panningEnvelope.nodes[i].tickIndex << " ";
    std::cout << std::endl << std::endl
        << "Pitch envelope: "
        << std::endl << "Flag                : " << (unsigned)itNewInstHeader.pitchEnvelope.flag
        << std::endl << "Nr of nodes         : " << (unsigned)itNewInstHeader.pitchEnvelope.nNodes
        << std::endl << "Loop start          : " << (unsigned)itNewInstHeader.pitchEnvelope.loopStart
        << std::endl << "Loop end            : " << (unsigned)itNewInstHeader.pitchEnvelope.loopEnd
        << std::endl << "Sustain loop start  : " << (unsigned)itNewInstHeader.pitchEnvelope.sustainLoopStart
        << std::endl << "Sustain loop end    : " << (unsigned)itNewInstHeader.pitchEnvelope.sustainLoopEnd
        << std::endl << "Envelope node points: " << std::endl << std::dec;
    for ( int i = 0; i < 25; i++ )
        std::cout
        << std::setw( 2 ) << (unsigned)itNewInstHeader.pitchEnvelope.nodes[i].magnitude << ":"
        << std::setw( 4 ) << (unsigned)itNewInstHeader.pitchEnvelope.nodes[i].tickIndex << " ";
    std::cout << std::endl << std::endl;

}

void ItDebugShow::sampleHeader( ItSampleHeader& itSampleHeader )
{
    bool    is16bitData = (itSampleHeader.flag & IT_SMP_IS_16_BIT) != 0;
    bool    isCompressed = (itSampleHeader.flag & IT_SMP_IS_COMPRESSED) != 0;
    bool    isStereoSample = (itSampleHeader.flag & IT_SMP_IS_STEREO) != 0;
    std::cout
        << std::endl << "Tag                 : " << itSampleHeader.tag[0]
        << itSampleHeader.tag[1] << itSampleHeader.tag[2] << itSampleHeader.tag[3]
        << std::endl << "Dos filename        : " << itSampleHeader.fileName
        << std::endl << "Global volume       : " << (unsigned)itSampleHeader.globalVolume
        << std::endl << "Flags               : " << (unsigned)itSampleHeader.flag
        << std::endl << "Associated w/ header: " << ((itSampleHeader.flag & IT_SMP_ASSOCIATED_WITH_HEADER) ? "yes" : "no")
        << std::endl << "16 bit sample       : " << (is16bitData ? "yes" : "no")
        << std::endl << "Stereo sample       : " << (isStereoSample ? "yes" : "no")
        << std::endl << "Compressed sample   : " << (isCompressed ? "yes" : "no")
        << std::endl << "Loop                : " << ((itSampleHeader.flag & IT_SMP_LOOP_ON) ? "on" : "off")
        << std::endl << "Sustain loop        : " << ((itSampleHeader.flag & IT_SMP_SUSTAIN_LOOP_ON) ? "on" : "off")
        << std::endl << "Pingpong loop       : " << ((itSampleHeader.flag & IT_SMP_PINGPONG_LOOP_ON) ? "on" : "off")
        << std::endl << "Pingpong sustain    : " << ((itSampleHeader.flag & IT_SMP_PINGPONG_SUSTAIN_LOOP_ON) ? "on" : "off")
        << std::endl << "Volume              : " << (unsigned)itSampleHeader.volume
        << std::endl << "Name                : " << itSampleHeader.name
        << std::endl << "Convert (1 = signed): " << (unsigned)itSampleHeader.convert        // bit0 set: signed smp data (off = unsigned)
        << std::endl << "Default Panning     : " << (unsigned)itSampleHeader.defaultPanning // 0..64, bit 7 set == use default panning
        << std::endl << "Length              : " << itSampleHeader.length
        << std::endl << "Loop start          : " << itSampleHeader.loopStart
        << std::endl << "Loop end            : " << itSampleHeader.loopEnd << std::hex
        << std::endl << "C5 speed            : " << itSampleHeader.c5Speed << std::dec
        << std::endl << "Sustain loop start  : " << itSampleHeader.sustainLoopStart
        << std::endl << "Sustain loop end    : " << itSampleHeader.sustainLoopEnd << std::hex
        << std::endl << "Sample data pointer : " << itSampleHeader.samplePointer << std::dec
        << std::endl << "Vibrato speed       : " << (unsigned)itSampleHeader.vibratoSpeed   // 0..64
        << std::endl << "Vibrato depth       : " << (unsigned)itSampleHeader.vibratoDepth   // 0..64
        << std::endl << "Vibrato wave form   : " << (unsigned)itSampleHeader.vibratoWaveForm// 0 = sine,1 = ramp down,2 = square,3 = random
        << std::endl << "Vibrato speed       : " << (unsigned)itSampleHeader.vibratoRate;   // 0..64
}

void ItDebugShow::pattern( Pattern& pattern )
{
#define IT_DEBUG_SHOW_MAX_CHN 13
    const int nChannels_ = 32; // TEMP DEBUG!
    for ( unsigned rowNr = 0; rowNr < pattern.getnRows(); rowNr++ )
    {
        std::cout << std::endl;
        for ( int channelNr = 0; channelNr < IT_DEBUG_SHOW_MAX_CHN; channelNr++ )
        {

            // **************************************************
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
            else std::cout << "  ";
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
