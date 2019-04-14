/*
    XM files with ADPCM compressed sample data are not supported yet.
    Stripped XM files should load normally.

*/
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>

#include "module.h"
#include "virtualfile.h"
                       
#define debug_xm_loader
#define debug_xm_show_patterns
#define debug_xm_play_samples

#define XM_DEBUG_SHOW_PATTERN_NO        0 // pattern to be shown
#define XM_DEBUG_SHOW_MAX_CHN           16

#ifdef debug_xm_loader
#include <bitset>
#include <iomanip>
#endif

#ifdef debug_xm_loader
extern const char *noteStrings[2 + MAXIMUM_NOTES];
#endif

#define XM_HEADER_SIZE_PART_ONE         60
#define XM_MAX_SONG_NAME_LENGTH         20
#define XM_TRACKER_NAME_LENGTH          20
#define XM_MAX_INSTRUMENT_NAME_LENGTH   22
#define XM_MAX_SAMPLE_NAME_LENGTH       22
#define XM_MAX_SAMPLES_PER_INST         16
#define XM_MAX_PATTERNS                 256
#define XM_MAX_INSTRUMENTS              128
#define XM_MAXIMUM_NOTES                (8 * 12)
#define XM_MAX_ENVELOPE_POINTS          12
#define XM_MIN_CHANNELS                 2
#define XM_MAX_CHANNELS                 32
#define XM_MAX_SONG_LENGTH              256
#define XM_MAX_CHANNELS                 32
#define XM_LINEAR_FREQUENCIES_FLAG      1
#define XM_MAX_BPM                      0xFF
#define XM_MAX_TEMPO                    0x1F
#define XM_MAX_PATTERN_ROWS             256
#define XM_NOTE_IS_PACKED               128
#define XM_NOTE_AVAIL                   1
#define XM_INSTRUMENT_AVAIL             2
#define XM_VOLUME_COLUMN_AVAIL          4
#define XM_EFFECT_AVAIL                 8
#define XM_EFFECT_ARGUMENT_AVAIL        16
#define XM_SAMPLE_LOOP_MASK             3   // 011b
#define XM_PINGPONG_LOOP_FLAG           2
#define XM_SIXTEEN_BIT_SAMPLE_FLAG      16
#define XM_STANDARD_COMPRESSION         0
#define XM_ADPCM_COMPRESSION            0xAD
#define XM_KEY_OFF                      97  // 8 octaves, 1 based, plus 1

#pragma pack (1)

struct XmHeader {                   // the xmHeader of the xm file... global info.
    char              fileTag[17];    // = "Extended Module"
    char              songTitle[XM_MAX_SONG_NAME_LENGTH];  // Name of the XM
    unsigned char     id;             // 0x1A
    char              trackerName[XM_TRACKER_NAME_LENGTH];// = "FastTracker v2.00"
    unsigned short    version;        // 0x01 0x04
    unsigned short    headerSize;     // size of xmHeader from here: min. 20 + 1 bytes
    unsigned short    reserved; 
    unsigned short    songLength;
    unsigned short    songRestartPosition;
    unsigned short    nChannels;
    unsigned short    nPatterns;
    unsigned short    nInstruments;
    unsigned short    flags;       // 0 = Amiga frequency table 1 = Linear frequency table
    unsigned short    defaultTempo;   
    unsigned short    defaultBpm;     
    unsigned char     patternTable[XM_MAX_SONG_LENGTH];
};                                 // sizeof == 336 bytes max (found in the wild)

struct XmPatternHeader {           // typedef of the pattern xmHeader
    unsigned short    headerSize;
    unsigned short    reserved;    // !!!
    unsigned char     pack;
    unsigned short    nRows;
    unsigned short    patternSize;
};                                 // sizeof == 9 bytes

struct XmInstrumentHeader1 {
    unsigned short    headerSize;  // size of the 2 headers
    unsigned short    reserved;    // !!!
    char              name[22];
    unsigned char     type;        // should be 0, but is sometimes 128,129,253
    unsigned short    nSamples;
};                                 // sizeof == 29 bytes

struct XmEnvelopePoint {
    unsigned short  x;
    unsigned short  y;
};
 
struct XmInstrumentHeader2 {
    unsigned short    sampleHeaderSize;
    unsigned short    reserved;            // !!!
    unsigned char     sampleForNote[XM_MAXIMUM_NOTES];
    XmEnvelopePoint   volumeEnvelope[12]; 
    XmEnvelopePoint   panningEnvelope[12];
    unsigned char     nVolumePoints;
    unsigned char     nPanningPoints;
    unsigned char     volumeSustain;
    unsigned char     volumeLoopStart;
    unsigned char     volumeLoopEnd;
    unsigned char     panningSustain;
    unsigned char     panningLoopStart;
    unsigned char     panningLoopEnd;
    unsigned char     volumeType;
    unsigned char     panningType;
    unsigned char     vibratoType;
    unsigned char     vibratoSweep;
    unsigned char     vibratoDepth;
    unsigned char     vibratoRate;
    unsigned short    volumeFadeOut;
};                                  

struct XmSampleHeader {
  unsigned          length;         // Sample length
  unsigned          repeatOffset;   // Sample loop start
  unsigned          repeatLength;   // Sample loop length
  unsigned char     volume;         // Volume
  signed char       finetune;       // Finetune (signed BYTE -128..+127)
  unsigned char     type;           // 0=No loop 1=Forward loop 2=Ping-pong 4=16-bit sampledata
  unsigned char     panning;        // Panning (0-255)
  signed char       relativeNote;   // Relative note number (signed BYTE)
  unsigned char     compression;    // Reserved
  char              name[22];       // Sample name
};                                  // size = 40

#pragma pack (8) 

// small class with functions that show debug information
class XmDebugShow {
public:
    static void fileHeader( XmHeader& xmHeader );
    static void instHeader1( XmInstrumentHeader1& xmInstrumentHeader1 );
    static void instHeader2( XmInstrumentHeader2& xmInstrumentHeader2 );
    static void sampleHeader( int sampleNr,SampleHeader & smpHdr );
    static void patternHeader( int patternNr,XmPatternHeader & xmPatternHeader );
    static void pattern( Pattern & pattern,int nChannels );
    static void illegalNote( int patternNr,int nRows,int idx,int note );
};

int Module::loadXmFile() 
{
    unsigned fileSize = 0;
    isLoaded_ = false;
    VirtualFile xmFile( fileName_ );
    if ( xmFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;
    fileSize = xmFile.fileSize();

    XmHeader xmHeader;
    if ( xmFile.read( &xmHeader,sizeof( XmHeader ) ) ) 
        return 0;
    // ultra simple error checking
    if (
        //(xmHeader.id != 0x1A                         ) ||  // removed for stripped xm compatibility
        //((xmHeader.version >> 8) != 1                ) ||  // removed for stripped xm compatibility
        (xmHeader.songLength     > XM_MAX_SONG_LENGTH) ||
        (xmHeader.nChannels      > XM_MAX_CHANNELS   ) ||
        (xmHeader.nChannels      < XM_MIN_CHANNELS   ) ||
        (xmHeader.nInstruments   > XM_MAX_INSTRUMENTS) ||
        (xmHeader.nPatterns      > XM_MAX_PATTERNS)    ||
        (xmHeader.defaultBpm     > XM_MAX_BPM)         ||
        (xmHeader.defaultTempo   > XM_MAX_TEMPO) ) {
        if ( showDebugInfo_ ) {
            XmDebugShow::fileHeader( xmHeader );
            std::cout << "\nError reading xmHeader, this is not an xm file.\n";
        }
        return 0;
    }
    trackerType_ = TRACKER_FT2;
    songTitle_ = "";
    for ( int i = 0; i < XM_MAX_SONG_NAME_LENGTH; i++ )
        songTitle_ += xmHeader.songTitle[i];
    
    trackerTag_ = "";
    for ( int i = 0; i < XM_TRACKER_NAME_LENGTH; i++ ) 
        trackerTag_ += xmHeader.trackerName[i];
    
    xmHeader.id             = 0; // use as zero terminator
    useLinearFrequencies_   = (bool)(xmHeader.flags & XM_LINEAR_FREQUENCIES_FLAG);
    isCustomRepeat_         = true;
    panningStyle_           = PANNING_STYLE_XM;
    nChannels_              = xmHeader.nChannels;
    nInstruments_           = xmHeader.nInstruments;
    nSamples_               = 0;
    nPatterns_              = xmHeader.nPatterns;
    defaultTempo_           = xmHeader.defaultTempo;
    defaultBpm_             = xmHeader.defaultBpm;
    songLength_             = xmHeader.songLength;
    songRestartPosition_    = xmHeader.songRestartPosition;
    if ( showDebugInfo_ )
        XmDebugShow::fileHeader( xmHeader );
    
    // initialize XM specific variables:
    if ( useLinearFrequencies_ ) {
        minPeriod_ = 1600;  
        maxPeriod_ = 7680;  
    } 
    else {
        minPeriod_ = 113;
        maxPeriod_ = 27392;
    }
    for ( unsigned i = 0; i < nChannels_; i++ )
        defaultPanPositions_[i] = PANNING_CENTER;
    
    // reset entries in the order table to one above the highest pattern number.
    // the Module::getPattern() function returns an empty pattern if the pattern
    // does not exist, but this is an extra safety. Oh well.
    memset( patternTable_,0,sizeof( Module::patternTable_ ) );
    for ( unsigned i = 0; i < songLength_; i++ ) {
        unsigned k = xmHeader.patternTable[i];
        if ( k >= nPatterns_ ) 
            k = nPatterns_; 
        patternTable_[i] = k;
    }
    // start reading the patterns
    xmFile.absSeek( XM_HEADER_SIZE_PART_ONE + xmHeader.headerSize ); 
    for (unsigned patternNr = 0; patternNr < nPatterns_; patternNr++) {
        int result = loadXmPattern( xmFile,patternNr );
        if ( result )
            return 0;
        if ( showDebugInfo_ && (patternNr == XM_DEBUG_SHOW_PATTERN_NO) )
#ifdef debug_xm_show_patterns
            XmDebugShow::pattern( *(patterns_[patternNr]),nChannels_ );
#endif
    }
    // Now read all the instruments & sample data
    for ( unsigned instNr = 1; instNr <= nInstruments_; instNr++ ) {
        int result = loadXmInstrument( xmFile,instNr );
        if ( result ) 
            return 0;
    }
    isLoaded_ = true;
    return 0;
}

int Module::loadXmInstrument( VirtualFile& xmFile,int instrumentNr )
{
    InstrumentHeader    instrument;
    XmInstrumentHeader1 xmInstrumentHeader1;
    XmInstrumentHeader2 xmInstrumentHeader2;

    // read the instrument header and check if it contains any samples
    xmFile.read( &xmInstrumentHeader1,sizeof( XmInstrumentHeader1 ) );
    if ( showDebugInfo_ ) {
        std::cout << "\n\nInstrument header " << instrumentNr << ":";
        XmDebugShow::instHeader1( xmInstrumentHeader1 );
    }

    // Necessary for packed xm files:
    if ( xmInstrumentHeader1.headerSize < sizeof( XmInstrumentHeader1 ) )
        xmInstrumentHeader1.nSamples = 0;

    if ( xmInstrumentHeader1.nSamples == 0 ) {
        if ( xmInstrumentHeader1.headerSize >=
            sizeof( XmInstrumentHeader1 ) )
            instrument.name.append(
                xmInstrumentHeader1.name,XM_MAX_INSTRUMENT_NAME_LENGTH );
        else
            instrument.name = "";
        instrument.nSamples = 0;

        // Correct file pointer according to header size in file:
        xmFile.relSeek(
            (int)xmInstrumentHeader1.headerSize -
            (int)sizeof( XmInstrumentHeader1 ) );
    } 
    else {
        instrument.nSamples = xmInstrumentHeader1.nSamples;
        xmFile.read( &xmInstrumentHeader2,sizeof( XmInstrumentHeader2 ) );

        // Set file pointer to correct position for next instrument
        xmFile.relSeek( (int)xmInstrumentHeader1.headerSize -
            (int)(sizeof( XmInstrumentHeader1 ) + sizeof( XmInstrumentHeader2 )) );

        instrument.name.append(
            xmInstrumentHeader1.name,XM_MAX_INSTRUMENT_NAME_LENGTH );

        if ( instrument.nSamples > XM_MAX_SAMPLES_PER_INST ) {
            if ( showDebugInfo_ )
                std::cout
                    << "\nMore than " << XM_MAX_SAMPLES_PER_INST
                    << " (" << instrument.nSamples
                    << ") samples for this instrument, exiting!"
                    << "\nSample Header Size       : "
                    << xmInstrumentHeader2.sampleHeaderSize;
            return 0;
        }
    }

    if ( instrument.nSamples ) {
        SampleHeader    samples[XM_MAX_SAMPLES_PER_INST];
        char            sampleNames[XM_MAX_SAMPLES_PER_INST][XM_MAX_SAMPLE_NAME_LENGTH + 1];

        // take care of enveloppes
        for ( unsigned i = 0; i < XM_MAX_ENVELOPE_POINTS; i++ ) {
            instrument.volumeEnvelope[i].x = xmInstrumentHeader2.volumeEnvelope[i].x;
            instrument.volumeEnvelope[i].y = xmInstrumentHeader2.volumeEnvelope[i].y;
            instrument.panningEnvelope[i].x = xmInstrumentHeader2.panningEnvelope[i].x;
            instrument.panningEnvelope[i].y = xmInstrumentHeader2.panningEnvelope[i].y;
            if ( showDebugInfo_ )
                std::cout 
                    << "\nEnveloppe point nr " << i << ": "
                    << instrument.volumeEnvelope[i].x << ","
                    << instrument.volumeEnvelope[i].y;
        }
        instrument.nVolumePoints = xmInstrumentHeader2.nVolumePoints;
        instrument.volumeSustain = xmInstrumentHeader2.volumeSustain;
        instrument.volumeLoopStart = xmInstrumentHeader2.volumeLoopStart;
        instrument.volumeLoopEnd = xmInstrumentHeader2.volumeLoopEnd;
        instrument.volumeType = xmInstrumentHeader2.volumeType;
        instrument.volumeFadeOut = xmInstrumentHeader2.volumeFadeOut;
        instrument.nPanningPoints = xmInstrumentHeader2.nPanningPoints;
        instrument.panningSustain = xmInstrumentHeader2.panningSustain;
        instrument.panningLoopStart = xmInstrumentHeader2.panningLoopStart;
        instrument.panningLoopEnd = xmInstrumentHeader2.panningLoopEnd;
        instrument.panningType = xmInstrumentHeader2.panningType;
        instrument.vibratoType = xmInstrumentHeader2.vibratoType;
        instrument.vibratoSweep = xmInstrumentHeader2.vibratoSweep;
        instrument.vibratoDepth = xmInstrumentHeader2.vibratoDepth;
        instrument.vibratoRate = xmInstrumentHeader2.vibratoRate;
        if ( showDebugInfo_ )
            std::cout
            << "\nSample xm header size for this instrument : "
            << xmInstrumentHeader2.sampleHeaderSize << "\n";

        // start reading samples:
        for ( unsigned iSample = 0; iSample < instrument.nSamples; iSample++ ) {
            XmSampleHeader  xmSampleHeader;
            //SampleHeader& sampleHeader = samples[iSample];
            xmFile.read( &xmSampleHeader,sizeof( XmSampleHeader ) );

            xmFile.relSeek(
                (int)xmInstrumentHeader2.sampleHeaderSize
                - (int)sizeof( XmSampleHeader ) );

            if ( xmInstrumentHeader2.sampleHeaderSize < sizeof( XmSampleHeader ) )
                sampleNames[iSample][0] = '\0';
            else {
                for ( unsigned i = 0; i < XM_MAX_SAMPLE_NAME_LENGTH; i++ )
                    sampleNames[iSample][i] = xmSampleHeader.name[i];
                sampleNames[iSample][XM_MAX_SAMPLE_NAME_LENGTH] = '\0';
            }
            samples[iSample].name           = sampleNames[iSample];
            samples[iSample].finetune       = xmSampleHeader.finetune;
            samples[iSample].length         = xmSampleHeader.length;
            samples[iSample].repeatLength   = xmSampleHeader.repeatLength;
            samples[iSample].repeatOffset   = xmSampleHeader.repeatOffset;
            samples[iSample].isRepeatSample = 
                (samples[iSample].repeatLength > 0) &&
                (xmSampleHeader.type & XM_SAMPLE_LOOP_MASK);
            samples[iSample].volume         = xmSampleHeader.volume;
            samples[iSample].relativeNote   = xmSampleHeader.relativeNote;
            samples[iSample].panning        = xmSampleHeader.panning;
            samples[iSample].dataType       =
                xmSampleHeader.type & XM_SIXTEEN_BIT_SAMPLE_FLAG ?
                SAMPLEDATA_SIGNED_16BIT : SAMPLEDATA_SIGNED_8BIT;
            samples[iSample].isPingpongSample =
                xmSampleHeader.type & XM_PINGPONG_LOOP_FLAG ? true : false;
            if ( xmSampleHeader.compression == XM_ADPCM_COMPRESSION ) {
                if ( showDebugInfo_ )
                    std::cout
                        << "\n\nADPCM compressed sample data"
                        << "is not supported yet!\n";
                return -1;
            }
            if ( showDebugInfo_ )
                XmDebugShow::sampleHeader( iSample,samples[iSample] );
        }
        int sampleMap[XM_MAX_SAMPLES_PER_INST];
        int smpCnt = 0;
        for ( unsigned i = 0; i < instrument.nSamples; i++ ) {
            if ( samples[i].length > 0 ) {
                smpCnt++;
                sampleMap[i] = smpCnt;
            } 
            else
                sampleMap[i] = 0;
        }
        if ( showDebugInfo_ )
            std::cout << "\nSample for note table: \n";
        for ( unsigned i = 0; i < XM_MAXIMUM_NOTES; i++ ) {
            unsigned smpNr = xmInstrumentHeader2.sampleForNote[i];
            if ( smpNr > instrument.nSamples ) {
                if ( showDebugInfo_ )
                    std::cout
                        << "\nIllegal sample nr in"
                        << " sample to note table, exiting!";
                return -1;
            }
            if ( sampleMap[smpNr] > 0 )
                instrument.sampleForNote[i] = nSamples_ + sampleMap[smpNr];
            else
                instrument.sampleForNote[i] = 0;
            if ( showDebugInfo_ )
                std::cout << std::setw( 4 ) << instrument.sampleForNote[i];
        }

        // should be unnecessary:
        for ( unsigned i = XM_MAXIMUM_NOTES; i < MAXIMUM_NOTES; i++ )
            instrument.sampleForNote[i] = 0;   // ?

                                               // load data of these instruments' samples:
        for ( unsigned iSample = 0;
            iSample < instrument.nSamples;
            iSample++ ) {
            if ( samples[iSample].length ) {
                nSamples_++;
                loadXmSample( xmFile,nSamples_,samples[iSample] );

                if ( showDebugInfo_ ) {
#ifdef debug_xm_play_samples
                    playSample( nSamples_ );
#endif
                }
            }
        }
    }
    instruments_[instrumentNr] = new Instrument;
    instruments_[instrumentNr]->load( instrument );
    return 0;
}

int Module::loadXmSample( VirtualFile& xmFile,int sampleNr,SampleHeader& sampleHeader )
{
    SHORT           oldSample16 = 0;
    SHORT           newSample16 = 0;
    signed char     oldSample8 = 0;
    signed char     newSample8 = 0;

    sampleHeader.data =
        (SHORT *)xmFile.getSafePointer( sampleHeader.length );
    xmFile.relSeek( sampleHeader.length );

    if ( sampleHeader.data == nullptr ) { // temp DEBUG:
        if ( showDebugInfo_ )
            std::cout
            << "\nCan't get safe sample pointer, exiting!";
        return 0;
    }

    if ( sampleHeader.dataType == SAMPLEDATA_SIGNED_16BIT ) {
        SHORT   *ps = (SHORT *)sampleHeader.data;
        SHORT   *pd = ps;
        sampleHeader.length >>= 1;
        sampleHeader.repeatLength >>= 1;
        sampleHeader.repeatOffset >>= 1;
        for ( unsigned iData = 0; iData < sampleHeader.length; iData++ ) {
            newSample16 = *ps++ + oldSample16;
            *pd++ = newSample16;
            oldSample16 = newSample16;
        }
    } else {
        signed char *ps = (signed char *)(sampleHeader.data);
        signed char *pd = ps;
        for ( unsigned iData = 0; iData < sampleHeader.length; iData++ ) {
            newSample8 = *ps++ + oldSample8;
            *pd++ = newSample8;
            oldSample8 = newSample8;
        }
    }
    samples_[sampleNr] = new Sample;
    samples_[sampleNr]->load( sampleHeader );
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
            unsigned& argument = remapFx.argument;
            unsigned volUp = argument & 0xF0;
            unsigned volDn = argument & 0x0F;
            if ( volUp && volDn )
                argument = volUp;
            break;
        }
        case SET_GLOBAL_VOLUME:
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

int Module::loadXmPattern( VirtualFile & xmFile,int patternNr )
{
    Note           *iNote,*patternData;
    XmPatternHeader xmPatternHeader;

    xmFile.read( &xmPatternHeader,sizeof( XmPatternHeader ) );
    xmFile.relSeek( (int)xmPatternHeader.headerSize - (int)sizeof( XmPatternHeader ) );
    if ( xmFile.getIOError() != VIRTFILE_NO_ERROR )
        return -1;
    if ( showDebugInfo_ )         
        XmDebugShow::patternHeader( patternNr,xmPatternHeader );
    
    if ( xmPatternHeader.nRows > XM_MAX_PATTERN_ROWS )
        return  -1;
    patterns_[patternNr] = new Pattern;
    patternData = new Note[nChannels_ * xmPatternHeader.nRows];
    patterns_[patternNr]->initialise( nChannels_,xmPatternHeader.nRows,patternData );
    iNote = patternData;

    // empty patterns are not stored
    if ( !xmPatternHeader.patternSize ) 
        return 0;

    for ( unsigned n = 0; n < (nChannels_ * xmPatternHeader.nRows); n++ ) {
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
                            patternNr,xmPatternHeader.nRows,n,iNote->note );
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
                            patternNr,xmPatternHeader.nRows,n,iNote->note );
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
    std::cout
        << "\nXM Module title          : " << xmHeader.songTitle // ? to correct
        << "\nXM file Tracker ID       : " << xmHeader.fileTag   // ? to correct
        << std::hex
        << "\nXM file version          : " << xmHeader.version << std::dec
        << "\nNr of channels           : " << xmHeader.nChannels
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

void XmDebugShow::instHeader1( XmInstrumentHeader1& xmInstrumentHeader1 )
{
    std::cout
        << "\nHdr size in c++  : " << sizeof( XmInstrumentHeader1 )
        << "\nHeader size      : " << xmInstrumentHeader1.headerSize
        << "\nHeader size (res): " << xmInstrumentHeader1.reserved
        << "\nInstr. type (0)  : " << xmInstrumentHeader1.type
        << "\nNr of Samples    : " << xmInstrumentHeader1.nSamples;
}

void XmDebugShow::instHeader2( XmInstrumentHeader2& xmInstrumentHeader2 )
{
    std::cout
        << "\nSample hdr size  : " << xmInstrumentHeader2.sampleHeaderSize
        << "\nSmp hdr sz. (res): " << xmInstrumentHeader2.reserved
        /*
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        << "\n                 : " << xmInstrumentHeader2
        */
        ;

/*
    unsigned short    sampleHeaderSize;
    unsigned short    reserved;       // !!!
    unsigned char     sampleForNote[XM_MAXIMUM_NOTES];
    XmEnvelopePoint   volumeEnvelope[12];
    XmEnvelopePoint   panningEnvelope[12];
    unsigned char     nVolumePoints;
    unsigned char     nPanningPoints;
    unsigned char     volumeSustain;
    unsigned char     volumeLoopStart;
    unsigned char     volumeLoopEnd;
    unsigned char     panningSustain;
    unsigned char     panningLoopStart;
    unsigned char     panningLoopEnd;
    unsigned char     volumeType;
    unsigned char     panningType;
    unsigned char     vibratoType;
    unsigned char     vibratoSweep;
    unsigned char     vibratoDepth;
    unsigned char     vibratoRate;
    unsigned short    volumeFadeOut;
*/
}

void XmDebugShow::sampleHeader( int sampleNr,SampleHeader& smpHdr )
{
    std::cout
        << "\n\nSample # " << sampleNr << ":"
        << "\nName             : " << smpHdr.name.c_str()
        << "\nFinetune         : " << smpHdr.finetune
        << "\nLength           : " << smpHdr.length
        << "\nRepeatLength     : " << smpHdr.repeatLength
        << "\nRepeatOffset     : " << smpHdr.repeatOffset
        << "\nRepeatSample     : " << smpHdr.isRepeatSample
        << "\nVolume           : " << smpHdr.volume
        << "\nRelative Note    : " << smpHdr.relativeNote
        << "\nPanning          : " << smpHdr.panning
        << "\n16 bit sample    : "
        << ((smpHdr.dataType == SAMPLEDATA_SIGNED_16BIT) ? "Yes" : "No")
        << "\nPing Loop active : " 
        << (smpHdr.isPingpongSample ? "Yes" : "No");
}

void XmDebugShow::patternHeader( int patternNr,XmPatternHeader& xmPatternHeader )
{
    std::cout 
        << "\nPattern nr " << patternNr << " header:"
        << "\nPattern Header Size (9) : " << xmPatternHeader.headerSize
        << "\nPattern # Rows          : " << xmPatternHeader.nRows
        << "\nPattern Pack system     : " << (unsigned)xmPatternHeader.pack
        << "\nPattern Data Size       : " << xmPatternHeader.patternSize;
}

void XmDebugShow::pattern( Pattern& pattern,int nChannels )
{
    for ( unsigned row = 0; row < pattern.getnRows(); row++ ) {                     
        Note *iNote = pattern.getRow( row );
        for ( int chn = 0; chn < nChannels; chn++ ) {
            if ( chn == 0 ) {
                std::cout
                    << "\n" << std::hex << std::setw( 2 ) << row << "/"
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



