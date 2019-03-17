/*
    XM files with ADPCM compressed sample data are not supported yet.
    Stripped XM files should load normally.

    crystals.wow is detected as a valid .xm file



*/
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>

#include "module.h"
#include "virtualfile.h"

//using namespace std;

#define debug_xm_loader
#define debug_xm_show_patterns
//#define debug_xm_play_samples

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
#define XM_MAX_PATTERNS                 256
#define XM_MAX_INSTRUMENTS              128
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
  char              songTitle[20];  // Name of the XM
  unsigned char     id;             // 0x1A
  char              trackerName[20];// = "FastTracker v2.00"
  unsigned short    version;        // 0x01 0x04
  unsigned          headerSize;     // size of xmHeader from here: min. 20 + 1 bytes
  unsigned short    songLength;
  unsigned short    songRestartPosition;
  unsigned short    nChannels;
  unsigned short    nPatterns;
  unsigned short    nInstruments;
  unsigned short    flags;          // 0 = Amiga frequency table 1 = Linear frequency table
  unsigned short    defaultTempo;   
  unsigned short    defaultBpm;     
  unsigned char     patternTable[XM_MAX_SONG_LENGTH];
};                                  // size = 336 max

struct XmPatternHeader {            // typedef of the pattern xmHeader
  unsigned          headerSize;
  unsigned char     pack;
  unsigned short    nRows;
  unsigned short    patternSize;
};                                  // size = 9

struct XmInstrumentHeaderPrimary {
  unsigned          headerSize;     // size of the 2 headers
  char              name[22];
  unsigned char     type;           // should be 0, but is sometimes 128,129,253
  unsigned short    nSamples;
};                                  // 29

struct XmEnvelopePoint {
public:
    unsigned short  x;
    unsigned short  y;
};
 
struct XmInstrumentHeaderSecondary {
  unsigned          sampleHeaderSize;
  unsigned char     sampleForNote[MAXIMUM_NOTES];
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
//  unsigned char     reserved[22];   // skipping f*cking 22 BYTEs!
};                                  // 234     29 + 234 = 263

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

int Module::loadXmFile() 
{
    /*
    char                        *buf, *bufp;
    std::ifstream::pos_type     fileSize = 0;
    std::ifstream               xmFile(fileName_,std::ios::in| std::ios::binary| std::ios::ate);
    */
    unsigned fileSize = 0;
    isLoaded_ = false;
    VirtualFile xmFile( fileName_ );
    if ( xmFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;

    /*
    // load file into byte buffer and then work on that buffer only
    if ( !xmFile.is_open() ) {
         std::cout << "can't open file";
        return 0; // exit on I/O error
    }
    */
    fileSize = xmFile.fileSize();
    /*
    buf = new char [(unsigned)fileSize];
    xmFile.seekg( 0,std::ios::beg );
    xmFile.read( buf, fileSize );
    xmFile.close();
    */
    // now start reading from memory
    // ultra simple error checking
    //xmHeader = (XmHeader *)buf;
    XmHeader xmHeader;
    if ( xmFile.read( &xmHeader,sizeof( XmHeader ) ) ) return 0;
    if (
        //(xmHeader.id != 0x1A                         ) ||  // removed for stripped xm compatibility
        //((xmHeader.version >> 8) != 1                ) ||  // removed for stripped xm compatibility
        (xmHeader.songLength     > XM_MAX_SONG_LENGTH) ||
        (xmHeader.nChannels      > XM_MAX_CHANNELS   ) ||
        (xmHeader.nChannels      < XM_MIN_CHANNELS   ) ||
        (xmHeader.nInstruments   > XM_MAX_INSTRUMENTS) ||
        (xmHeader.nPatterns      > XM_MAX_PATTERNS)    ||
        (xmHeader.defaultBpm     > XM_MAX_BPM)         ||
        (xmHeader.defaultTempo   > XM_MAX_TEMPO)
        ) {
#ifdef debug_xm_loader
        char *hex = "0123456789ABCDEF";
        std::cout << "\nXm file xmHeader:";
        std::cout << "\nID                   = ";
        
        if (xmHeader.id > 0xF) std::cout << hex[(xmHeader.id >> 4) & 0xF];
        std::cout << hex[xmHeader.id & 0xF];
        std::cout << "\nVersion              = ";
        if (xmHeader.version > 0xFFF) std::cout << hex[(xmHeader.version >> 12) & 0xF];
        if (xmHeader.version > 0xFF ) std::cout << hex[(xmHeader.version >>  8) & 0xF] << ".";
        if (xmHeader.version > 0xF  ) std::cout << hex[(xmHeader.version >>  4) & 0xF];
        std::cout << hex[xmHeader.version & 0xF];
        std::cout << "\nSong length          = " << xmHeader.songLength;
        std::cout << "\nNr of channels       = " << xmHeader.nChannels;
        std::cout << "\nNr of instruments    = " << xmHeader.nInstruments;
        std::cout << "\nNr of Patterns       = " << xmHeader.nPatterns;
        std::cout << "\nDefault Bpm          = " << xmHeader.defaultBpm;
        std::cout << "\nDefault Tempo        = " << xmHeader.defaultTempo;
        std::cout << "\nXM MAX Tempo         = " << XM_MAX_TEMPO;
        std::cout << "\nError reading xmHeader, this is not an xm file.\n";
#endif
        //delete [] buf;
        return 0;
    }
    trackerType_ = TRACKER_FT2;
    songTitle_ = "";
    for ( int i = 0; i < XM_MAX_SONG_NAME_LENGTH; i++ ) {
        songTitle_ += xmHeader.songTitle[i];
    }
    trackerTag_ = "";
    for ( int i = 0; i < XM_TRACKER_NAME_LENGTH; i++ ) {
        trackerTag_ += xmHeader.trackerName[i];
    }
    xmHeader.id              = 0; // use as zero terminator
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
#ifdef debug_xm_loader
    std::cout << "\nXM Module title          = " << songTitle_.c_str();
    std::cout << "\nXM file Tracker ID       = " << trackerTag_.c_str();
    std::cout << "\n# Channels               = " << xmHeader.nChannels;
    std::cout << "\n# Instruments            = " << xmHeader.nInstruments;
    std::cout << "\n# Patterns               = " << xmHeader.nPatterns;
    std::cout << "\nDefault Tempo            = " << xmHeader.defaultTempo;
    std::cout << "\nDefault Bpm              = " << xmHeader.defaultBpm;
    std::cout << "\nSong Length              = " << xmHeader.songLength;
    std::cout << "\nSong restart position    = " << xmHeader.songRestartPosition;
    std::cout << "\nHeader Size              = " << xmHeader.headerSize;
    std::cout << "\nFrequency / period system: " << (useLinearFrequencies_ ? "Linear" : "Amiga");
    std::cout << "\nSize of XmHeader                     = " << sizeof(XmHeader);
    std::cout << "\nSize of XmPatternHeader              = " << sizeof(XmPatternHeader);
    std::cout << "\nSize of XmInstrumentHeaderPrimary    = " << sizeof(XmInstrumentHeaderPrimary);
    std::cout << "\nSize of XmEnvelopePoint              = " << sizeof(XmEnvelopePoint);
    std::cout << "\nSize of XmInstrumentHeaderSecondary  = " << sizeof(XmInstrumentHeaderSecondary);
    std::cout << "\nSize of XmSampleHeader               = " << sizeof(XmSampleHeader) << "\n";
    //_getch();
#endif
    // initialize xm specific variables:
    if ( useLinearFrequencies_ )
    {
        minPeriod_ = 1600;  
        maxPeriod_ = 7680;  
    } else {
        minPeriod_ = 113;
        maxPeriod_ = 27392;
    }
    for ( unsigned i = 0; i < nChannels_; i++ )
    {
        defaultPanPositions_[i] = PANNING_CENTER;
    }
    /*
    an XM file does never contain an empty pattern but it may be used.
    we therefor always create an empty pattern, but only one.
    */
    memset(patternTable_, 0, sizeof(*patternTable_) * XM_MAX_SONG_LENGTH);
    for (unsigned i = 0; i < songLength_/*XM_MAX_SONG_LENGTH*/; i++) {
        unsigned k = xmHeader.patternTable[i];
        if ((k + 1) > nPatterns_) k = nPatterns_; 
        patternTable_[i] = k;
    }
    // start reading the patterns


    //bufp = buf + sizeof(XmHeader);
    //bufp = buf + XM_HEADER_SIZE_PART_ONE + xmHeader.headerSize;



    for (unsigned iPattern = 0; iPattern < nPatterns_; iPattern++) {
        Note            *iNote, *patternData;
        //unsigned        pack, volumeColumn;
        XmPatternHeader xmPatternHeader;

        //xmPatternHeader = (XmPatternHeader *)bufp;
        xmFile.read( &xmPatternHeader,sizeof( XmPatternHeader ) );
        xmFile.relSeek( (int)xmPatternHeader.headerSize - (int)sizeof( XmPatternHeader ) );
        if ( xmFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;

#ifdef debug_xm_loader
#define SHOW_PATTERN_NO 0
        if (iPattern == (SHOW_PATTERN_NO + 1)) _getch();
        std::cout << "\nPattern # " << iPattern << ":";
        std::cout << "\nPattern Header Size (9) = " << xmPatternHeader.headerSize;
        std::cout << "\nPattern # Rows          = " << xmPatternHeader.nRows;
        std::cout << "\nPattern Pack system     = " << (unsigned)xmPatternHeader.pack;
        std::cout << "\nPattern Data Size       = " << xmPatternHeader.patternSize;
#endif
        if (/*xmPatternHeader.pack ||*/ (xmPatternHeader.nRows > XM_MAX_PATTERN_ROWS) ) {
            //delete [] buf;
            return 0;
        }  
        patterns_[iPattern] = new Pattern;
        patternData = new Note[nChannels_ * xmPatternHeader.nRows];
        patterns_[iPattern]->initialise(nChannels_, xmPatternHeader.nRows, patternData );
        iNote = patternData;
        // below (memset) is done in pattern class
        //memset(patternData, 0, sizeof(Note) * nChannels_ * xmPatternHeader.nRows);
        //bufp += sizeof(XmPatternHeader);
        //bufp += xmPatternHeader.headerSize;

        // empty patterns are not stored
        if ( !xmPatternHeader.patternSize ) continue; // ?

        for ( unsigned n = 0; n < (nChannels_ * xmPatternHeader.nRows); n++ ) 
        {
            //pack = *((unsigned char *)bufp++);
            unsigned char pack;
            unsigned char volumeColumn;
            if ( xmFile.read( &pack,sizeof( unsigned char ) ) ) return 0;

            if ( pack & XM_NOTE_IS_PACKED ) 
            {
                if ( pack & XM_NOTE_AVAIL ) 
                {
                    //iNote->note         = *((unsigned char *)bufp++);
                    unsigned char note;
                    if( xmFile.read( &note,sizeof( unsigned char ) ) ) return 0;
                    iNote->note = note;
#ifdef debug_xm_loader
                    if ( iNote->note > XM_KEY_OFF ) {
                        std::cout << "\nPattern # " << iPattern << ":";
                        std::cout << "\nPattern Header Size (9) = " << xmPatternHeader.headerSize;
                        std::cout << "\nPattern # Rows          = " << xmPatternHeader.nRows;
                        std::cout << "\nPattern Pack system     = " << (unsigned)xmPatternHeader.pack;
                        std::cout << "\nPattern Data Size       = " << xmPatternHeader.patternSize;
                        std::cout << "\nRow nr                  = " << n / xmPatternHeader.nRows;
                        std::cout << "\nColumn nr               = " << n % xmPatternHeader.nRows;
                        std::cout << "\nIllegal note            = " << iNote->note;
                        std::cout << "\n";
                        _getch();
                    }
#endif
                } 
                if ( pack & XM_INSTRUMENT_AVAIL ) 
                {
                    //iNote->instrument   = *((unsigned char *)bufp++);
                    unsigned char instrument;
                    if ( xmFile.read( &instrument,sizeof( unsigned char ) ) ) return 0;
                    iNote->instrument = instrument;
                }
                if ( pack & XM_VOLUME_COLUMN_AVAIL ) 
                {
                    //volumeColumn        = *((unsigned char *)bufp++);
                    if ( xmFile.read( &volumeColumn,sizeof( unsigned char ) ) ) return 0;
                } else volumeColumn = 0;

                if ( pack & XM_EFFECT_AVAIL ) 
                {
                    //iNote->effects[1].effect = *((unsigned char *)bufp++);
                    unsigned char effect;
                    if ( xmFile.read( &effect,sizeof( unsigned char ) ) ) return 0;
                    iNote->effects[1].effect = effect;
                } 
                if ( pack & XM_EFFECT_ARGUMENT_AVAIL ) 
                {
                    //iNote->effects[1].argument = *((unsigned char *)bufp++);
                    unsigned char argument;
                    if ( xmFile.read( &argument,sizeof( unsigned char ) ) ) return 0;
                    iNote->effects[1].argument = argument;
                } 
            } else {
                iNote->note = pack;
#ifdef debug_xm_loader
                if ( iNote->note > XM_KEY_OFF ) {
                    std::cout << "\nPattern # " << iPattern << ":";
                    std::cout << "\nPattern Header Size (9) = " << xmPatternHeader.headerSize;
                    std::cout << "\nPattern # Rows          = " << xmPatternHeader.nRows;
                    std::cout << "\nPattern Pack system     = " << (unsigned)xmPatternHeader.pack;
                    std::cout << "\nPattern Data Size       = " << xmPatternHeader.patternSize;
                    std::cout << "\nRow nr                  = " << n / xmPatternHeader.nRows;
                    std::cout << "\nColumn nr               = " << n % xmPatternHeader.nRows;
                    std::cout << "\nIllegal note            = " << iNote->note;
                    std::cout << "\n";
                    _getch();
                }
#endif           
                /*
                iNote->instrument           = *((unsigned char *)bufp++);
                volumeColumn                = *((unsigned char *)bufp++);
                iNote->effects[1].effect    = *((unsigned char *)bufp++);
                iNote->effects[1].argument  = *((unsigned char *)bufp++);
                */
                unsigned char instrument;
                unsigned char effect;
                unsigned char argument;

                if ( xmFile.read( &instrument,sizeof( unsigned char ) ) ) return 0;
                if ( xmFile.read( &volumeColumn,sizeof( unsigned char ) ) ) return 0;
                if ( xmFile.read( &effect,sizeof( unsigned char ) ) ) return 0;
                if ( xmFile.read( &argument,sizeof( unsigned char ) ) ) return 0;
                iNote->instrument = instrument;
                iNote->effects[1].effect = effect;
                iNote->effects[1].argument = argument;
            }
            if ( iNote->note == XM_KEY_OFF ) iNote->note = KEY_OFF;
            /*
            // we might not use period values later --------------
            if ( iNote->note && (iNote->note != KEY_OFF) ) {   
                if( useLinearFrequencies_ ) {
                    iNote->period = 120 * 64 - iNote->note * 64; 
                } else {
                    iNote->period = periods[iNote->note];
                }
            } else iNote->period = 0;
            */
            // ---------------------------------------------------
            if (volumeColumn) {
                if      (volumeColumn > 0xF0)
                    iNote->effects[0].effect   = TONE_PORTAMENTO;
                else if (volumeColumn > 0xE0)
                    iNote->effects[0].effect   = PANNING_SLIDE;
                else if (volumeColumn > 0xD0)
                    iNote->effects[0].effect   = PANNING_SLIDE;
                else if (volumeColumn > 0xC0)
                    iNote->effects[0].effect   = SET_FINE_PANNING;
                else if (volumeColumn > 0xB0)
                    iNote->effects[0].effect   = VIBRATO; 
                else if (volumeColumn > 0xA0)
                    iNote->effects[0].effect   = SET_VIBRATO_SPEED;
                else if (volumeColumn > 0x90)
                    iNote->effects[0].effect   = EXTENDED_EFFECTS; //FINE_VOLUME_SLIDE_UP
                else if (volumeColumn > 0x80)
                    iNote->effects[0].effect   = EXTENDED_EFFECTS; //FINE_VOLUME_SLIDE_DOWN
                else if (volumeColumn > 0x70)
                    iNote->effects[0].effect   = VOLUME_SLIDE;
                else if (volumeColumn > 0x60)
                    iNote->effects[0].effect   = VOLUME_SLIDE;
                else 
                    iNote->effects[0].effect   = SET_VOLUME;

                if (iNote->effects[0].effect == SET_VOLUME) {
                    iNote->effects[0].argument = volumeColumn - 0x10;
                } else {
                    iNote->effects[0].argument = volumeColumn & 0xF;
                }
                switch (iNote->effects[0].effect) {
                    case TONE_PORTAMENTO :
                    {
                        iNote->effects[0].argument <<= 4;
                        // emulate FT2 bug:
                        if ( iNote->effects[1].effect == NOTE_DELAY )
                        {
                            iNote->effects[0].effect = NO_EFFECT;
                            iNote->effects[0].argument = NO_EFFECT;
                        }
                        break;
                    }
                    case EXTENDED_EFFECTS : 
                    {
                        if (volumeColumn > 0x80) {
                            iNote->effects[0].argument += (FINE_VOLUME_SLIDE_DOWN << 4);
                        } else {
                            iNote->effects[0].argument += (FINE_VOLUME_SLIDE_UP << 4);
                        }
                        break;
                    }
                    case PANNING_SLIDE: 
                    {
                        if (volumeColumn > 0xE0) // slide right
                                iNote->effects[0].argument <<= 4;
                        break;
                    }
                    case SET_FINE_PANNING: // rough panning, really
                    {
                        iNote->effects[0].argument <<= 4; // *= 17; // ?
                        break;
                    }
                    case SET_VIBRATO_SPEED:
                    {
                        iNote->effects[0].argument <<= 4;
                        break;
                    }
                    /*
                    case VIBRATO :  
                    {
                        if ( volumeColumn < 0xB0 )
                        {
                            iNote->effects[0].effect = SET_VIBRATO_SPEED;
                            iNote->effects[0].argument <<= 4;
                        }
                        break;
                    }
                    */
                    case VOLUME_SLIDE: 
                    {
                        if (volumeColumn > 0x70) // slide up
                                iNote->effects[0].argument <<= 4;                          
                        break;
                    }
                }                
            }            
            // do some error checking & effect remapping:
            for (unsigned fxloop = 0; fxloop < MAX_EFFECT_COLUMNS; fxloop++) {
                switch (iNote->effects[fxloop].effect) {
                    case 0: // arpeggio / no effect
                    {
                        if ( (fxloop == 1) &&
                            iNote->effects[fxloop].argument )
                            iNote->effects[fxloop].effect = ARPEGGIO;
                        break;
                    }
                    case TONE_PORTAMENTO_AND_VOLUME_SLIDE:
                    case VIBRATO_AND_VOLUME_SLIDE:
                    case VOLUME_SLIDE:
                    {
                        // in .mod & .xm files volume slide up has
                        // priority over volume slide down
                        unsigned& argument = iNote->effects[fxloop].argument;
                        unsigned volUp = argument & 0xF0;
                        unsigned volDn = argument & 0x0F;
                        if ( volUp && volDn ) argument = volUp;
                        break;
                    }
                    case SET_GLOBAL_VOLUME:
                    case SET_VOLUME :
                    {
                        if (iNote->effects[fxloop].argument > MAX_VOLUME)
                            iNote->effects[fxloop].argument = MAX_VOLUME;
                        break;
                    }
                    case SET_TEMPO :
                    {
                        if ( !iNote->effects[fxloop].argument ) {
                            iNote->effects[fxloop].effect = NO_EFFECT;
                        } else {
                            if ( iNote->effects[fxloop].argument > 0x1F ) {
                                iNote->effects[fxloop].effect = SET_BPM;
                            }
                        }
                        break;
                    }
                }
            }
#ifdef debug_xm_loader
#ifdef debug_xm_show_patterns
            if (iPattern == SHOW_PATTERN_NO) {
                int row = n / nChannels_;
                int chn = n % nChannels_;
                if ( chn == 0 ) { 
                     std::cout << std::endl
                        << std::hex << std::setw( 2 ) << row << "/"
                        << std::setw( 2 ) << xmPatternHeader.nRows << "|";
                } else if ( chn < 16 )
                {
                     std::cout << noteStrings[iNote->note] << "|";
                } //else  std::cout << std::endl;
                 std::cout << std::dec;
                /*
                char    hex[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

                if ((!(n % 5)) && (nChannels_ > 5)) std::cout << "\n";
                if (!(n % nChannels_)) std::cout << "\n";
                elsestd::cout << "|";
                std::cout << noteStrings[iNote->note];
                std::cout << ":";
                if (iNote->instrument < 10) std::cout << "0";
                std::cout << iNote->instrument;
                if (iNote->effects[0].effect < 0x10) std::cout << "0";
                std::cout << hex[iNote->effects[0].effect];
                std::cout << hex[iNote->effects[0].argument >> 4];
                std::cout << hex[iNote->effects[0].argument & 0xF];
                if (iNote->effects[1].effect < 0x10) std::cout << "0";
                std::cout << hex[iNote->effects[1].effect];
                std::cout << hex[iNote->effects[1].argument >> 4];
                std::cout << hex[iNote->effects[1].argument & 0xF];
                */
            }
#endif
#endif
            iNote++;
        }
    }
    /*
    Create the empty pattern
    */
    patterns_[nPatterns_] = new Pattern;
    patterns_[nPatterns_]->initialise( nChannels_, XM_MAX_PATTERN_ROWS, 
                                      new Note[nChannels_ * XM_MAX_PATTERN_ROWS] );
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // ************************************************************************
    // Now read all the instruments & sample data
    unsigned smpIdx = 1;
    for ( unsigned iInstrument = 1; iInstrument <= nInstruments_; iInstrument++ ) {
        InstrumentHeader            instrument;
        char                        instrumentName[XM_MAX_INSTRUMENT_NAME_LENGTH + 1];
        /*
        char                        *thisInstrument;
        XmInstrumentHeaderPrimary   *instrumentHeader1;
        XmInstrumentHeaderSecondary *instrumentHeader2;

        thisInstrument = bufp;
        instrumentHeader1 = (XmInstrumentHeaderPrimary *)bufp;
        */
        XmInstrumentHeaderPrimary   xmInstrumentHeaderPrimary;
        XmInstrumentHeaderSecondary xmInstrumentHeaderSecondary;

        if ( xmFile.read( &xmInstrumentHeaderPrimary,sizeof( XmInstrumentHeaderPrimary ) ) ) return 0;


        if ( xmInstrumentHeaderPrimary.headerSize < sizeof( XmInstrumentHeaderPrimary ) ) {
            instrumentName[0] = '\0';
            instrument.nSamples = 0;
            xmFile.relSeek( 
                (int)xmInstrumentHeaderPrimary.headerSize - 
                (int)sizeof( XmInstrumentHeaderPrimary ) 
            );
        } else {
            //instrumentHeader2 = (XmInstrumentHeaderSecondary *)(bufp + sizeof( XmInstrumentHeaderPrimary ));
            if ( xmFile.read( &xmInstrumentHeaderSecondary,sizeof( XmInstrumentHeaderSecondary ) ) ) return 0;

            for ( int i = 0; i < XM_MAX_INSTRUMENT_NAME_LENGTH; i++ ) 
            {
                instrumentName[i] = xmInstrumentHeaderPrimary.name[i];
            }
            instrumentName[XM_MAX_INSTRUMENT_NAME_LENGTH] = '\0';
            instrument.nSamples = xmInstrumentHeaderPrimary.nSamples;

            xmFile.relSeek( (int)xmInstrumentHeaderPrimary.headerSize -
                (int)(sizeof( XmInstrumentHeaderPrimary ) + sizeof( XmInstrumentHeaderSecondary )) );
        }
        //bufp += xmInstrumentHeaderPrimary.headerSize;

        instrument.name = instrumentName;

#ifdef debug_xm_loader
        std::cout << "\n\nInstrument xmHeader " << iInstrument << " size = " << xmInstrumentHeaderPrimary.headerSize;
        std::cout << "\nInstrument name          = " << instrument.name.c_str();
        std::cout << "\nInstrument type (0)      = " << (int)(xmInstrumentHeaderPrimary.type);
        std::cout << "\nNr of samples            = " << instrument.nSamples;
        if (instrument.nSamples)
            std::cout << "\nSample Header Size       = " << xmInstrumentHeaderSecondary.sampleHeaderSize;          
#endif
        if ( instrument.nSamples ) 
        { 
            unsigned        sampleOffset;
            SampleHeader    samples[MAX_SAMPLES];
            char            sampleNames[MAX_SAMPLES][XM_MAX_SAMPLE_NAME_LENGTH + 1];   

            for ( unsigned i = 0; i < MAXIMUM_NOTES; i++ )
                instrument.sampleForNote[i] = xmInstrumentHeaderSecondary.sampleForNote[i] + smpIdx;
            for ( unsigned i = 0; i < 12; i++ ) 
            {
                instrument.volumeEnvelope [i].x = xmInstrumentHeaderSecondary.volumeEnvelope [i].x;
                instrument.volumeEnvelope [i].y = xmInstrumentHeaderSecondary.volumeEnvelope [i].y;
                instrument.panningEnvelope[i].x = xmInstrumentHeaderSecondary.panningEnvelope[i].x;
                instrument.panningEnvelope[i].y = xmInstrumentHeaderSecondary.panningEnvelope[i].y;
#ifdef debug_xm_loader
                std::cout << "\nEnveloppe point #" << i << ": "
                    << instrument.volumeEnvelope[i].x << "," 
                    << instrument.volumeEnvelope[i].y;
#endif
            }
            instrument.nVolumePoints    = xmInstrumentHeaderSecondary.nVolumePoints;
            instrument.volumeSustain    = xmInstrumentHeaderSecondary.volumeSustain;
            instrument.volumeLoopStart  = xmInstrumentHeaderSecondary.volumeLoopStart;
            instrument.volumeLoopEnd    = xmInstrumentHeaderSecondary.volumeLoopEnd;
            instrument.volumeType       = xmInstrumentHeaderSecondary.volumeType;
            instrument.volumeFadeOut    = xmInstrumentHeaderSecondary.volumeFadeOut;
            instrument.nPanningPoints   = xmInstrumentHeaderSecondary.nPanningPoints;
            instrument.panningSustain   = xmInstrumentHeaderSecondary.panningSustain;
            instrument.panningLoopStart = xmInstrumentHeaderSecondary.panningLoopStart;
            instrument.panningLoopEnd   = xmInstrumentHeaderSecondary.panningLoopEnd;
            instrument.panningType      = xmInstrumentHeaderSecondary.panningType;
            instrument.vibratoType      = xmInstrumentHeaderSecondary.vibratoType;
            instrument.vibratoSweep     = xmInstrumentHeaderSecondary.vibratoSweep;
            instrument.vibratoDepth     = xmInstrumentHeaderSecondary.vibratoDepth;
            instrument.vibratoRate      = xmInstrumentHeaderSecondary.vibratoRate;
#ifdef debug_xm_loader
            std::cout << "\nSample xmHeader size for this instrument = ";
            std::cout << xmInstrumentHeaderSecondary.sampleHeaderSize;
            std::cout << "\n";
            for (unsigned i = 0; i < MAXIMUM_NOTES; i++)
                std::cout << instrument.sampleForNote[i] << " ";
#endif
            for ( unsigned iSample = 0; iSample < instrument.nSamples; iSample++ ) 
            {
                //XmSampleHeader  *xmSample = (XmSampleHeader *)bufp;
                XmSampleHeader  xmSampleHeader;        
                if ( xmFile.read( &xmSampleHeader,sizeof( XmSampleHeader ) ) ) return 0;

                //bufp += xmInstrumentHeaderSecondary.sampleHeaderSize;
                xmFile.relSeek(
                    (int)xmInstrumentHeaderSecondary.sampleHeaderSize
                    -(int)sizeof( XmSampleHeader ) 
                );
                if ( xmInstrumentHeaderSecondary.sampleHeaderSize < sizeof( XmSampleHeader ) ) {
                    sampleNames[iSample][0] = '\0';
                } else {
                    for ( unsigned i = 0; i < XM_MAX_SAMPLE_NAME_LENGTH; i++ ) {
                        sampleNames[iSample][i] = xmSampleHeader.name[i];                   
                    }
                    sampleNames[iSample][XM_MAX_SAMPLE_NAME_LENGTH] = '\0';
                }
                samples[iSample].name           = sampleNames[iSample];
                samples[iSample].finetune       = xmSampleHeader.finetune;
                samples[iSample].length         = xmSampleHeader.length;
                samples[iSample].repeatLength   = xmSampleHeader.repeatLength;
                samples[iSample].repeatOffset   = xmSampleHeader.repeatOffset;
                samples[iSample].isRepeatSample = (samples[iSample].repeatLength > 0) &&
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
                    std::cout << "\n\nADPCM compressed sample data is not supported yet!\n";
                    //delete [] buf;
                    return 0;
                }
#ifdef debug_xm_loader
                std::cout << "\n\nSample # " << iSample << ":";
                std::cout << "\nName             = " << samples[iSample].name.c_str();
                std::cout << "\nFinetune         = " << samples[iSample].finetune;
                std::cout << "\nLength           = " << samples[iSample].length;
                std::cout << "\nRepeatLength     = " << samples[iSample].repeatLength;
                std::cout << "\nRepeatOffset     = " << samples[iSample].repeatOffset;
                std::cout << "\nRepeatSample     = " << samples[iSample].isRepeatSample;
                std::cout << "\nVolume           = " << samples[iSample].volume;
                std::cout << "\nRelative Note    = " << samples[iSample].relativeNote;
                std::cout << "\nPanning          = " << samples[iSample].panning;
                std::cout << "\n16 bit sample    = " << 
                    ((samples[iSample].dataType == SAMPLEDATA_SIGNED_16BIT) ? "Yes" : "No");
                std::cout << "\nPing Loop active = " << 
                    (samples[iSample].isPingpongSample ? "Yes" : "No");
#endif
            }
            sampleOffset = 0;
            for ( unsigned iSample = 0; iSample < instrument.nSamples; iSample++ ) 
            {
                if ( samples[iSample].length ) 
                {
                    SHORT           oldSample16 = 0;
                    SHORT           newSample16 = 0;
                    signed char     oldSample8 = 0;
                    signed char     newSample8 = 0;
                    nSamples_++;


                    //samples[iSample].data = (SHORT *)(bufp + sampleOffset);
                    samples[iSample].data = (SHORT *)xmFile.getSafePointer( samples[iSample].length );                    
                    if ( samples[iSample].data == nullptr )  // temp DEBUG:
                    {
                        return 0;
                    }

                    sampleOffset += samples[iSample].length;
                    if (samples[iSample].dataType == SAMPLEDATA_SIGNED_16BIT ) {
                        SHORT   *ps = (SHORT *)samples[iSample].data;
                        SHORT   *pd = ps;
                        samples[iSample].length       >>= 1; 
                        samples[iSample].repeatLength >>= 1;
                        samples[iSample].repeatOffset >>= 1;
                        for ( unsigned iData = 0; iData < samples[iSample].length; iData++ ) {
                            newSample16 = *ps++ + oldSample16;
                            *pd++       = newSample16;
                            oldSample16 = newSample16;
                        }
                    } else {
                        signed char *ps = (signed char *)(samples[iSample].data);
                        signed char *pd = ps;
#ifdef debug_xm_loader
#define SHOWNR 19
                        std::cout << "\nStart: " << (int)oldSample8 << "\n";
                        for (unsigned iData = 0; iData < SHOWNR; iData++) {
                            SHORT   t = (SHORT)ps[iData];
                            if (t >= 0) std::cout << " ";
                            if (abs(t) < 100) std::cout << " ";
                            if (abs(t) < 10 ) std::cout << " ";
                            std::cout << t;
                            //cout << " ";
                        }
                        std::cout << "\n";
#endif
                        for ( unsigned iData = 0; iData < samples[iSample].length; iData++ ) {
                            newSample8 = *ps++ + oldSample8;
#ifdef debug_xm_loader
                            if (iData < SHOWNR) {
                                SHORT   t = newSample8;
                                if (t >= 0) std::cout << " ";
                                if (abs(t) < 100) std::cout << " ";
                                if (abs(t) < 10 ) std::cout << " ";
                                std::cout << t;
                                //cout << " ";
                            } 
                            if (iData == SHOWNR) std::cout << "\n";
                            /*
                            if      (newSample8 < -128) {std::cout << " " << newSample8; *pd++ = -128; }
                            else if (newSample8 >  127) {std::cout << " " << newSample8; *pd++ =  127; }
                            else */
#endif
                            *pd++      = newSample8; 
                            oldSample8 = newSample8;
                        }
                    }
                    samples_[smpIdx] = new Sample;
                    samples_[smpIdx]->load( samples[iSample] );
                    smpIdx++;
                    /*
                    instrument.samples[iSample] = new Sample;
                    instrument.samples[iSample]->load(samples[iSample]);
                    */
#ifdef debug_xm_loader
                    std::cout << "\n"; 
                    // _getch();
#endif
                } /*else {
                    samples[iSample].data = 0;
                }*/
            }
            //bufp += sampleOffset;
            xmFile.relSeek( sampleOffset );
#ifdef debug_xm_loader
#ifdef debug_xm_play_samples
            for (unsigned iSample = 0; iSample < instrument.nSamples; iSample++) {
                if (samples[iSample].length) {
                    HWAVEOUT        hWaveOut;
                    WAVEFORMATEX    waveFormatEx;
                    MMRESULT        result;
                    WAVEHDR         waveHdr;
//                    std::cout << "\nFile size                = " << fileSize;
//                    std::cout << "\nFile offset              = " << (unsigned)(bufp - buf);
//                    std::cout << "\nSample data offset       = " << (unsigned)((char *)samples[iSample].data - buf);
//                    std::cout << "\nSample length (bytes)    = " << samples[iSample].length;
//                    std::cout << "\nSample length (samples)  = " << samples[iSample].length;
                    waveFormatEx.wFormatTag     = WAVE_FORMAT_PCM;
                    waveFormatEx.nChannels      = 1;
                    waveFormatEx.nSamplesPerSec = 8000; // frequency
                    waveFormatEx.wBitsPerSample = 16;
                    waveFormatEx.nBlockAlign    = waveFormatEx.nChannels * 
                                                 (waveFormatEx.wBitsPerSample >> 3);
                    waveFormatEx.nAvgBytesPerSec= waveFormatEx.nSamplesPerSec * 
                                                  waveFormatEx.nBlockAlign;
                    waveFormatEx.cbSize         = 0;

                    result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormatEx, 
                                         0, 0, CALLBACK_NULL);
                    if (result != MMSYSERR_NOERROR) {
                        if ( !iInstrument ) std::cout << "\nError opening wave mapper!\n";
                    } else {
                        int retry = 0;
                        if ( !iInstrument ) std::cout << "\nWave mapper successfully opened!\n";
                        std::cout << "\nPlaying sample # " << iSample;
                        waveHdr.dwBufferLength = 
                            instrument.samples[iSample]->getLength ()                  
                            * waveFormatEx.nBlockAlign;
                        waveHdr.lpData = (LPSTR)
                            (instrument.samples[iSample]->getData ());
                        waveHdr.dwFlags = 0;

                        result = waveOutPrepareHeader(hWaveOut, &waveHdr, 
                                                                      sizeof(WAVEHDR));
                        while ((result != MMSYSERR_NOERROR) && (retry < 10)) {
                            retry++;
                            std::cout << "\nError preparing wave mapper xmHeader!";
                            switch (result) {
                                case MMSYSERR_INVALHANDLE : 
                                    { 
                                    std::cout << "\nSpecified device handle is invalid.";
                                    break;
                                    }
                                case MMSYSERR_NODRIVER    : 
                                    {
                                    std::cout << "\nNo device driver is present.";
                                    break;
                                    }
                                case MMSYSERR_NOMEM       : 
                                    {
                                    std::cout << "\nUnable to allocate or lock memory.";
                                    break;
                                    }
                                default:
                                    {
                                    std::cout << "\nOther unknown error " << result;
                                    }
                            }
                            Sleep(1);
                            result = waveOutPrepareHeader(hWaveOut, &waveHdr, 
                                                                      sizeof(WAVEHDR));
                        } 
                        result = waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));  
                        retry = 0;
                        while ((result != MMSYSERR_NOERROR) && (retry < 10)) {
                            retry++;
                            std::cout << "\nError writing to wave mapper!";
                            switch (result) {
                                case MMSYSERR_INVALHANDLE : 
                                    { 
                                    std::cout << "\nSpecified device handle is invalid.";
                                    break;
                                    }
                                case MMSYSERR_NODRIVER    : 
                                    {
                                    std::cout << "\nNo device driver is present.";
                                    break;
                                    }
                                case MMSYSERR_NOMEM       : 
                                    {
                                    std::cout << "\nUnable to allocate or lock memory.";
                                    break;
                                    }
                                case WAVERR_UNPREPARED    : 
                                    {
                                    std::cout << "\nThe data block pointed to by the pwh parameter hasn't been prepared.";
                                    break;
                                    }
                                default:
                                    {
                                    std::cout << "\nOther unknown error " << result;
                                    }
                            }
                            result = waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));            
                            Sleep(10);
                        } 
                        _getch();
                        waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
                        waveOutReset(hWaveOut);
                        waveOutClose(hWaveOut);
                    }
		        }
            }
#endif
#endif
        }
        instruments_[iInstrument] = new Instrument;
        instruments_[iInstrument]->load( instrument );        
    }
    isLoaded_ = true;
    //delete [] buf;
    return 0;
}
