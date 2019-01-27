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

using namespace std;

//#define debug_xm_loader
//#define debug_xm_show_patterns
//#define debug_xm_play_samples

#ifdef debug_xm_loader
#include <bitset>
#include <iomanip>
#endif

extern const char *noteStrings[2 + MAXIMUM_NOTES];

#define XM_HEADER_SIZE_PART_ONE         60
#define XM_MAX_SONG_NAME_LENGTH         20
#define XM_TRACKER_NAME_LENGTH          20
#define XM_MAX_INSTRUMENT_NAME_LENGTH   22
#define XM_MAX_SAMPLE_NAME_LENGTH       22
#define XM_MAX_PATTERNS                 256
#define XM_MAX_INSTRUMENTS              128
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

struct XmHeader {                   // the header of the xm file... global info.
  char              fileTag[17];    // = "Extended Module"
  char              songTitle[20];  // Name of the XM
  unsigned char     id;             // 0x1A
  char              trackerName[20];// = "FastTracker v2.00"
  unsigned short    version;        // 0x01 0x04
  unsigned          headerSize;     // size of header from here: min. 20 + 1 bytes
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

struct XmPatternHeader {            // typedef of the pattern header
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
    XmHeader                    *header;
    XmPatternHeader             *pattern;
    char                        *buf, *bufp;
    ifstream::pos_type          fileSize = 0;
    ifstream                    xmFile(fileName_, ios::in|ios::binary|ios::ate);
    
    isLoaded_ = false;
    // load file into byte buffer and then work on that buffer only
    if ( !xmFile.is_open() ) {
        std::cout << "can't open file";
        return 0; // exit on I/O error
    }
    fileSize = xmFile.tellg();
    buf = new char [(unsigned)fileSize];
    xmFile.seekg (0, ios::beg);
    xmFile.read (buf, fileSize);
    xmFile.close ();

    // now start reading from memory
    // ultra simple error checking
    header = (XmHeader *)buf;
    if (
        //(header->id != 0x1A                         ) ||  // removed for stripped xm compatibility
        //((header->version >> 8) != 1                ) ||  // removed for stripped xm compatibility
        (header->songLength     > XM_MAX_SONG_LENGTH) ||
        (header->nChannels      > XM_MAX_CHANNELS   ) ||
        (header->nInstruments   > XM_MAX_INSTRUMENTS) ||
        (header->nPatterns      > XM_MAX_PATTERNS)    ||
        (header->defaultBpm     > XM_MAX_BPM)         ||
        (header->defaultTempo   > XM_MAX_TEMPO)
        ) {
#ifdef debug_xm_loader
        char *hex = "0123456789ABCDEF";
        cout << "\nXm file header:";
        cout << "\nID                   = ";
        if (header->id > 0xF) cout << hex[(header->id >> 4) & 0xF];
        cout << hex[header->id & 0xF];
        cout << "\nVersion              = ";
        if (header->version > 0xFFF) cout << hex[(header->version >> 12) & 0xF];
        if (header->version > 0xFF ) cout << hex[(header->version >>  8) & 0xF] << ".";
        if (header->version > 0xF  ) cout << hex[(header->version >>  4) & 0xF];
        cout << hex[header->version & 0xF];
        cout << "\nSong length          = " << header->songLength;
        cout << "\nNr of channels       = " << header->nChannels;
        cout << "\nNr of instruments    = " << header->nInstruments;
        cout << "\nNr of Patterns       = " << header->nPatterns;
        cout << "\nDefault Bpm          = " << header->defaultBpm;
        cout << "\nDefault Tempo        = " << header->defaultTempo;
        cout << "\nXM MAX Tempo         = " << XM_MAX_TEMPO;
        cout << "\nError reading header, this is not an xm file.\n";
#endif
        delete [] buf;
        return 0;
    }
    trackerType_ = TRACKER_FT2;
    songTitle_ = "";
    for ( int i = 0; i < XM_MAX_SONG_NAME_LENGTH; i++ ) {
        songTitle_ += header->songTitle[i];
    }
    trackerTag_ = "";
    for ( int i = 0; i < XM_TRACKER_NAME_LENGTH; i++ ) {
        trackerTag_ += header->trackerName[i];
    }
    header->id              = 0; // use as zero terminator
    useLinearFrequencies_   = (bool)(header->flags & XM_LINEAR_FREQUENCIES_FLAG);
    isCustomRepeat_         = true;
    panningStyle_           = PANNING_STYLE_XM;
    nChannels_              = header->nChannels;
    nInstruments_           = header->nInstruments;
    nSamples_               = 0;
    nPatterns_              = header->nPatterns;
    defaultTempo_           = header->defaultTempo;
    defaultBpm_             = header->defaultBpm;
    songLength_             = header->songLength;
    songRestartPosition_    = header->songRestartPosition;
#ifdef debug_xm_loader
    cout << "\nXM Module title          = " << songTitle_.c_str();
    cout << "\nXM file Tracker ID       = " << trackerTag_.c_str();
    cout << "\n# Channels               = " << header->nChannels;
    cout << "\n# Instruments            = " << header->nInstruments;
    cout << "\n# Patterns               = " << header->nPatterns;
    cout << "\nDefault Tempo            = " << header->defaultTempo;
    cout << "\nDefault Bpm              = " << header->defaultBpm;
    cout << "\nSong Length              = " << header->songLength;
    cout << "\nSong restart position    = " << header->songRestartPosition;
    cout << "\nHeader Size              = " << header->headerSize;
    cout << "\nFrequency / period system: " << (useLinearFrequencies_ ? "Linear" : "Amiga");
    cout << "\nSize of XmHeader                     = " << sizeof(XmHeader);
    cout << "\nSize of XmPatternHeader              = " << sizeof(XmPatternHeader);
    cout << "\nSize of XmInstrumentHeaderPrimary    = " << sizeof(XmInstrumentHeaderPrimary);
    cout << "\nSize of XmEnvelopePoint              = " << sizeof(XmEnvelopePoint);
    cout << "\nSize of XmInstrumentHeaderSecondary  = " << sizeof(XmInstrumentHeaderSecondary);
    cout << "\nSize of XmSampleHeader               = " << sizeof(XmSampleHeader) << "\n";
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
        unsigned k = header->patternTable[i];
        if ((k + 1) > nPatterns_) k = nPatterns_; 
        patternTable_[i] = k;
    }
    // start reading the patterns
    //bufp = buf + sizeof(XmHeader);
    bufp = buf + XM_HEADER_SIZE_PART_ONE + header->headerSize;
    for (unsigned iPattern = 0; iPattern < nPatterns_; iPattern++) {
        Note        *iNote, *patternData;
        unsigned    pack, volumeColumn;

        pattern = (XmPatternHeader *)bufp;
#ifdef debug_xm_loader
#define SHOW_PATTERN_NO 0
        if (iPattern == (SHOW_PATTERN_NO + 1)) _getch();
        cout << "\nPattern # " << iPattern << ":";
        cout << "\nPattern Header Size (9) = " << pattern->headerSize;
        cout << "\nPattern # Rows          = " << pattern->nRows;
        cout << "\nPattern Pack system     = " << (unsigned)pattern->pack;
        cout << "\nPattern Data Size       = " << pattern->patternSize;
#endif
        if (/*pattern->pack ||*/ (pattern->nRows > XM_MAX_PATTERN_ROWS)) {
            delete [] buf;
            return 0;
        }  
        patterns_[iPattern] = new Pattern;
        patternData = new Note[nChannels_ * pattern->nRows];
        patterns_[iPattern]->Initialise(nChannels_, pattern->nRows, patternData);
        iNote = patternData;
        // below (memset) is done in pattern class
        //memset(patternData, 0, sizeof(Note) * nChannels_ * pattern->nRows);
        //bufp += sizeof(XmPatternHeader);
        bufp += pattern->headerSize;
        // empty patterns are not stored
        if (!pattern->patternSize) continue; // ?

        for (unsigned n = 0; n < (nChannels_ * pattern->nRows); n++) {
            pack = *((unsigned char *)bufp++);
            if (pack & XM_NOTE_IS_PACKED) {
                if (pack & XM_NOTE_AVAIL) {
                    iNote->note         = *((unsigned char *)bufp++);
#ifdef debug_xm_loader
                    if ( iNote->note > XM_KEY_OFF ) {
                        cout << "\nPattern # " << iPattern << ":";
                        cout << "\nPattern Header Size (9) = " << pattern->headerSize;
                        cout << "\nPattern # Rows          = " << pattern->nRows;
                        cout << "\nPattern Pack system     = " << (unsigned)pattern->pack;
                        cout << "\nPattern Data Size       = " << pattern->patternSize;
                        cout << "\nRow nr                  = " << n / pattern->nRows;
                        cout << "\nColumn nr               = " << n % pattern->nRows;
                        cout << "\nIllegal note            = " << iNote->note;
                        cout << "\n";
                        _getch();
                    }
#endif
                } 
                if (pack & XM_INSTRUMENT_AVAIL) {
                    iNote->instrument   = *((unsigned char *)bufp++);
                } 
                if (pack & XM_VOLUME_COLUMN_AVAIL) {
                    volumeColumn        = *((unsigned char *)bufp++);
                } else volumeColumn = 0;
                if (pack & XM_EFFECT_AVAIL) {
                    iNote->effects[1].effect = *((unsigned char *)bufp++);
                } 
                if (pack & XM_EFFECT_ARGUMENT_AVAIL) {
                    iNote->effects[1].argument = *((unsigned char *)bufp++);
                } 
            } else {
                iNote->note = pack;
#ifdef debug_xm_loader
                if ( iNote->note > XM_KEY_OFF ) {
                    cout << "\nPattern # " << iPattern << ":";
                    cout << "\nPattern Header Size (9) = " << pattern->headerSize;
                    cout << "\nPattern # Rows          = " << pattern->nRows;
                    cout << "\nPattern Pack system     = " << (unsigned)pattern->pack;
                    cout << "\nPattern Data Size       = " << pattern->patternSize;
                    cout << "\nRow nr                  = " << n / pattern->nRows;
                    cout << "\nColumn nr               = " << n % pattern->nRows;
                    cout << "\nIllegal note            = " << iNote->note;
                    cout << "\n";
                    _getch();
                }
#endif                
                iNote->instrument           = *((unsigned char *)bufp++);
                volumeColumn                = *((unsigned char *)bufp++);
                iNote->effects[1].effect    = *((unsigned char *)bufp++);
                iNote->effects[1].argument  = *((unsigned char *)bufp++);
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
                    case PANNING_SLIDE : 
                    {
                        if (volumeColumn > 0xE0) // slide right
                                iNote->effects[0].argument <<= 4;
                        break;
                    }
                    case SET_FINE_PANNING : // rough panning, really
                    {
                        iNote->effects[0].argument <<= 4; // *= 17; // ?
                        break;
                    }
                    case SET_VIBRATO_SPEED :
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
                    case VOLUME_SLIDE : 
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
                    case SET_GLOBAL_VOLUME : 
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
                        << std::setw( 2 ) << pattern->nRows << "|";
                } else if ( chn < 16 )
                {
                    std::cout << noteStrings[iNote->note] << "|";
                } //else std::cout << std::endl;
                std::cout << std::dec;
                /*
                char    hex[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

                if ((!(n % 5)) && (nChannels_ > 5)) cout << "\n";
                if (!(n % nChannels_)) cout << "\n";
                else cout << "|";
                cout << noteStrings[iNote->note];
                cout << ":";
                if (iNote->instrument < 10) cout << "0";
                cout << iNote->instrument;
                if (iNote->effects[0].effect < 0x10) cout << "0";
                cout << hex[iNote->effects[0].effect];
                cout << hex[iNote->effects[0].argument >> 4];
                cout << hex[iNote->effects[0].argument & 0xF];
                if (iNote->effects[1].effect < 0x10) cout << "0";
                cout << hex[iNote->effects[1].effect];
                cout << hex[iNote->effects[1].argument >> 4];
                cout << hex[iNote->effects[1].argument & 0xF];
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
    patterns_[nPatterns_]->Initialise(nChannels_, XM_MAX_PATTERN_ROWS, 
                                      new Note[nChannels_ * XM_MAX_PATTERN_ROWS]);

    // Now read all the instruments & sample data
    for (unsigned iInstrument = 0; iInstrument < nInstruments_; iInstrument++) {
        XmInstrumentHeaderPrimary   *instrumentHeader1;
        XmInstrumentHeaderSecondary *instrumentHeader2;
        InstrumentHeader            instrument;
        char                        instrumentName[XM_MAX_INSTRUMENT_NAME_LENGTH + 1];
        char                        *thisInstrument;
        
        thisInstrument = bufp;
        instrumentHeader1 = (XmInstrumentHeaderPrimary   *)bufp;

        if (instrumentHeader1->headerSize < sizeof(XmInstrumentHeaderPrimary)) {
            instrumentName[0] = '\0';
            instrument.nSamples = 0;
        } else {
            instrumentHeader2 = (XmInstrumentHeaderSecondary *)
                                (bufp + sizeof(XmInstrumentHeaderPrimary));
            for (int i = 0; i < XM_MAX_INSTRUMENT_NAME_LENGTH; i++) {
                instrumentName[i] = instrumentHeader1->name[i];
            }
            instrumentName[XM_MAX_INSTRUMENT_NAME_LENGTH] = '\0';
            instrument.nSamples = instrumentHeader1->nSamples;
        }
        bufp += instrumentHeader1->headerSize;
        instrument.name = instrumentName;

#ifdef debug_xm_loader
        cout << "\n\nInstrument header " << iInstrument << " size = " << instrumentHeader1->headerSize;
        cout << "\nInstrument name          = " << instrument.name.c_str();
        cout << "\nInstrument type (0)      = " << (int)(instrumentHeader1->type);
        cout << "\nNr of samples            = " << instrument.nSamples;
        if (instrument.nSamples)
            cout << "\nSample Header Size       = " << instrumentHeader2->sampleHeaderSize;          
#endif
        if (instrument.nSamples) { 
            unsigned        sampleOffset;
            SampleHeader    samples[MAX_SAMPLES];
            char            sampleNames[MAX_SAMPLES][XM_MAX_SAMPLE_NAME_LENGTH + 1];

            for (unsigned i = 0; i < MAXIMUM_NOTES; i++)
                instrument.sampleForNote[i] = instrumentHeader2->sampleForNote[i];
            for (unsigned i = 0; i < 12; i++) {
                instrument.volumeEnvelope [i].x = instrumentHeader2->volumeEnvelope [i].x;
                instrument.volumeEnvelope [i].y = instrumentHeader2->volumeEnvelope [i].y;
                instrument.panningEnvelope[i].x = instrumentHeader2->panningEnvelope[i].x;
                instrument.panningEnvelope[i].y = instrumentHeader2->panningEnvelope[i].y;
#ifdef debug_xm_loader
                cout << "\nEnveloppe point #" << i << ": "
                    << instrument.volumeEnvelope[i].x << "," 
                    << instrument.volumeEnvelope[i].y;
#endif
            }
            instrument.nVolumePoints    = instrumentHeader2->nVolumePoints;
            instrument.volumeSustain    = instrumentHeader2->volumeSustain;
            instrument.volumeLoopStart  = instrumentHeader2->volumeLoopStart;
            instrument.volumeLoopEnd    = instrumentHeader2->volumeLoopEnd;
            instrument.volumeType       = instrumentHeader2->volumeType;
            instrument.volumeFadeOut    = instrumentHeader2->volumeFadeOut;
            instrument.nPanningPoints   = instrumentHeader2->nPanningPoints;
            instrument.panningSustain   = instrumentHeader2->panningSustain;
            instrument.panningLoopStart = instrumentHeader2->panningLoopStart;
            instrument.panningLoopEnd   = instrumentHeader2->panningLoopEnd;
            instrument.panningType      = instrumentHeader2->panningType;
            instrument.vibratoType      = instrumentHeader2->vibratoType;
            instrument.vibratoSweep     = instrumentHeader2->vibratoSweep;
            instrument.vibratoDepth     = instrumentHeader2->vibratoDepth;
            instrument.vibratoRate      = instrumentHeader2->vibratoRate;
#ifdef debug_xm_loader
            cout << "\nSample header size for this instrument = ";
            cout << instrumentHeader2->sampleHeaderSize;
            cout << "\n";
            for (unsigned i = 0; i < MAXIMUM_NOTES; i++)
                cout << instrument.sampleForNote[i] << " ";
#endif
            for (unsigned iSample = 0; iSample < instrument.nSamples; iSample++) {
                XmSampleHeader  *xmSample = (XmSampleHeader *)bufp;
                bufp += instrumentHeader2->sampleHeaderSize;
                if (instrumentHeader2->sampleHeaderSize < sizeof(XmSampleHeader)) {
                    sampleNames[iSample][0] = '\0';
                } else {
                    for (unsigned i = 0; i < XM_MAX_SAMPLE_NAME_LENGTH; i++) {
                        sampleNames[iSample][i] = xmSample->name[i];                   
                    }
                    sampleNames[iSample][XM_MAX_SAMPLE_NAME_LENGTH] = '\0';
                }
                samples[iSample].name           = sampleNames[iSample];
                samples[iSample].finetune       = xmSample->finetune;
                samples[iSample].length         = xmSample->length;
                samples[iSample].repeatLength   = xmSample->repeatLength;
                samples[iSample].repeatOffset   = xmSample->repeatOffset;
                samples[iSample].isRepeatSample = (samples[iSample].repeatLength > 0) &&
                                                  (xmSample->type & XM_SAMPLE_LOOP_MASK);
                samples[iSample].volume         = xmSample->volume;
                samples[iSample].relativeNote   = xmSample->relativeNote;
                samples[iSample].panning        = xmSample->panning;
                samples[iSample].dataType       = 
                    ((xmSample->type & XM_SIXTEEN_BIT_SAMPLE_FLAG) ? 
                     SIGNED_SIXTEEN_BIT_SAMPLE : SIGNED_EIGHT_BIT_SAMPLE);
                samples[iSample].isPingpongSample = 
                    ((xmSample->type & XM_PINGPONG_LOOP_FLAG) ? true : false);
                if (xmSample->compression == XM_ADPCM_COMPRESSION) {
                    cout << "\n\nADPCM compressed sample data is not supported yet!\n";
                    delete [] buf;
                    return 0;
                }
#ifdef debug_xm_loader
                cout << "\n\nSample # " << iSample << ":";
                cout << "\nName             = " << samples[iSample].name.c_str();
                cout << "\nFinetune         = " << samples[iSample].finetune;
                cout << "\nLength           = " << samples[iSample].length;
                cout << "\nRepeatLength     = " << samples[iSample].repeatLength;
                cout << "\nRepeatOffset     = " << samples[iSample].repeatOffset;
                cout << "\nRepeatSample     = " << samples[iSample].isRepeatSample;
                cout << "\nVolume           = " << samples[iSample].volume;
                cout << "\nRelative Note    = " << samples[iSample].relativeNote;
                cout << "\nPanning          = " << samples[iSample].panning;
                cout << "\n16 bit sample    = " << 
                    ((samples[iSample].dataType == SIGNED_SIXTEEN_BIT_SAMPLE) ? "Yes" : "No");
                cout << "\nPing Loop active = " << 
                    (samples[iSample].isPingpongSample ? "Yes" : "No");
#endif
            }
            sampleOffset = 0;
            for (unsigned iSample = 0; iSample < instrument.nSamples; iSample++) {
                if (samples[iSample].length) {
                    SHORT           oldSample16 = 0;
                    SHORT           newSample16 = 0;
                    signed char     oldSample8 = 0;
                    signed char     newSample8 = 0;
                    nSamples_++;
                    samples[iSample].data = (SHORT *)(bufp + sampleOffset);                 
                    sampleOffset += samples[iSample].length;
                    if (samples[iSample].dataType == SIGNED_SIXTEEN_BIT_SAMPLE) {
                        SHORT   *ps = (SHORT *)samples[iSample].data;
                        SHORT   *pd = ps;
                        samples[iSample].length       >>= 1; 
                        samples[iSample].repeatLength >>= 1;
                        samples[iSample].repeatOffset >>= 1;
                        for (unsigned iData = 0; iData < samples[iSample].length; iData++) {
                            newSample16 = *ps++ + oldSample16;
                            *pd++       = newSample16;
                            oldSample16 = newSample16;
                        }
                    } else {
                        signed char *ps = (signed char *)(samples[iSample].data);
                        signed char *pd = ps;
#ifdef debug_xm_loader
#define SHOWNR 19
                        cout << "\nStart: " << (int)oldSample8 << "\n";
                        for (unsigned iData = 0; iData < SHOWNR; iData++) {
                            SHORT   t = (SHORT)ps[iData];
                            if (t >= 0) cout << " ";
                            if (abs(t) < 100) cout << " ";
                            if (abs(t) < 10 ) cout << " ";
                            cout << t;
                            //cout << " ";
                        }
                        cout << "\n";
#endif
                        for (unsigned iData = 0; iData < samples[iSample].length; iData++) {
                            newSample8 = *ps++ + oldSample8;
#ifdef debug_xm_loader
                            if (iData < SHOWNR) {
                                SHORT   t = newSample8;
                                if (t >= 0) cout << " ";
                                if (abs(t) < 100) cout << " ";
                                if (abs(t) < 10 ) cout << " ";
                                cout << t;
                                //cout << " ";
                            } 
                            if (iData == SHOWNR) cout << "\n";
                            /*
                            if      (newSample8 < -128) { cout << " " << newSample8; *pd++ = -128; }
                            else if (newSample8 >  127) { cout << " " << newSample8; *pd++ =  127; }
                            else */
#endif
                            *pd++      = newSample8; 
                            oldSample8 = newSample8;
                        }
                    }
                    instrument.samples[iSample] = new Sample;
                    instrument.samples[iSample]->load(samples[iSample]);
#ifdef debug_xm_loader
                    cout << "\n"; 
                    // _getch();
#endif
                } else {
                    samples[iSample].data = 0;
                }
            }
            bufp += sampleOffset;
#ifdef debug_xm_loader
#ifdef debug_xm_play_samples
            for (unsigned iSample = 0; iSample < instrument.nSamples; iSample++) {
                if (samples[iSample].length) {
                    HWAVEOUT        hWaveOut;
                    WAVEFORMATEX    waveFormatEx;
                    MMRESULT        result;
                    WAVEHDR         waveHdr;
//                    cout << "\nFile size                = " << fileSize;
//                    cout << "\nFile offset              = " << (unsigned)(bufp - buf);
//                    cout << "\nSample data offset       = " << (unsigned)((char *)samples[iSample].data - buf);
//                    cout << "\nSample length (bytes)    = " << samples[iSample].length;
//                    cout << "\nSample length (samples)  = " << samples[iSample].length;
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
                        if (!iInstrument) cout << "\nError opening wave mapper!\n";
                    } else {
                        int retry = 0;
                        if (!iInstrument) cout << "\nWave mapper successfully opened!\n";
                        cout << "\nPlaying sample # " << iSample;
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
                            cout << "\nError preparing wave mapper header!";
                            switch (result) {
                                case MMSYSERR_INVALHANDLE : 
                                    { 
                                    cout << "\nSpecified device handle is invalid.";
                                    break;
                                    }
                                case MMSYSERR_NODRIVER    : 
                                    {
                                    cout << "\nNo device driver is present.";
                                    break;
                                    }
                                case MMSYSERR_NOMEM       : 
                                    {
                                    cout << "\nUnable to allocate or lock memory.";
                                    break;
                                    }
                                default:
                                    {
                                    cout << "\nOther unknown error " << result;
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
                            cout << "\nError writing to wave mapper!";
                            switch (result) {
                                case MMSYSERR_INVALHANDLE : 
                                    { 
                                    cout << "\nSpecified device handle is invalid.";
                                    break;
                                    }
                                case MMSYSERR_NODRIVER    : 
                                    {
                                    cout << "\nNo device driver is present.";
                                    break;
                                    }
                                case MMSYSERR_NOMEM       : 
                                    {
                                    cout << "\nUnable to allocate or lock memory.";
                                    break;
                                    }
                                case WAVERR_UNPREPARED    : 
                                    {
                                    cout << "\nThe data block pointed to by the pwh parameter hasn't been prepared.";
                                    break;
                                    }
                                default:
                                    {
                                    cout << "\nOther unknown error " << result;
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
        instruments_[iInstrument]->load(instrument);        
    }
    isLoaded_ = true;
    delete [] buf;
    return 0;
}
