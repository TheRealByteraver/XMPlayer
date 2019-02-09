/*
Thanks must go to:
- PSI (Sami Tammilehto) / Future Crew for creating Scream Tracker 3
- FireLight / Brett Paterson for writing a detailed document explaining how to
  parse .s3m files. Without fs3mdoc.txt this would have been hell. With his
  invaluable document, it was a hell of a lot easier.
*/

#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>

#include "Module.h"

//#define debug_s3m_loader  
//#define debug_s3m_show_patterns
//#define debug_s3m_play_samples


#ifdef debug_s3m_loader
#include <bitset>
#include <iomanip>
#endif

#define S3M_MIN_FILESIZE                    (sizeof(S3mFileHeader) + \
                                             sizeof(S3mInstHeader) + 64 + 16)
#define S3M_MAX_CHANNELS                    32
#define S3M_MAX_INSTRUMENTS                 100
#define S3M_CHN_UNUSED                      255
#define S3M_DEFAULT_PAN_LEFT                48
#define S3M_DEFAULT_PAN_RIGHT               207
#define S3M_DEFAULT_PAN_CENTER              128
#define S3M_MAX_SONGTITLE_LENGTH            28
#define S3M_MAX_SAMPLENAME_LENGTH           28
#define S3M_MAX_VOLUME                      64
#define S3M_TRACKER_TAG_LENGTH              4
#define S3M_MARKER_PATTERN                  254
#define S3M_END_OF_SONG_MARKER              255
#define S3M_ST2VIBRATO_FLAG                 1
#define S3M_ST2TEMPO_FLAG                   2
#define S3M_AMIGASLIDES_FLAG                4
#define S3M_VOLUME_ZERO_OPT_FLAG            8
#define S3M_AMIGA_LIMITS_FLAG               16
#define S3M_FILTER_ENABLE_FLAG              32
#define S3M_ST300_VOLSLIDES_FLAG            64
#define S3M_CUSTOM_DATA_FLAG                128
#define S3M_TRACKER_MASK                    0xF000
#define S3M_TRACKER_VERSION_MASK            0x0FFF
#define S3M_SIGNED_SAMPLE_DATA              1
#define S3M_UNSIGNED_SAMPLE_DATA            2
#define S3M_STEREO_FLAG                     128
#define S3M_DEFAULT_PANNING_PRESENT         0xFC
#define S3M_SAMPLE_NOTPACKED_FLAG           0
#define S3M_SAMPLE_PACKED_FLAG              1    // DP30ADPCM, not supported
#define S3M_SAMPLE_LOOP_FLAG                1
#define S3M_SAMPLE_STEREO_FLAG              2    // not supported
#define S3M_SAMPLE_16BIT_FLAG               4    // not supported
#define S3M_ROWS_PER_PATTERN                64
#define S3M_PTN_END_OF_ROW_MARKER           0
#define S3M_PTN_CHANNEL_MASK                31
#define S3M_PTN_NOTE_INST_FLAG              32
#define S3M_PTN_VOLUME_COLUMN_FLAG          64
#define S3M_PTN_EFFECT_PARAM_FLAG           128
#define S3M_KEY_OFF                         254
#define S3M_MAX_NOTE                        (9 * 12)
#define S3M_INSTRUMENT_TYPE_SAMPLE          1
#define S3M_INSTRUMENT_TYPE_ADLIB_MELODY    2
#define S3M_INSTRUMENT_TYPE_ADLIB_DRUM      3


#pragma pack (1) 
struct S3mFileHeader {
    char            songTitle[S3M_MAX_SONGTITLE_LENGTH];
    unsigned char   id;             // 0x1A
    unsigned char   fileType;       // 0x1D for ScreamTracker 3
    unsigned short  reserved1;
    unsigned short  songLength;     // 
    unsigned short  nInstruments;
    unsigned short  nPatterns;      // including marker (unsaved) patterns
    unsigned short  flags;          // CWTV == 1320h -> ST3.20
    unsigned short  CWTV;           // Created with tracker / version
    unsigned short  sampleDataType; // 1 = signed, 2 = unsigned
    char            tag[4];         // SCRM
    unsigned char   globalVolume;   // maximum volume == 64
    unsigned char   defaultTempo;   // default == 6
    unsigned char   defaultBpm;     // default == 125
    unsigned char   masterVolume;   // SB PRO master vol, stereo flag
    unsigned char   gusClickRemoval;// probably nChannels * 2, min = 16?
    unsigned char   useDefaultPanning;// == 0xFC if def. chn pan pos. are present
    unsigned char   reserved2[8];
    unsigned short  customDataPointer;
    unsigned char   channelsEnabled[32];
};

struct S3mInstHeader {
    unsigned char   sampleType;
    char            dosFilename[12];// 12, not 13!
    unsigned char   memSeg;
    unsigned short  memOfs;
    unsigned        length;
    unsigned        loopStart;
    unsigned        loopEnd;
    unsigned char   volume;
    unsigned char   reserved;       // should be 0x1D
    unsigned char   packId;         // should be 0
    unsigned char   flags;
    unsigned        c4Speed;
    unsigned char   reserved2[12];  // useless info for us
    char            name[S3M_MAX_SAMPLENAME_LENGTH];
    char            tag[4];         // "SCRS"
};

struct S3mUnpackedNote {
    unsigned char note;
    unsigned char inst;
    unsigned char volc;
    unsigned char fx;
    unsigned char fxp;
};
#pragma pack (8) 

#ifdef debug_s3m_loader
const char *notetable[] = { 
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
#endif

int Module::loadS3mFile() {
    char                    *buf;
    std::ifstream::pos_type  fileSize = 0;
    std::ifstream            s3mFile( 
        fileName_,std::ios::in | std::ios::binary | std::ios::ate );
    // initialize s3m specific variables:
    minPeriod_ = 56;    // periods[9 * 12 - 1]
    maxPeriod_ = 27392; // periods[0]
    // load file into byte buffer and then work on that buffer only
    isLoaded_ = false;
    if ( !s3mFile.is_open() ) return 0; // exit on I/O error
    fileSize = s3mFile.tellg();
    buf = new char[(int)fileSize];
    s3mFile.seekg( 0,std::ios::beg );
    s3mFile.read( buf,fileSize );
    s3mFile.close();
    S3mFileHeader& s3mFileHeader = *((S3mFileHeader *)buf);
    // some very basic checking
    if ( (fileSize < S3M_MIN_FILESIZE) ||
         ( ! ((s3mFileHeader.tag[0] == 'S') &&
              (s3mFileHeader.tag[1] == 'C') &&
              (s3mFileHeader.tag[2] == 'R') &&
              (s3mFileHeader.tag[3] == 'M'))
          ) 
        // || (s3mFileHeader.id != 0x1A)
        || (s3mFileHeader.sampleDataType < 1)
        || (s3mFileHeader.sampleDataType > 2)
        ) { 
#ifdef debug_s3m_loader
        std::cout << std::endl
            << "SCRM tag not found or file is too small, exiting.";
#endif
        delete[] buf;
        return 0;
    }
    s3mFileHeader.id = '\0'; // use the DOS EOF marker as end of cstring marker    
#ifdef debug_s3m_loader
    std::cout << std::endl
        << "header info:" << std::endl
        << "songTitle:          " << s3mFileHeader.songTitle << std::endl
        << "file type:          " << std::hex << (int)s3mFileHeader.fileType << std::endl
        << "songLength:         " << std::dec << (int)s3mFileHeader.songLength << std::endl
        << "# Instruments:      " << (int)s3mFileHeader.nInstruments << std::endl
        << "# Patterns:         " << (int)s3mFileHeader.nPatterns << std::endl
        << "Flags:              " << std::bitset<16>( s3mFileHeader.flags ) << std::endl
        << "made w/ (CWTV):     " << std::hex << s3mFileHeader.CWTV << std::endl
        << "sampleDataType:     ";
    if ( (int)s3mFileHeader.sampleDataType == S3M_UNSIGNED_SAMPLE_DATA )
        std::cout << "unsigned (standard)";
    else if ( (int)s3mFileHeader.sampleDataType == S3M_SIGNED_SAMPLE_DATA )
        std::cout << "signed (non-standard)";
    else std::cout << "illegal value: error in file!";
    std::cout << std::endl
        << "globalVolume:       " << std::dec << (int)s3mFileHeader.globalVolume << std::endl
        << "defaultTempo:       " << (int)s3mFileHeader.defaultTempo << std::endl
        << "defaultBpm:         " << (int)s3mFileHeader.defaultBpm << std::endl
        << "masterVolume:       " << (int)s3mFileHeader.masterVolume << std::endl
        << "gusClickRemoval:    " << (int)s3mFileHeader.gusClickRemoval << std::endl
        << "useDefaultPanning:  " << (int)s3mFileHeader.useDefaultPanning << std::endl
        << "customDataPointer:  " << (int)s3mFileHeader.customDataPointer << std::endl
        << "channelsEnabled:    " << std::dec << std::endl;
    for ( int chn = 0; chn < 32; chn++ )
    {
        std::cout << std::setw( 2 ) << chn << ":" 
            << std::setw( 3 ) << (int)s3mFileHeader.channelsEnabled[chn] << " ";
        if ( (chn + 1) % 8 == 0 ) std::cout << std::endl;
    }
    std::cout << std::endl;
#endif
    if ( (s3mFileHeader.CWTV == 0x1300) ||
        (s3mFileHeader.flags & S3M_ST300_VOLSLIDES_FLAG) )
            trackerType_ = TRACKER_ST300;
    else    trackerType_ = TRACKER_ST321;
    nChannels_ = 0;
    int chnRemap[S3M_MAX_CHANNELS];
    int chnBackmap[S3M_MAX_CHANNELS];
    int chnPanVals[S3M_MAX_CHANNELS];
    for ( int chn = 0; chn < S3M_MAX_CHANNELS; chn++ )
    {
        chnBackmap[chn] = S3M_CHN_UNUSED;
        chnPanVals[chn] = S3M_DEFAULT_PAN_CENTER;
        int chnInfo = (int)s3mFileHeader.channelsEnabled[chn];
        if ( chnInfo < 16 ) // channel is used! // x64 FT2 detects weird #chn
        {
            chnBackmap[nChannels_] = chn;
            chnRemap[chn] = nChannels_;
            if ( chnInfo < 7 )  chnPanVals[nChannels_] = S3M_DEFAULT_PAN_LEFT;
            else                chnPanVals[nChannels_] = S3M_DEFAULT_PAN_RIGHT;
            nChannels_++;
        } else { 
            chnRemap[chn] = S3M_CHN_UNUSED;
        }
    }
#ifdef debug_s3m_loader
    std::cout << std::endl
        << "# channels:         " << std::dec << nChannels_;
    std::cout << std::endl << "Order list: ";
#endif
    for ( int i = 0; i < S3M_MAX_SONGTITLE_LENGTH; i++ ) {
        songTitle_ += s3mFileHeader.songTitle[i];
    }
    for ( int i = 0; i < S3M_TRACKER_TAG_LENGTH; i++ ) {
        trackerTag_ += s3mFileHeader.tag[i];
    }
    useLinearFrequencies_ = false;   // S3M uses AMIGA periods
    songRestartPosition_ = 0;
    isCustomRepeat_ = false;
    panningStyle_ = PANNING_STYLE_S3M; 
    nInstruments_ = s3mFileHeader.nInstruments;
    nSamples_ = 0;
    defaultTempo_ = s3mFileHeader.defaultTempo;
    defaultBpm_ = s3mFileHeader.defaultBpm;
    if ( defaultTempo_ == 0 || defaultTempo_ == 255 ) defaultTempo_ = 6;
    if ( defaultBpm_ < 33 ) defaultBpm_ = 125;
    // Read in the Pattern order table:
    memset( patternTable_,0,sizeof( *patternTable_ ) * MAX_PATTERNS );
    nPatterns_ = 0;
    songLength_ = 0;
    int bufOffset = sizeof( S3mFileHeader );
    unsigned char *OrderList = (unsigned char *)(buf + bufOffset);
    /*
    for ( int i = 0; i < s3mFileHeader.songLength; i++ )
    {
        unsigned order = OrderList[i];
        if ( order < S3M_MARKER_PATTERN ) { 
            patternTable_[songLength_] = order;
            songLength_++;
            if (order > nPatterns_) nPatterns_ = order;

#ifdef debug_s3m_loader
            std::cout << order << " ";
#endif
        }
    }
    */
    for ( int i = 0; i < s3mFileHeader.songLength; i++ )
    {
        unsigned order = OrderList[i];
        if ( order >= S3M_MARKER_PATTERN ) order = MARKER_PATTERN;
        else if ( order > nPatterns_ ) nPatterns_ = order;
        patternTable_[songLength_] = order;
        songLength_++;        
#ifdef debug_s3m_loader
        std::cout << order << " ";
#endif
        
    }
    nPatterns_++;
#ifdef debug_s3m_loader
    std::cout << std::endl
        << "SongLength (corr.): " << songLength_ << std::endl
        << "Number of Patterns: " << nPatterns_ << std::endl;
#endif
    bufOffset += s3mFileHeader.songLength;
    const unsigned short *instrParaPtrs = (unsigned short *)(buf + bufOffset);
    bufOffset += s3mFileHeader.nInstruments * 2; // sizeof( unsigned short ) == 2
    const unsigned short *ptnParaPtrs = (unsigned short *)(buf + bufOffset);
    bufOffset += s3mFileHeader.nPatterns    * 2; // sizeof( unsigned short ) == 2
    const unsigned char *defPanPositions = (unsigned char *)(buf + bufOffset); // 32 bytes
    if ( (bufOffset + 32) > fileSize ) { 
#ifdef debug_s3m_loader
        std::cout << std::endl
            << "File is too small, exiting.";
#endif
        delete[] buf;
        return 0;
    }
    // to be reviewed (ugly code) --------------------------
    if ( (int)s3mFileHeader.masterVolume & S3M_STEREO_FLAG )
    {
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defaultPanPositions_[i] = (defPanPositions[i] & 0xF) * 17; // convert to 8 bit pan
    } else {
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defaultPanPositions_[i] = S3M_DEFAULT_PAN_CENTER;
    }
    if ( s3mFileHeader.useDefaultPanning == S3M_DEFAULT_PANNING_PRESENT )
    {
        for ( unsigned i = 0; i < nChannels_; i++ )
        {                       
            chnPanVals[i] = defaultPanPositions_[chnBackmap[i]];
        }
    }    
    for ( unsigned i = 0; i < nChannels_; i++ )
    {
        defaultPanPositions_[i] = chnPanVals[i];
    }
    // end "to be reviewed" marker -------------------------
#ifdef debug_s3m_loader
    std::cout << std::endl << "Instrument pointers: ";
    for ( int i = 0; i < s3mFileHeader.nInstruments; i++ )
    {
        std::cout << instrParaPtrs[i] << " ";
    }
    std::cout << std::endl << "Pattern pointers: ";
    for ( int i = 0; i < s3mFileHeader.nPatterns; i++ )
    {
        std::cout << ptnParaPtrs[i] << " ";
    }
    std::cout << std::endl << "Use default panning from file: "
        << ((s3mFileHeader.useDefaultPanning == S3M_DEFAULT_PANNING_PRESENT) ?
        "yes" : "no");
    std::cout << std::endl << "Default panning positions: ";
    for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
    {
        std::cout << ((int)defPanPositions[i]) << " ";
    }
    std::cout << std::endl;
    if ( ((int)s3mFileHeader.masterVolume & S3M_STEREO_FLAG) == 0 )
            std::cout << "S3M file is in mono mode.";
    else    std::cout << "S3M file is in stereo mode.";
    std::cout << std::endl << "Final panning positions: ";
    for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
    {
        std::cout << ((int)chnPanVals[i]) << " ";
    }
    std::cout << std::endl;
#endif
    // read instruments here
    nSamples_ = 0;
    int smpDataPtrs[S3M_MAX_INSTRUMENTS];
    if ( s3mFileHeader.nInstruments > S3M_MAX_INSTRUMENTS )
        s3mFileHeader.nInstruments = S3M_MAX_INSTRUMENTS;
    for ( int nInst = 0; nInst < s3mFileHeader.nInstruments; nInst++ )
    {
        bufOffset = 16 * instrParaPtrs[nInst];
        S3mInstHeader& s3mInstHeader = *((S3mInstHeader *)(buf + bufOffset));
        smpDataPtrs[nInst] = 
            16 * (((int)s3mInstHeader.memSeg << 16) + (int)s3mInstHeader.memOfs);
        if ( !s3mInstHeader.c4Speed ) s3mInstHeader.c4Speed = (unsigned)NTSC_C4_SPEED;
#ifdef debug_s3m_loader
        s3mInstHeader.name[S3M_MAX_SAMPLENAME_LENGTH - 1] = '\0'; // debug only
        s3mInstHeader.tag[3] = '\0';                              // debug only
        std::cout << std::endl
            << "Instrument nr " << nInst << " info: " << std::endl
            << "Sample Type:        ";
        if ( s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_SAMPLE )
            std::cout << "digital sample";
        else if ( s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_ADLIB_MELODY )
            std::cout << "adlib melody";
        else if ( s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_ADLIB_DRUM )
            std::cout << "adlib drum";
        std::cout << std::endl
            << "Sample location:    "
            << (int)s3mInstHeader.memSeg << ":"
            << (int)s3mInstHeader.memOfs << " " << std::endl;
        s3mInstHeader.memSeg = '\0'; // so we can read the DOS file name
        std::cout
            << "Sample location:    " << smpDataPtrs[nInst] << std::endl
            << "Length:             " << s3mInstHeader.length << std::endl
            << "Loop Start:         " << s3mInstHeader.loopStart << std::endl
            << "Loop End:           " << s3mInstHeader.loopEnd << std::endl
            << "Volume:             " << (int)s3mInstHeader.volume << std::endl
            << "0x1D:               0x" << std::hex << (int)s3mInstHeader.reserved << std::endl << std::dec
            << "pack ID:            " << (int)s3mInstHeader.packId << std::endl
            << "flags:              " << (int)s3mInstHeader.flags << std::endl
            << "C2SPD:              " << s3mInstHeader.c4Speed << std::endl
            << "Sample DOS Name:    " << s3mInstHeader.dosFilename << std::endl
            << "Name:               " << s3mInstHeader.name << std::endl
            << "Tag:                " << s3mInstHeader.tag << std::endl;
#endif
        if ( (s3mInstHeader.sampleType != 0) &&
            (s3mInstHeader.sampleType != S3M_INSTRUMENT_TYPE_SAMPLE) ) {
#ifdef debug_s3m_loader
            // exit on error disabled for 2nd_pm.s3m
            std::cout << std::endl
                << "Unable to read adlib samples!"/*", exiting!"*/;
#endif
            //delete[] buf;
            //return 0;
        }
        InstrumentHeader    instrument;
        SampleHeader        sample;
        for ( int i = 0; i < S3M_MAX_SAMPLENAME_LENGTH; i++ )
            sample.name += s3mInstHeader.name[i];
        instrument.name = sample.name;
        sample.length = s3mInstHeader.length;
        sample.repeatOffset = s3mInstHeader.loopStart;
        sample.volume = (int)s3mInstHeader.volume;
        //sample.c4Speed = instHeader.c4Speed;
        // safety checks:
        if ( s3mInstHeader.loopEnd >= s3mInstHeader.loopStart )
            sample.repeatLength = s3mInstHeader.loopEnd - s3mInstHeader.loopStart + 1;
        else sample.repeatLength = sample.length;
        if ( sample.volume > S3M_MAX_VOLUME ) sample.volume = S3M_MAX_VOLUME;
        if ( sample.repeatOffset >= sample.length ) sample.repeatOffset = 0;
        if ( sample.repeatOffset + sample.repeatLength > sample.length )
            sample.repeatLength = sample.length - sample.repeatOffset;
        sample.isRepeatSample = (s3mInstHeader.flags & S3M_SAMPLE_LOOP_FLAG) != 0;
        if ( (s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_SAMPLE) &&
            (smpDataPtrs[nInst] != 0)) {
            nSamples_++;
            sample.data = (SHORT *)(buf + smpDataPtrs[nInst]);
            if ( (char *)((char *)sample.data + sample.length) > (buf + (unsigned)fileSize) )
            {
#ifdef debug_s3m_loader
                std::cout << std::endl
                    << "Missing sample data while reading file, exiting!" << std::endl
                    << "sample.data:   " << std::dec << (unsigned)sample.data << std::endl
                    << "sample.length: " << sample.length << std::endl
                    << "data + length: " << (unsigned)(sample.data + sample.length) << std::endl
                    << "filesize:      " << fileSize << std::endl
                    << "overshoot:     "
                    << (unsigned)((char *)(sample.data + sample.length) - (buf + (unsigned)fileSize))
                    << std::endl;
#endif
                delete[] buf;
                return 0;
            }
            instrument.samples[0] = new Sample;
            sample.dataType = SIGNED_EIGHT_BIT_SAMPLE;
            // convert sample data from unsigned to signed:
            if ( s3mFileHeader.sampleDataType == S3M_UNSIGNED_SAMPLE_DATA ) {
                unsigned char *s = (unsigned char *)sample.data;
                for ( unsigned i = 0; i < sample.length; i++ ) *s++ ^= 128;
            }            
            // finetune + relative note recalc
            unsigned int s3mPeriod = ((unsigned)8363 * periods[4 * 12]) / s3mInstHeader.c4Speed;
            unsigned j;
            for ( j = 0; j < MAXIMUM_NOTES; j++ ) {
                if ( s3mPeriod >= periods[j] ) break;
            }
            if ( j < MAXIMUM_NOTES ) {
                sample.relativeNote = j - (4 * 12);
                sample.finetune = (int)round(
                    ((double)(133 - j) - 12.0 * log2( (double)s3mPeriod / 13.375 ))
                     * 128.0) - 128;
            } else { 
                sample.relativeNote = 0;
                sample.finetune = 0;
            }
#ifdef debug_s3m_loader
            std::cout 
                << "relative note: " 
                << notetable[4 * 12 + sample.relativeNote] << std::endl
                << "finetune:      " << sample.finetune
                << std::endl;
#endif
            instrument.samples[0]->load( sample );
        }
        instruments_[nInst] = new Instrument;
        instruments_[nInst]->load( instrument );
#ifdef debug_s3m_loader
#ifdef debug_s3m_play_samples
        std::cout << "\nSample " << nInst << ": name     = " << instruments_[nInst]->getName();
        if ( !instruments_[nInst]->getSample( 0 ) ) _getch();

        if ( instruments_[nInst]->getSample( 0 ) ) {
            HWAVEOUT        hWaveOut;
            WAVEFORMATEX    waveFormatEx;
            MMRESULT        result;
            WAVEHDR         waveHdr;

            std::cout << "\nSample " << nInst << ": length   = " << instruments_[nInst]->getSample( 0 )->getLength();
            std::cout << "\nSample " << nInst << ": rep ofs  = " << instruments_[nInst]->getSample( 0 )->getRepeatOffset();
            std::cout << "\nSample " << nInst << ": rep len  = " << instruments_[nInst]->getSample( 0 )->getRepeatLength();
            std::cout << "\nSample " << nInst << ": volume   = " << instruments_[nInst]->getSample( 0 )->getVolume();
            std::cout << "\nSample " << nInst << ": finetune = " << instruments_[nInst]->getSample( 0 )->getFinetune();

            // not very elegant but hey, is debug code lol
            if ( !instruments_[nInst]->getSample( 0 )->getData() ) break;

            waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
            waveFormatEx.nChannels = 1;
            waveFormatEx.nSamplesPerSec = 16000; // frequency
            waveFormatEx.wBitsPerSample = 16;
            waveFormatEx.nBlockAlign = waveFormatEx.nChannels *
                (waveFormatEx.wBitsPerSample >> 3);
            waveFormatEx.nAvgBytesPerSec = waveFormatEx.nSamplesPerSec *
                waveFormatEx.nBlockAlign;
            waveFormatEx.cbSize = 0;

            result = waveOutOpen( &hWaveOut,WAVE_MAPPER,&waveFormatEx,
                0,0,CALLBACK_NULL );
            if ( result != MMSYSERR_NOERROR ) {
                if ( !nInst ) std::cout << "\nError opening wave mapper!\n";
            } else {
                int retry = 0;
                if ( !nInst ) std::cout << "\nWave mapper successfully opened!\n";
                waveHdr.dwBufferLength = instruments_[nInst]->getSample( 0 )->getLength() *
                    waveFormatEx.nBlockAlign;
                waveHdr.lpData = (LPSTR)(instruments_[nInst]->getSample( 0 )->getData());
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
                
                //while(waveOutUnprepareHeader(hWaveOut, &waveHdr,
                //sizeof(WAVEHDR)) == WAVERR_STILLPLAYING)
                //{
                //Sleep(50);
                //}
                
                waveOutReset( hWaveOut );
                waveOutClose( hWaveOut );
            }
        }
#endif
#endif
    }
    // Now load the patterns:
    S3mUnpackedNote *unPackedPtn = new S3mUnpackedNote[S3M_ROWS_PER_PATTERN * nChannels_];
    for ( unsigned iPtn = 0; iPtn < /*s3mFileHeader.nPatterns*/nPatterns_; iPtn++ )
    {
        memset( unPackedPtn,0,S3M_ROWS_PER_PATTERN * nChannels_ * sizeof( S3mUnpackedNote ) );
        char *s3mPtn = buf + (unsigned)16 * (unsigned)(ptnParaPtrs[iPtn]);
        unsigned packedSize = *((unsigned short *)s3mPtn);
        if( (s3mPtn + packedSize) > ( buf + (unsigned)fileSize ) )
        {
#ifdef debug_s3m_loader
            std::cout << std::endl
                << "Missing pattern data while reading file, exiting!";
#endif
            delete[] unPackedPtn;
            delete[] buf;
            return 0;
        }
        unsigned char *ptnPtr = (unsigned char *)s3mPtn + 2;
        int row = 0;
        for ( ; (ptnPtr < (unsigned char *)s3mPtn + packedSize) &&
                (row < S3M_ROWS_PER_PATTERN); ) {
            char pack = *ptnPtr++;
            if ( pack == S3M_PTN_END_OF_ROW_MARKER ) row++;                
            else { 
                unsigned chn = chnRemap[pack & S3M_PTN_CHANNEL_MASK]; // may return 255               
                unsigned char note = 0;
                unsigned char inst = 0;
                unsigned char volc = 0;
                unsigned char fx   = 0;
                unsigned char fxp  = 0;
                bool newNote = false;
                if ( pack & S3M_PTN_NOTE_INST_FLAG )
                {
                    newNote = true;
                    note = *ptnPtr++;
                    inst = *ptnPtr++;
                }
                // we add 0x10 to the volume column so we now an effect is there
                if ( pack & S3M_PTN_VOLUME_COLUMN_FLAG ) volc = 0x10 + *ptnPtr++;
                if ( pack & S3M_PTN_EFFECT_PARAM_FLAG )
                {
                    fx = *ptnPtr++;
                    fxp = *ptnPtr++;
                }
                if ( chn < nChannels_ )
                {
                    S3mUnpackedNote& unpackedNote = unPackedPtn[row * nChannels_ + chn];
                    if ( newNote )
                    {
                        if ( note == S3M_KEY_OFF ) unpackedNote.note = KEY_OFF;
                        else if ( note != 0xFF ) 
                        {
                            unpackedNote.note = (note >> 4) * 12 + (note & 0xF) + 1;
                            if ( unpackedNote.note > S3M_MAX_NOTE )
                                unpackedNote.note = 0;
                        } else note = 0; // added: 0 or 255 means no note
                    }
                    unpackedNote.inst = inst;
                    unpackedNote.volc = volc;
                    unpackedNote.fx = fx;
                    unpackedNote.fxp = fxp;
                }                
            }            
        }
#ifdef debug_s3m_loader
#ifdef debug_s3m_show_patterns
        std::cout << std::dec << std::endl << "Pattern nr " << iPtn << ":" << std::endl;
        for ( int row = 0; row < S3M_ROWS_PER_PATTERN; row++ )
        {
            std::cout << std::endl << std::hex << std::setw( 2 ) << row << ":" << std::dec;
            for ( unsigned chn = 0; chn < nChannels_; chn++ )
                if ( chn < 8 )
                {
                    UnpackedNote& unpackedNote = unPackedPtn[row * nChannels_ + chn];
                    if ( unpackedNote.note ) {
                        if( unpackedNote.note < (12 * 11) )
                            std::cout << notetable[unpackedNote.note];
                        else if ( unpackedNote.note == KEY_OFF ) std::cout << "===";
                             else std::cout << "!!!";

                    } else std::cout << "---";
                    //std::cout << "." << std::setw( 3 ) << (int)unpackedNote.note;                    
                    // std::cout << std::hex;
                    // if ( unpackedNote.inst ) std::cout << std::setw( 2 ) << (int)unpackedNote.inst;
                    // else  std::cout << "  ";
                    // if ( unpackedNote.volc ) std::cout << std::setw( 2 ) << (int)unpackedNote.volc;
                    // else  std::cout << "  ";
                    // if ( unpackedNote.fx ) std::cout << std::setw( 2 ) << (int)unpackedNote.fx;
                    // else  std::cout << "  ";
                    // if ( unpackedNote.fxp ) std::cout << std::setw( 2 ) << (int)unpackedNote.fxp;
                    // else  
                    //    std::cout << "  ";

                    /*
                    if ( chn == 15 ) { 
                        std::cout << std::hex;
                        if ( unpackedNote.volc ) std::cout << std::setw( 2 ) << (int)unpackedNote.volc;
                        else  std::cout << "  "; 
                        std::cout << "  ";
                        if ( unpackedNote.fx ) std::cout << std::setw( 2 ) << (int)unpackedNote.fx;
                        else  std::cout << "  ";
                        std::cout << "  ";
                        if ( unpackedNote.fxp ) std::cout << std::setw( 2 ) << (int)unpackedNote.fxp;
                        else std::cout << "  ";
                    }
                    */
                    std::cout << "|";
                }
        } 
#endif
#endif
        // read the pattern into the internal format:
        Note        *iNote,*patternData;
        S3mUnpackedNote* unPackedNote = unPackedPtn;
        patterns_[iPtn] = new Pattern;
        patternData = new Note[nChannels_ * S3M_ROWS_PER_PATTERN];
        patterns_[iPtn]->Initialise( nChannels_,S3M_ROWS_PER_PATTERN,patternData );
        iNote = patternData;
        for ( unsigned n = 0; n < (nChannels_ * S3M_ROWS_PER_PATTERN); n++ ) {
            iNote->note = unPackedNote->note;
            iNote->instrument = unPackedNote->inst;

            if ( unPackedNote->volc )
            {
                unPackedNote->volc -= 0x10;
                if ( unPackedNote->volc <= S3M_MAX_VOLUME )
                {
                    iNote->effects[0].effect = SET_VOLUME;
                    iNote->effects[0].argument = unPackedNote->volc;
                } else {
                    if ( unPackedNote->volc >= 128 &&
                         unPackedNote->volc <= 192 )
                    {
                        iNote->effects[0].effect = SET_FINE_PANNING;
                        iNote->effects[0].argument =
                            (unPackedNote->volc - 128) << 4;
                        if ( iNote->effects[0].argument > 0xFF )
                            iNote->effects[0].argument = 0xFF;
                    } 
                }
            }
            /*
                S3M effect A = 1, B = 2, etc
                do some error checking & effect remapping:
            */
            iNote->effects[1].argument = unPackedNote->fxp;
            switch ( unPackedNote->fx ) {
                case 1:  // A: set Speed
                {
                    iNote->effects[1].effect = SET_TEMPO; 
                    if ( !unPackedNote->fxp ) 
                    { 
                        iNote->effects[1].effect = NO_EFFECT;
                        iNote->effects[1].argument = NO_EFFECT;
                    }
                    break;
                }
                case 2: // B
                {
                    iNote->effects[1].effect = POSITION_JUMP;
                    break;
                }
                case 3: // C
                {
                    iNote->effects[1].effect = PATTERN_BREAK; // recalc argument?
                    break;
                }
                case 4: // D: all kinds of (fine) volume slide
                /*
                    So, apparently:
                    D01 = volume slide down by 1
                    D10 = volume slide up   by 1
                    DF1 = fine volume slide down by 1
                    D1F = fine volume slide up   by 1
                    DFF = fine volume slide up   by F
                    DF0 = volume slide up   by F
                    D0F = volume slide down by F
                */
                {   
                    iNote->effects[1].effect = VOLUME_SLIDE; // default
                    /*
                    int slide1 = iNote->effects[1].argument >> 4;
                    int slide2 = iNote->effects[1].argument & 0xF;
                    if ( (slide2 == 0xF) && slide1 ) // fine volume up if arg non-zero
                    {
                        iNote->effects[1].effect = EXTENDED_EFFECTS;
                        iNote->effects[1].argument = (FINE_VOLUME_SLIDE_UP << 4)
                            + slide1;
                    } else if ( (slide1 == 0xF) && slide2 ) { // fine volume down if arg non-zero
                        iNote->effects[1].effect = EXTENDED_EFFECTS;
                        iNote->effects[1].argument = (FINE_VOLUME_SLIDE_DOWN << 4)
                            + slide2;
                    } 
                    */
                    break;
                }
                case 5: // E: all kinds of (extra) (fine) portamento down
                {
                    /*
                    int xfx = iNote->effects[1].argument >> 4;
                    int xfxArg = iNote->effects[1].argument & 0xF;
                    switch ( xfx )
                    {
                        case 0xE: // extra fine slide
                        {
                            iNote->effects[1].effect = EXTRA_FINE_PORTAMENTO;
                            iNote->effects[1].argument = (EXTRA_FINE_PORTAMENTO_DOWN << 4)
                                + xfxArg;
                            break;
                        }
                        case 0xF: // fine slide
                        {
                            iNote->effects[1].effect = EXTENDED_EFFECTS;
                            iNote->effects[1].argument = (FINE_PORTAMENTO_DOWN << 4)
                                + xfxArg;
                            
                            break;
                        }
                        default: // normal slide
                        {
                            iNote->effects[1].effect = PORTAMENTO_DOWN;
                            break;
                        }
                    }
                    */
                    iNote->effects[1].effect = PORTAMENTO_DOWN;
                    break;
                }
                case 6: // F: all kinds of (extra) (fine) portamento up
                {
                    /*
                    int xfx = iNote->effects[1].argument >> 4;
                    int xfxArg = iNote->effects[1].argument & 0xF;
                    switch ( xfx )
                    {
                        case 0xE: // extra fine slide
                        {
                            iNote->effects[1].effect = EXTRA_FINE_PORTAMENTO;
                            iNote->effects[1].argument = (EXTRA_FINE_PORTAMENTO_UP << 4)
                                + xfxArg;
                            break;
                        }
                        case 0xF: // fine slide
                        {
                            iNote->effects[1].effect = EXTENDED_EFFECTS;
                            iNote->effects[1].argument = (FINE_PORTAMENTO_UP << 4)
                                + xfxArg;

                            break;
                        }
                        default: // normal slide
                        {
                            iNote->effects[1].effect = PORTAMENTO_UP;
                            break;
                        }
                    }
                    */
                    iNote->effects[1].effect = PORTAMENTO_UP;
                    break;
                }
                case 7: // G
                {
                    iNote->effects[1].effect = TONE_PORTAMENTO;
                    break;
                }
                case 8: // H
                {
                    iNote->effects[1].effect = VIBRATO;
                    break;
                }
                case 9: // I
                {
                    iNote->effects[1].effect = TREMOR;
                    break;
                }
                case 10: // J
                {
                    iNote->effects[1].effect = ARPEGGIO;
                    break;
                }
                case 11: // K
                {
                    iNote->effects[1].effect = VIBRATO_AND_VOLUME_SLIDE;
                    break;
                }
                case 12: // L
                {
                    iNote->effects[1].effect = TONE_PORTAMENTO_AND_VOLUME_SLIDE;
                    break;
                }
                // skip effects 'M' and 'N' here which are not used
                case 15: // O
                {
                    iNote->effects[1].effect = SET_SAMPLE_OFFSET;
                    break;
                }
                // skip effect 'P'
                case 17: // Q
                {
                    iNote->effects[1].effect = MULTI_NOTE_RETRIG; // retrig + volslide
                    break;
                }
                case 18: // R
                {
                    iNote->effects[1].effect = TREMOLO;
                        break;
                }
                case 19: // extended effects 'S'
                {
                    int xfxp = unPackedNote->fxp & 0xF;
                    iNote->effects[1].effect = EXTENDED_EFFECTS;
                    iNote->effects[1].argument = xfxp;
                    switch( unPackedNote->fxp >> 4 )
                    { 
                        // effect 0 = set filter => same as .mod
                        case 1:
                        {
                            iNote->effects[1].argument = (SET_GLISSANDO_CONTROL << 4) + xfxp;
                            break;
                        }
                        case 2:
                        {
                            iNote->effects[1].argument = (SET_FINETUNE << 4) + xfxp;
                            break;
                        }
                        case 3:
                        {
                            iNote->effects[1].argument = (SET_VIBRATO_CONTROL << 4) + xfxp;
                            break;
                        }
                        case 4:
                        {
                            iNote->effects[1].argument = (SET_TREMOLO_CONTROL << 4) + xfxp;
                            break;
                        }
                        case 8:
                        {
                            iNote->effects[1].effect = SET_FINE_PANNING;
                            iNote->effects[1].argument = xfxp * 17;
                            break;
                        }
                        case 10: // stereo control, for panic.s3m :s
                        {
                            /*
                            Signed rough panning, meaning:
                                 0 is center
                                -8 is full left 
                                 7 is full right

                            unsigned: | signed nibble: | pan value:
                            ----------+----------------+-----------
                             0        |  0             |  8
                             1        |  1             |  9
                             2        |  2             | 10
                             3        |  3             | 11
                             4        |  4             | 12
                             5        |  5             | 13
                             6        |  6             | 14
                             7        |  7             | 15
                             8        | -8             |  0
                             9        | -7             |  1
                            10        | -6             |  2
                            11        | -5             |  3
                            12        | -4             |  4
                            13        | -3             |  5
                            14        | -2             |  6
                            15        | -1             |  7 

                            Conversion from signed nibble to 
                            0 .. 15 unsigned panning value:

                            if (nibble > 7), then nibble = nibble - 8
                            else nibble = nibble + 8 

                            This is according to fs3mdoc.txt
                            */
                            iNote->effects[1].effect = SET_FINE_PANNING;
                            if ( xfxp > 7 ) xfxp -= 8;
                            else            xfxp += 8;
                            iNote->effects[1].argument = xfxp * 17;
                            break;
                        }
                        case 11:
                        {
                            iNote->effects[1].argument = (SET_PATTERN_LOOP << 4) + xfxp;
                            break;
                        }
                        // other extended effect commands are again same as in .mod
                    }                    
                    break;
                }
                case 20: // T
                {
                    iNote->effects[1].effect = SET_BPM; 
                    if ( unPackedNote->fxp < 0x20 )
                    {
                        iNote->effects[1].effect = NO_EFFECT;
                        iNote->effects[1].argument = NO_EFFECT;
                    }
                    break;
                }
                case 21: // U 
                {
                    iNote->effects[1].effect = FINE_VIBRATO;
                    break;
                }
                case 22: // V
                {
                    iNote->effects[1].effect = SET_GLOBAL_VOLUME;
                    break;
                }
                case 23: // W
                {
                    iNote->effects[1].effect = GLOBAL_VOLUME_SLIDE;
                    break;
                }            
                case 24: // X
                {
                    /*
                        XA4 = surround:
                        Enables surround playback on this channel. When using 
                        stereo playback, the right channel of a sample is 
                        played with inversed phase (Pro Logic Surround). When 
                        using quad playback, the rear channels are used for 
                        playing this channel. Surround mode can be disabled by 
                        executing a different panning command on the same 
                        channel. 

                    */
                    // surround is not supported yet:
                    if ( iNote->effects[1].argument == 0xA4 ) break; 
                    iNote->effects[1].effect = SET_FINE_PANNING;
                    iNote->effects[1].argument <<= 1;
                    if ( iNote->effects[1].argument > 0xFF )
                        iNote->effects[1].argument = 0xFF;
                    break;
                }
                default: // unknown effect command
                {
                    iNote->effects[1].effect = NO_EFFECT;
                    iNote->effects[1].argument = NO_EFFECT;
                    break;
                }
            }
            // next note / channel:
            iNote++;
            unPackedNote++;
        }
    }
    delete[] unPackedPtn;
    delete[] buf;
    isLoaded_ = true;
    return 0;
}