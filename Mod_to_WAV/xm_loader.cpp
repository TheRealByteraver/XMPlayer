/*
    XM files with ADPCM compressed sample data are not supported yet.
    Stripped XM files should load normally.

    Loader variable name conventions (should be same among all loaders):
    - Index in instrument loop: instrumentNr
    - Index in sample loop: sampleNr
    - Index in pattern loop: patternNr
    - short name for instrumentHeader: instHdr
    - short name for sampleHeader: smpHdr

*/
#include <climits>
#if CHAR_BIT != 8 
This code requires a byte to be 8 bits wide
#endif

#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>
#include <bitset>
#include <iomanip>
#include <vector>
#include <iterator>

#include "module.h"
#include "virtualfile.h"
                       

//#define debug_xm_show_patterns
//#define debug_xm_play_samples

const int XM_DEBUG_SHOW_PATTERN_NO      = 0;// pattern to be shown
const int XM_DEBUG_SHOW_MAX_CHN         = 16;

const int XM_HEADER_SIZE_PART_ONE       = 60;
const int XM_MAX_SONG_NAME_LENGTH       = 20;
const int XM_TRACKER_NAME_LENGTH        = 20;
const int XM_MAX_INSTRUMENT_NAME_LENGTH = 22;
const int XM_MAX_SAMPLE_NAME_LENGTH     = 22;
const int XM_MAX_SAMPLES_PER_INST       = 16;
const int XM_MAX_PATTERNS               = 256;
const int XM_MAX_INSTRUMENTS            = 128;
const int XM_MAXIMUM_NOTES              = 8 * 12;
const int XM_MAX_ENVELOPE_NODES         = 12;
const int XM_ENVELOPE_IS_ENABLED_FLAG   = 1;
const int XM_ENVELOPE_IS_SUSTAINED_FLAG = 2;
const int XM_ENVELOPE_IS_LOOPED_FLAG    = 4;
const int XM_MIN_CHANNELS               = 2;
const int XM_MAX_CHANNELS               = 32;
const int XM_MAX_SONG_LENGTH            = 256;
const int XM_LINEAR_FREQUENCIES_FLAG    = 1;
const int XM_MAX_BPM                    = 0xFF;
const int XM_MAX_TEMPO                  = 0x1F;
const int XM_MAX_PATTERN_ROWS           = 256;
const int XM_NOTE_IS_PACKED             = 128;
const int XM_NOTE_AVAIL                 = 1;
const int XM_INSTRUMENT_AVAIL           = 2;
const int XM_VOLUME_COLUMN_AVAIL        = 4;
const int XM_EFFECT_AVAIL               = 8;
const int XM_EFFECT_ARGUMENT_AVAIL      = 16;
const int XM_SAMPLE_LOOP_MASK           = 3;  // 011b
const int XM_PINGPONG_LOOP_FLAG         = 2;
const int XM_SIXTEEN_BIT_SAMPLE_FLAG    = 16;
const int XM_STANDARD_COMPRESSION       = 0;
const int XM_ADPCM_COMPRESSION          = 0xAD;
const int XM_KEY_OFF                    = 8 * 12 + 1; // 8 octaves, 1 based, plus 1

#pragma pack (1)

// the header of the xm file... global info.
struct XmHeader {    
    char              fileTag[17];                        // = "Extended Module"
    char              songTitle[XM_MAX_SONG_NAME_LENGTH]; // Name of the XM
    std::uint8_t      id;                                 // 0x1A
    char              trackerName[XM_TRACKER_NAME_LENGTH];// = "FastTracker v2.00"
    std::uint16_t     version;                            // 0x01 0x04
    std::uint16_t     headerSize;       // size of xmHeader from here: min. 20 + 1 bytes
    std::uint16_t     reserved; 
    std::uint16_t     songLength;
    std::uint16_t     songRestartPosition;
    std::uint16_t     nrChannels;
    std::uint16_t     nPatterns;
    std::uint16_t     nInstruments;
    std::uint16_t     flags;            // 0 = Amiga frequency table 1 = Linear frequency table
    std::uint16_t     defaultTempo;   
    std::uint16_t     defaultBpm;     
    std::uint8_t      patternTable[XM_MAX_SONG_LENGTH];
};                                 // sizeof == 336 bytes max (found in the wild)

struct XmPatternHeader {           // typedef of the pattern xmHeader
    std::uint16_t     headerSize;
    std::uint16_t     reserved;    // !!!
    std::uint8_t      pack;
    std::uint16_t     nRows;
    std::uint16_t     patternSize;
};                                 // sizeof == 9 bytes

struct XmInstrumentHeader1 {
    std::uint16_t     headerSize;  // size of the 2 headers
    std::uint16_t     reserved;    // !!!
    char              name[22];
    std::uint8_t      type;        // should be 0, but is sometimes 128,129,253
    std::uint16_t     nSamples;
};                                 // sizeof == 29 bytes

struct XmEnvelopeNode {
    std::uint16_t     x;
    std::uint16_t     y;
};
 
struct XmInstrumentHeader2 {
    std::uint16_t     sampleHeaderSize;
    std::uint16_t     reserved;            // !!!
    std::uint8_t      sampleForNote[XM_MAXIMUM_NOTES];
    XmEnvelopeNode    volumeEnvelope[XM_MAX_ENVELOPE_NODES];
    XmEnvelopeNode    panningEnvelope[XM_MAX_ENVELOPE_NODES];
    std::uint8_t      nrVolumeNodes;
    std::uint8_t      nrPanningNodes;
    std::uint8_t      volumeSustain;
    std::uint8_t      volumeLoopStart;
    std::uint8_t      volumeLoopEnd;
    std::uint8_t      panningSustain;
    std::uint8_t      panningLoopStart;
    std::uint8_t      panningLoopEnd;
    std::uint8_t      volumeType;
    std::uint8_t      panningType;
    std::uint8_t      vibratoType;
    std::uint8_t      vibratoSweep;
    std::uint8_t      vibratoDepth;
    std::uint8_t      vibratoRate;
    std::uint16_t     volumeFadeOut;
};                                  

struct XmSampleHeader {
  std::uint32_t       length;         // Sample length
  std::uint32_t       repeatOffset;   // Sample loop start
  std::uint32_t       repeatLength;   // Sample loop length
  std::uint8_t        volume;         // Volume
  std::int8_t         finetune;       // Finetune (signed BYTE -128..+127)
  std::uint8_t        type;           // 0=No loop 1=Forward loop 2=Ping-pong 4=16-bit sampledata
  std::uint8_t        panning;        // Panning (0-255)
  std::int8_t         relativeNote;   // Relative note number (signed BYTE)
  std::uint8_t        compression;    // Reserved
  char                name[22];       // Sample name
};                                    // size = 40

#pragma pack (8) 

// small class with functions that show debug information
class XmDebugShow {
public:
    static void fileHeader( XmHeader& xmHeader );
    static void instHeader1( XmInstrumentHeader1& xmInstrumentHeader1 );
    static void instHeader2( XmInstrumentHeader2& xmInstrumentHeader2 );
    static void sampleHeader( int sampleNr,SampleHeader& sampleHeader );
    static void patternHeader( int patternNr,XmPatternHeader& xmPatternHeader );
    static void pattern( Pattern & pattern,int nrChannels );
    static void illegalNote( int patternNr,int nRows,int idx,int note );
};

int Module::loadXmFile( VirtualFile& moduleFile )
{
    //unsigned fileSize = 0;
    //isLoaded_ = false;
    //VirtualFile xmFile( fileName_ );
    //if ( xmFile.getIOError() != VIRTFILE_NO_ERROR ) 
    //    return 0;
    moduleFile.absSeek( 0 );
    VirtualFile& xmFile = moduleFile;
    unsigned fileSize = xmFile.fileSize();

    XmHeader xmHeader;
    if ( xmFile.read( &xmHeader,sizeof( XmHeader ) ) ) 
        return 0;

    if ( showDebugInfo_ )
        XmDebugShow::fileHeader( xmHeader );

    // ultra simple error checking
    if (
        //(xmHeader.id != 0x1A                         ) ||  // removed for stripped xm compatibility
        //((xmHeader.version >> 8) != 1                ) ||  // removed for stripped xm compatibility
        (xmHeader.songLength     > XM_MAX_SONG_LENGTH) ||
        (xmHeader.nrChannels      > XM_MAX_CHANNELS   ) ||
        (xmHeader.nrChannels      < XM_MIN_CHANNELS   ) ||
        (xmHeader.nInstruments   > XM_MAX_INSTRUMENTS) ||
        (xmHeader.nPatterns      > XM_MAX_PATTERNS)    ||
        (xmHeader.defaultBpm     > XM_MAX_BPM)         ||
        (xmHeader.defaultTempo   > XM_MAX_TEMPO) ) {
        if ( showDebugInfo_ ) 
            std::cout << "\nError reading xmHeader, this is not an xm file.\n";
        return 0;
    }
    trackerType_ = TRACKER_FT2;
    songTitle_.assign( xmHeader.songTitle,XM_MAX_SONG_NAME_LENGTH );
    trackerTag_.assign( xmHeader.trackerName,XM_TRACKER_NAME_LENGTH );
    
    xmHeader.id             = 0; // use as zero terminator
    useLinearFrequencies_   = (bool)(xmHeader.flags & XM_LINEAR_FREQUENCIES_FLAG);
    isCustomRepeat_         = true;
    panningStyle_           = PANNING_STYLE_XM;
    nrChannels_             = xmHeader.nrChannels;
    nrInstruments_          = xmHeader.nInstruments;
    nrSamples_              = 0;
    nrPatterns_             = xmHeader.nPatterns;
    defaultTempo_           = xmHeader.defaultTempo;
    defaultBpm_             = xmHeader.defaultBpm;
    songLength_             = xmHeader.songLength;
    songRestartPosition_    = xmHeader.songRestartPosition;
    
    // initialize XM specific variables:
    if ( useLinearFrequencies_ ) {
        minPeriod_ = 1600;  
        maxPeriod_ = 7680;  
    } 
    else {
        minPeriod_ = 113;
        maxPeriod_ = 27392;
    }
    for ( unsigned i = 0; i < nrChannels_; i++ )
        defaultPanPositions_[i] = PANNING_CENTER;
    
    // reset entries in the order table to one above the highest pattern number.
    // the Module::getPattern() function returns an empty pattern if the pattern
    // does not exist, but this is an extra safety. Oh well.
    memset( patternTable_,0,sizeof( Module::patternTable_ ) );
    for ( unsigned i = 0; i < songLength_; i++ ) {
        unsigned k = xmHeader.patternTable[i];
        if ( k >= nrPatterns_ ) 
            k = nrPatterns_; 
        patternTable_[i] = k;
    }
    // start reading the patterns
    xmFile.absSeek( XM_HEADER_SIZE_PART_ONE + xmHeader.headerSize ); 
    for (unsigned patternNr = 0; patternNr < nrPatterns_; patternNr++) {
        if ( loadXmPattern( xmFile,patternNr ) )
            return 0;
#ifdef debug_xm_show_patterns
        if ( showDebugInfo_ && (patternNr == XM_DEBUG_SHOW_PATTERN_NO) )
            XmDebugShow::pattern( *(patterns_[patternNr]),nrChannels_ );
#endif
    }
    // Tell the envelopes which style they should follow:


    // Now read all the instruments & sample data
    for ( unsigned instrumentNr = 1; instrumentNr <= nrInstruments_; instrumentNr++ ) {
        if ( loadXmInstrument( xmFile,instrumentNr ) )
            return 0;
    }
    isLoaded_ = true;
    return 0;
}

int Module::loadXmInstrument( VirtualFile& xmFile,int instrumentNr )
{
    InstrumentHeader    instHdr;
    XmInstrumentHeader1 xmInstHdr1;
    XmInstrumentHeader2 xmInstHdr2;

    // read the instrument header and check if it contains any samples
    xmFile.read( &xmInstHdr1,sizeof( XmInstrumentHeader1 ) );
    if ( showDebugInfo_ ) {
        std::cout << "\n\nInstrument header " << instrumentNr << ":";
        XmDebugShow::instHeader1( xmInstHdr1 );
    }
    // Necessary for packed xm files:
    if ( xmInstHdr1.headerSize < sizeof( XmInstrumentHeader1 ) )
        xmInstHdr1.nSamples = 0;

    if ( xmInstHdr1.nSamples == 0 ) {
        if ( xmInstHdr1.headerSize >=
            sizeof( XmInstrumentHeader1 ) )
            instHdr.name.assign(
                xmInstHdr1.name,XM_MAX_INSTRUMENT_NAME_LENGTH );
        instHdr.nrSamples = 0;

        // Correct file pointer according to header size in file:
        xmFile.relSeek(
            (int)xmInstHdr1.headerSize -
            (int)sizeof( XmInstrumentHeader1 ) );
    } 
    else {
        instHdr.nrSamples = xmInstHdr1.nSamples;
        xmFile.read( &xmInstHdr2,sizeof( XmInstrumentHeader2 ) );

        // Set file pointer to correct position for sample reading
        xmFile.relSeek( 
            (int)xmInstHdr1.headerSize
            - (int)sizeof( XmInstrumentHeader1 ) 
            - (int)sizeof( XmInstrumentHeader2 ) );

        if ( instHdr.nrSamples > XM_MAX_SAMPLES_PER_INST ) {
            if ( showDebugInfo_ )
                std::cout
                    << "\nMore than " << XM_MAX_SAMPLES_PER_INST
                    << " (" << instHdr.nrSamples
                    << ") samples for this instHdr, exiting!"
                    << "\nSample Header Size       : "
                    << xmInstHdr2.sampleHeaderSize;
            return 0;
        }

        instHdr.name.assign(
            xmInstHdr1.name,XM_MAX_INSTRUMENT_NAME_LENGTH );

        // take care of envelopes. First, sanity checking:
        if ( xmInstHdr2.nrVolumeNodes > XM_MAX_ENVELOPE_NODES )
            xmInstHdr2.nrVolumeNodes = XM_MAX_ENVELOPE_NODES;
        if ( xmInstHdr2.nrPanningNodes > XM_MAX_ENVELOPE_NODES )
            xmInstHdr2.nrPanningNodes = XM_MAX_ENVELOPE_NODES;

        // tell the envelope functions we want XM style envelope processing:
        instHdr.volumeEnvelope.setEnvelopeStyle( xmEnvelopeStyle );

        // initialize envelope parameters. First the volume envelope:
        unsigned char flags = 0;
        if ( xmInstHdr2.volumeType & XM_ENVELOPE_IS_ENABLED_FLAG )
            flags |= ENVELOPE_IS_ENABLED_FLAG;
        if ( xmInstHdr2.volumeType & XM_ENVELOPE_IS_SUSTAINED_FLAG )
            flags |= ENVELOPE_IS_SUSTAINED_FLAG;
        if ( xmInstHdr2.volumeType & XM_ENVELOPE_IS_LOOPED_FLAG )
            flags |= ENVELOPE_IS_LOOPED_FLAG;
        instHdr.volumeEnvelope.setFlags( flags );

        // and now the panning envelope:
        flags = 0;
        if ( xmInstHdr2.panningType & XM_ENVELOPE_IS_ENABLED_FLAG )
            flags |= ENVELOPE_IS_ENABLED_FLAG;
        if ( xmInstHdr2.panningType & XM_ENVELOPE_IS_SUSTAINED_FLAG )
            flags |= ENVELOPE_IS_SUSTAINED_FLAG;
        if ( xmInstHdr2.panningType & XM_ENVELOPE_IS_LOOPED_FLAG )
            flags |= ENVELOPE_IS_LOOPED_FLAG;
        instHdr.panningEnvelope.setFlags( flags );

        // copy volume envelope:
        for ( unsigned i = 0; i < xmInstHdr2.nrVolumeNodes; i++ ) {
            instHdr.volumeEnvelope.nodes[i].x  = xmInstHdr2.volumeEnvelope[i].x;
            instHdr.volumeEnvelope.nodes[i].y  =
                (unsigned char)xmInstHdr2.volumeEnvelope[i].y;
        }
        instHdr.volumeEnvelope.nrNodes = xmInstHdr2.nrVolumeNodes;
        instHdr.volumeEnvelope.sustainStart = 
            instHdr.volumeEnvelope.sustainEnd = xmInstHdr2.volumeSustain;
        instHdr.volumeEnvelope.loopStart = xmInstHdr2.volumeLoopStart;
        instHdr.volumeEnvelope.loopEnd = xmInstHdr2.volumeLoopEnd;

        // safety check: no sustain if after loop end
        if ( instHdr.volumeEnvelope.isLooped() && 
            (instHdr.volumeEnvelope.sustainStart > instHdr.volumeEnvelope.loopEnd) )
            instHdr.volumeEnvelope.disableSustain();

        // copy panning envelope:
        for ( unsigned i = 0; i < xmInstHdr2.nrPanningNodes; i++ ) {
            instHdr.panningEnvelope.nodes[i].x = xmInstHdr2.panningEnvelope[i].x;
            instHdr.panningEnvelope.nodes[i].y =
                (unsigned char)xmInstHdr2.panningEnvelope[i].y;
        }
        instHdr.panningEnvelope.nrNodes = xmInstHdr2.nrPanningNodes;
        instHdr.panningEnvelope.sustainStart = 
            instHdr.panningEnvelope.sustainEnd = xmInstHdr2.panningSustain;
        instHdr.panningEnvelope.loopStart = xmInstHdr2.panningLoopStart;
        instHdr.panningEnvelope.loopEnd = xmInstHdr2.panningLoopEnd;

        // safety check: no sustain if after loop end
        if ( instHdr.panningEnvelope.isLooped() &&
            (instHdr.panningEnvelope.sustainStart > instHdr.panningEnvelope.loopEnd) )
            instHdr.panningEnvelope.disableSustain();

        // copy some more miscellaneous parameters:
        instHdr.volumeFadeOut = xmInstHdr2.volumeFadeOut;
        instHdr.vibratoConfig.type = xmInstHdr2.vibratoType;
        instHdr.vibratoConfig.sweep = xmInstHdr2.vibratoSweep;
        instHdr.vibratoConfig.depth = xmInstHdr2.vibratoDepth;
        instHdr.vibratoConfig.rate = xmInstHdr2.vibratoRate;

        if ( showDebugInfo_ )
            XmDebugShow::instHeader2( xmInstHdr2 );
    } // done reading instHdr, except for sample-for-note table

    if ( instHdr.nrSamples ) {
        SampleHeader    smpHdr[XM_MAX_SAMPLES_PER_INST];
        char            sampleNames[XM_MAX_SAMPLES_PER_INST][XM_MAX_SAMPLE_NAME_LENGTH + 1];

        // start reading sample headers:
        for ( unsigned sampleNr = 0; sampleNr < instHdr.nrSamples; sampleNr++ ) {
            XmSampleHeader  xmSampleHeader;
            xmFile.read( &xmSampleHeader,sizeof( XmSampleHeader ) );

            xmFile.relSeek(
                (int)xmInstHdr2.sampleHeaderSize
                - (int)sizeof( XmSampleHeader ) );

            if ( xmInstHdr2.sampleHeaderSize < sizeof( XmSampleHeader ) )
                sampleNames[sampleNr][0] = '\0';
            else {
                for ( unsigned i = 0; i < XM_MAX_SAMPLE_NAME_LENGTH; i++ )
                    sampleNames[sampleNr][i] = xmSampleHeader.name[i];
                sampleNames[sampleNr][XM_MAX_SAMPLE_NAME_LENGTH] = '\0';
            }
            smpHdr[sampleNr].name           = sampleNames[sampleNr];
            smpHdr[sampleNr].finetune       = xmSampleHeader.finetune;
            smpHdr[sampleNr].length         = xmSampleHeader.length;
            smpHdr[sampleNr].repeatLength   = xmSampleHeader.repeatLength;
            smpHdr[sampleNr].repeatOffset   = xmSampleHeader.repeatOffset;
            smpHdr[sampleNr].isRepeatSample = 
                (smpHdr[sampleNr].repeatLength > 0) &&
                (xmSampleHeader.type & XM_SAMPLE_LOOP_MASK);
            smpHdr[sampleNr].volume         = xmSampleHeader.volume;
            smpHdr[sampleNr].relativeNote   = xmSampleHeader.relativeNote;
            smpHdr[sampleNr].panning        = xmSampleHeader.panning;

            //smpHdr[sampleNr].dataType       =
            //    xmSampleHeader.type & XM_SIXTEEN_BIT_SAMPLE_FLAG ?
            //    SAMPLEDATA_SIGNED_16BIT : SAMPLEDATA_SIGNED_8BIT;

            smpHdr[sampleNr].dataType = SAMPLEDATA_IS_SIGNED_FLAG;
            if ( xmSampleHeader.type & XM_SIXTEEN_BIT_SAMPLE_FLAG )
                smpHdr[sampleNr].dataType |= SAMPLEDATA_IS_16BIT_FLAG;


            smpHdr[sampleNr].isPingpongSample =
                xmSampleHeader.type & XM_PINGPONG_LOOP_FLAG ? true : false;
            if ( xmSampleHeader.compression == XM_ADPCM_COMPRESSION ) {
                if ( showDebugInfo_ )
                    std::cout
                        << "\n\nADPCM compressed sample data"
                        << "is not supported yet!\n";
                return -1;
            }
            if ( showDebugInfo_ )
                XmDebugShow::sampleHeader( sampleNr,smpHdr[sampleNr] );
        } // finished reading sample headers

        // remap the sample-for-note configuration of the instrument header
        for ( int i = 0; i < MAXIMUM_NOTES;i++ )
            instHdr.sampleForNote[i].note = i;

        int sampleMap[XM_MAX_SAMPLES_PER_INST];
        int smpCnt = 0;
        for ( unsigned i = 0; i < instHdr.nrSamples; i++ ) {
            if ( smpHdr[i].length > 0 ) {
                smpCnt++;
                sampleMap[i] = smpCnt;
            } 
            else
                sampleMap[i] = 0;
        }
        if ( showDebugInfo_ )
            std::cout << "\nSample for note table: \n";
        for ( unsigned i = 0; i < XM_MAXIMUM_NOTES; i++ ) {
            unsigned smpNr = xmInstHdr2.sampleForNote[i];
            if ( smpNr > instHdr.nrSamples ) {
                if ( showDebugInfo_ )
                    std::cout
                        << "\nIllegal sample nr in"
                        << " sample to note table, exiting!";
                return -1;
            }
            if ( sampleMap[smpNr] > 0 )
                instHdr.sampleForNote[i].sampleNr = 
                    nrSamples_ + sampleMap[smpNr];
            else
                instHdr.sampleForNote[i].sampleNr = 0;
            if ( showDebugInfo_ )
                std::cout 
                    << std::setw( 4 ) 
                    << instHdr.sampleForNote[i].sampleNr;
        }

        // should be unnecessary:
        for ( unsigned i = XM_MAXIMUM_NOTES; i < MAXIMUM_NOTES; i++ ) {
            //instHdr.sampleForNote[i].note = i;       // ?
            instHdr.sampleForNote[i].sampleNr = 0;   // ?
        }

        // load sample data of this instruments' sample Header:
        for ( unsigned sampleNr = 0;
            sampleNr < instHdr.nrSamples;
            sampleNr++ ) {
            if ( smpHdr[sampleNr].length ) {
                nrSamples_++;
                loadXmSample( xmFile,nrSamples_,smpHdr[sampleNr] );
                if ( showDebugInfo_ ) {
#ifdef debug_xm_play_samples
                    playSampleNr( nSamples_ );
#endif
                }
            }
        }
    }
    instruments_[instrumentNr] = std::make_unique < Instrument >( instHdr );
    return 0;
}

int Module::loadXmSample( VirtualFile& xmFile,int sampleNr,SampleHeader& smpHdr )
{
    std::int16_t           oldSample16 = 0;
    std::int16_t           newSample16 = 0;
    signed char     oldSample8 = 0;
    signed char     newSample8 = 0;

    smpHdr.data = (std::int16_t *)xmFile.getSafePointer( smpHdr.length );
    xmFile.relSeek( smpHdr.length );

    if ( smpHdr.data == nullptr ) { // temp DEBUG:
        if ( showDebugInfo_ )
            std::cout
            << "\nCan't get safe sample pointer, exiting!";
        return 0;
    }
    // decode delta encoded sample data
    if ( smpHdr.dataType & SAMPLEDATA_IS_16BIT_FLAG ) {
        std::int16_t   *ps = (std::int16_t *)smpHdr.data;
        std::int16_t   *pd = ps;
        smpHdr.length >>= 1;
        smpHdr.repeatLength >>= 1;
        smpHdr.repeatOffset >>= 1;
        for ( unsigned iData = 0; iData < smpHdr.length; iData++ ) {
            newSample16 = *ps++ + oldSample16;
            *pd++ = newSample16;
            oldSample16 = newSample16;
        }
    } 
    else {
        signed char *ps = (signed char *)smpHdr.data;
        signed char *pd = ps;
        for ( unsigned iData = 0; iData < smpHdr.length; iData++ ) {
            newSample8 = *ps++ + oldSample8;
            *pd++ = newSample8;
            oldSample8 = newSample8;
        }
    }
    samples_[sampleNr] = std::make_unique<Sample>( smpHdr );
    return 0;
}

// Pattern decoder helper functions
void decodeXmVolumeColumn( Effect& target,unsigned char volc )
{
    target.effect = NO_EFFECT;
    target.argument = 0;
    if ( !volc ) 
        return;    // nothing to do
    if ( volc > 0xF0 )
        target.effect = TONE_PORTAMENTO;
    else if ( volc > 0xE0 )
        target.effect = PANNING_SLIDE;
    else if ( volc > 0xD0 )
        target.effect = PANNING_SLIDE;
    else if ( volc > 0xC0 )
        target.effect = SET_FINE_PANNING;
    else if ( volc > 0xB0 )
        target.effect = VIBRATO;
    else if ( volc > 0xA0 )
        target.effect = SET_VIBRATO_SPEED;
    else if ( volc > 0x90 )
        target.effect = EXTENDED_EFFECTS; //FINE_VOLUME_SLIDE_UP
    else if ( volc > 0x80 )
        target.effect = EXTENDED_EFFECTS; //FINE_VOLUME_SLIDE_DOWN
    else if ( volc > 0x70 )
        target.effect = VOLUME_SLIDE;
    else if ( volc > 0x60 )
        target.effect = VOLUME_SLIDE;
    else
        target.effect = SET_VOLUME;

    if ( target.effect == SET_VOLUME )
        target.argument = volc - 0x10;
    else
        target.argument = volc & 0xF;

    switch ( target.effect ) {
        case TONE_PORTAMENTO:
        {
            target.argument <<= 4;
            /*
            if ( iNote->effects[1].effect == NOTE_DELAY ) {// emulate FT2 bug
                target.effect = NO_EFFECT;
                target.argument = NO_EFFECT;
            }
            */
            break;
        }
        case EXTENDED_EFFECTS:
        {
            if ( volc > 0x80 )
                target.argument += (FINE_VOLUME_SLIDE_DOWN << 4);
            else
                target.argument += (FINE_VOLUME_SLIDE_UP << 4);
            break;
        }
        case PANNING_SLIDE:
        {
            if ( volc > 0xE0 ) // slide right
                target.argument <<= 4;
            break;
        }
        case SET_FINE_PANNING: // rough panning, really
        {
            target.argument <<= 4;
            break;
        }
        case SET_VIBRATO_SPEED:
        {
            target.argument <<= 4;
            break;
        }
        /*
        case VIBRATO :
        {
            if ( volc < 0xB0 ) {
                target.effect = SET_VIBRATO_SPEED;
                target.argument <<= 4;
            }
            break;
        }
        */
        case VOLUME_SLIDE:
        {
            if ( volc > 0x70 ) // slide up
                target.argument <<= 4;
            break;
        }
    }
}

void remapXmEffects( Effect& remapFx )
{
    // do some error checking & effect remapping:
    switch ( remapFx.effect ) {
        case 0: // arpeggio / no effect
        {
            if ( remapFx.argument )
                remapFx.effect = ARPEGGIO;
            break;
        }
        case TONE_PORTAMENTO_AND_VOLUME_SLIDE:
        case VIBRATO_AND_VOLUME_SLIDE:
        case VOLUME_SLIDE:
        {
            // in .mod & .xm files volume slide up has
            // priority over volume slide down
            unsigned char& argument = remapFx.argument;
            unsigned volUp = argument & 0xF0;
            unsigned volDn = argument & 0x0F;
            if ( volUp && volDn )
                argument = volUp;
            break;
        }
        case SET_GLOBAL_VOLUME:
        // scale up from 0..64 to 0..128 for .IT compatibility
        {
            remapFx.argument *= 2;
            if ( remapFx.argument > MAX_GLOBAL_VOLUME )
                remapFx.argument = MAX_GLOBAL_VOLUME;
            break;
        }
        case SET_VOLUME:
        {
            if ( remapFx.argument > MAX_VOLUME )
                remapFx.argument = MAX_VOLUME;
            break;
        }
        case SET_TEMPO:
        {
            if ( !remapFx.argument )
                remapFx.effect = NO_EFFECT;
            else
                if ( remapFx.argument > 0x1F )
                    remapFx.effect = SET_BPM;
            break;
        }
    }
}

int Module::loadXmPattern( VirtualFile& xmFile,int patternNr )
{
    XmPatternHeader xmPtnHdr;

    xmFile.read( &xmPtnHdr,sizeof( XmPatternHeader ) );
    xmFile.relSeek( (int)xmPtnHdr.headerSize - (int)sizeof( XmPatternHeader ) );
    if ( xmFile.getIOError() != VIRTFILE_NO_ERROR )
        return -1;
    if ( showDebugInfo_ )         
        XmDebugShow::patternHeader( patternNr,xmPtnHdr );
    
    if ( xmPtnHdr.nRows > XM_MAX_PATTERN_ROWS )
        return  -1;   

    std::vector<Note> patternData( nrChannels_ * xmPtnHdr.nRows );
    std::vector<Note>::iterator iNote = patternData.begin();

    // empty patterns are not stored
    if ( !xmPtnHdr.patternSize ) 
        return 0;

    for ( unsigned n = 0; n < (nrChannels_ * xmPtnHdr.nRows); n++ ) {
        unsigned char pack;
        unsigned char volumeColumn;
        if ( xmFile.read( &pack,sizeof( unsigned char ) ) )
            return -1;

        if ( pack & XM_NOTE_IS_PACKED ) {
            if ( pack & XM_NOTE_AVAIL ) {
                unsigned char note;
                if ( xmFile.read( &note,sizeof( unsigned char ) ) )
                    return -1;
                iNote->note = note;
                if ( iNote->note > XM_KEY_OFF ) {
                    if ( showDebugInfo_ )
                        XmDebugShow::illegalNote(
                            patternNr,xmPtnHdr.nRows,n,iNote->note );
                    iNote->note = 0;
                }
            }
            if ( pack & XM_INSTRUMENT_AVAIL ) {
                unsigned char instrument;
                if ( xmFile.read( &instrument,sizeof( unsigned char ) ) )
                    return -1;
                iNote->instrument = instrument;
            }
            if ( pack & XM_VOLUME_COLUMN_AVAIL ) {
                if ( xmFile.read( &volumeColumn,sizeof( unsigned char ) ) )
                    return -1;
            } else volumeColumn = 0;

            if ( pack & XM_EFFECT_AVAIL ) {
                unsigned char effect;
                if ( xmFile.read( &effect,sizeof( unsigned char ) ) )
                    return -1;
                iNote->effects[1].effect = effect;
            }
            if ( pack & XM_EFFECT_ARGUMENT_AVAIL ) {
                unsigned char argument;
                if ( xmFile.read( &argument,sizeof( unsigned char ) ) )
                    return -1;
                iNote->effects[1].argument = argument;
            }
        } else {
            iNote->note = pack;
            if ( showDebugInfo_ ) {
                if ( iNote->note > XM_KEY_OFF ) {
                    if ( showDebugInfo_ ) 
                        XmDebugShow::illegalNote( 
                            patternNr,xmPtnHdr.nRows,n,iNote->note );
                    iNote->note = 0;
                }
            }
            unsigned char instrument;
            unsigned char effect;
            unsigned char argument;

            if ( xmFile.read( &instrument,sizeof( unsigned char ) ) )
                return -1;
            if ( xmFile.read( &volumeColumn,sizeof( unsigned char ) ) )
                return -1;
            if ( xmFile.read( &effect,sizeof( unsigned char ) ) )
                return -1;
            if ( xmFile.read( &argument,sizeof( unsigned char ) ) )
                return -1;
            iNote->instrument = instrument;
            iNote->effects[1].effect = effect;
            iNote->effects[1].argument = argument;
        }
        if ( iNote->note == XM_KEY_OFF )
            iNote->note = KEY_OFF;

        decodeXmVolumeColumn( iNote->effects[0],volumeColumn );

        // emulate FT2 bug:
        if ( iNote->effects[1].effect == NOTE_DELAY ) {
            iNote->effects[0].effect = NO_EFFECT;
            iNote->effects[0].argument = 0;
        }

        remapXmEffects( iNote->effects[1] );
        iNote++;
    }
    patterns_[patternNr] = std::make_unique < Pattern >
        ( nrChannels_,xmPtnHdr.nRows,patternData );
    return 0;
}

// DEBUG helper functions that write verbose output to the screen:

void XmDebugShow::illegalNote( int patternNr,int nRows,int idx,int note )
{
    std::cout
        << "\nIllegal note in pattern nr " << patternNr
        << ", row " << (idx / nRows)
        << ", column " << (idx % nRows)
        << ": " << note << "\n";
    _getch();
}

void XmDebugShow::fileHeader( XmHeader& xmHeader )
{
    std::string fileTag;
    std::string songTitle;
    std::string trackerName;
    fileTag.assign( xmHeader.fileTag,17 );
    songTitle.assign( xmHeader.songTitle,XM_MAX_SONG_NAME_LENGTH );
    trackerName.assign( xmHeader.trackerName,XM_TRACKER_NAME_LENGTH );

    std::cout
        << "\nXM Module tag            : " << fileTag
        << "\nXM Module title          : " << songTitle
        << "\nXM Module tracker name   : " << trackerName
        << std::hex
        << "\nXM file version          : " << xmHeader.version << std::dec
        << "\nNr of channels           : " << xmHeader.nrChannels
        << "\nNr of instruments        : " << xmHeader.nInstruments
        << "\nNr of patterns           : " << xmHeader.nPatterns
        << "\nDefault Tempo            : " << xmHeader.defaultTempo
        << "\nDefault Bpm              : " << xmHeader.defaultBpm
        << "\nSong length              : " << xmHeader.songLength
        << "\nSong restart position    : " << xmHeader.songRestartPosition
        << "\nHeader size              : " << xmHeader.headerSize
        << "\nFrequency / period system: "
        << ((xmHeader.flags & XM_LINEAR_FREQUENCIES_FLAG) ? "Linear" : "Amiga");
    /*
    std::cout << "\nSize of XmHeader                     = " << sizeof( XmHeader );
    std::cout << "\nSize of XmPatternHeader              = " << sizeof( XmPatternHeader );
    std::cout << "\nSize of XmInstrumentHeader1          = " << sizeof( XmInstrumentHeader1 );
    std::cout << "\nSize of XmEnvelopePoint              = " << sizeof( XmEnvelopePoint );
    std::cout << "\nSize of XmInstrumentHeader2          = " << sizeof( XmInstrumentHeader2 );
    std::cout << "\nSize of XmSampleHeader               = " << sizeof( XmSampleHeader ) << "\n";
    */
}

void XmDebugShow::instHeader1( XmInstrumentHeader1& xmInstHdr1 )
{
    std::cout
        << "\nHdr size in c++  : " << sizeof( XmInstrumentHeader1 )
        << "\nHeader size      : " << xmInstHdr1.headerSize
        << "\nHeader size (res): " << xmInstHdr1.reserved
        << "\nInstr. type (0)  : " << (unsigned)xmInstHdr1.type
        << "\nNr of Samples    : " << xmInstHdr1.nSamples;
}

void XmDebugShow::instHeader2( XmInstrumentHeader2& xmInstHdr2 )
{
    std::cout
        << "\nSample hdr size  : " << xmInstHdr2.sampleHeaderSize
        << "\nSmp hdr sz. (res): " << xmInstHdr2.reserved
       
        << "\n# vol. env. pts  : " << (unsigned)xmInstHdr2.nrVolumeNodes
        << "\nvol. loop start @: " << (unsigned)xmInstHdr2.volumeLoopStart
        << "\nvol. loop End @  : " << (unsigned)xmInstHdr2.volumeLoopEnd
        << "\nvol. sustain @   : " << (unsigned)xmInstHdr2.volumeSustain
        << "\nvol. env. enabled: " 
        << ((xmInstHdr2.volumeType & XM_ENVELOPE_IS_ENABLED_FLAG) ? "yes" : "no")
        << "\nvol. env. sustain: "
        << ((xmInstHdr2.volumeType & XM_ENVELOPE_IS_SUSTAINED_FLAG) ? "yes" : "no")
        << "\nvol. env. looped : "
        << ((xmInstHdr2.volumeType & XM_ENVELOPE_IS_LOOPED_FLAG) ? "yes" : "no")
        << "\n# panning Pts    : " << (unsigned)xmInstHdr2.nrPanningNodes
        << "\npanning sustain @: " << (unsigned)xmInstHdr2.panningSustain
        << "\npan. loop Start @: " << (unsigned)xmInstHdr2.panningLoopStart
        << "\npan. loop End @  : " << (unsigned)xmInstHdr2.panningLoopEnd
        << "\npan. env. enabled: "
        << ((xmInstHdr2.panningType & XM_ENVELOPE_IS_ENABLED_FLAG) ? "yes" : "no")
        << "\npan. env. sustain: "
        << ((xmInstHdr2.panningType & XM_ENVELOPE_IS_SUSTAINED_FLAG) ? "yes" : "no")
        << "\npan. env. looped : "
        << ((xmInstHdr2.panningType & XM_ENVELOPE_IS_LOOPED_FLAG) ? "yes" : "no")
        << "\nvolumeFadeOut    : " << xmInstHdr2.volumeFadeOut
        << "\nvibratoType      : " << (unsigned)xmInstHdr2.vibratoType
        << "\nvibratoDepth     : " << (unsigned)xmInstHdr2.vibratoDepth
        << "\nvibratoRate      : " << (unsigned)xmInstHdr2.vibratoRate
        << "\nvibratoSweep     : " << (unsigned)xmInstHdr2.vibratoSweep;

    if ( xmInstHdr2.volumeType & XM_ENVELOPE_IS_ENABLED_FLAG ) {
        std::cout
            << "\nVolume envelope nodes: ";
        for ( int i = 0; i < xmInstHdr2.nrVolumeNodes; i++ )
            std::cout << std::dec
            << xmInstHdr2.volumeEnvelope[i].x
            << ","
            << xmInstHdr2.volumeEnvelope[i].y
            << " ";
    }
    if ( xmInstHdr2.panningType & XM_ENVELOPE_IS_ENABLED_FLAG ) {
        std::cout
            << "\nPanning envelope nodes: ";
        for ( int i = 0; i < xmInstHdr2.nrPanningNodes; i++ )
            std::cout << std::dec
            << xmInstHdr2.panningEnvelope[i].x
            << ","
            << xmInstHdr2.panningEnvelope[i].y
            << " ";
    }
}

void XmDebugShow::sampleHeader( int sampleNr,SampleHeader& smpHdr )
{
    std::cout
        << "\n\n    Sample # " << sampleNr << ":"
        << "\n    Name             : " << smpHdr.name
        << "\n    Finetune         : " << smpHdr.finetune
        << "\n    Length           : " << smpHdr.length
        << "\n    RepeatLength     : " << smpHdr.repeatLength
        << "\n    RepeatOffset     : " << smpHdr.repeatOffset
        << "\n    RepeatSample     : " << smpHdr.isRepeatSample
        << "\n    Volume           : " << smpHdr.volume
        << "\n    Relative Note    : " << smpHdr.relativeNote
        << "\n    Panning          : " << smpHdr.panning
        << "\n    16 bit sample    : "
        << (((smpHdr.dataType & SAMPLEDATA_IS_16BIT_FLAG) != 0) ? "Yes" : "No")
        << "\n    Ping Loop active : " 
        << (smpHdr.isPingpongSample ? "Yes" : "No");
}

void XmDebugShow::patternHeader( int patternNr,XmPatternHeader& xmPtnHdr )
{
    std::cout 
        << "\nPattern nr " << patternNr << " header:"
        << "\nPattern Header Size (9) : " << xmPtnHdr.headerSize
        << "\nPattern # Rows          : " << xmPtnHdr.nRows
        << "\nPattern Pack system     : " << (unsigned)xmPtnHdr.pack
        << "\nPattern Data Size       : " << xmPtnHdr.patternSize;
}

void XmDebugShow::pattern( Pattern& pattern,int nrChannels )
{
    for ( unsigned rowNr = 0; rowNr < pattern.getnRows(); rowNr++ ) {                     
        const Note *iNote = pattern.getRow( rowNr );
        for ( int chn = 0; chn < nrChannels; chn++ ) {
            if ( chn == 0 ) {
                std::cout
                    << "\n" << std::hex << std::setw( 2 ) << rowNr << "/"
                    << std::setw( 2 ) << pattern.getnRows() << "|" << std::dec;
            } 
            if ( chn < XM_DEBUG_SHOW_MAX_CHN ) {
                if ( iNote->note < XM_MAXIMUM_NOTES + 2 )
                    std::cout << noteStrings[iNote->note] << "|";
                else if ( iNote->note == KEY_OFF )
                    std::cout << "===|";
                // show instrument nr:
                // show volume column:
                // show effect & argument:
            } 
        }
    }
}



