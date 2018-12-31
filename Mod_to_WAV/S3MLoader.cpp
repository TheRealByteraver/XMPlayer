/*
Thanks must go to:
- PSI (Sami Tammilehto) / Future Crew for creating Scream Tracker 3
- FireLight / Brett Paterson for writing a detailed document explaining how to
  parse .s3m files. Without fs3mdoc.txt this would have been hell. With his
  valuable document, it was a breeze.
*/

#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>

#include "Module.h"

#define debug_s3m_loader  

#ifdef debug_s3m_loader
#include <bitset>
#include <iomanip>
#endif

#define S3M_MIN_FILESIZE            (sizeof(S3mFileHeader) + 4 * 64 * 2 + 1)
#define S3M_MAX_CHANNELS            32
#define S3M_MAX_INSTRUMENTS         100
#define S3M_CHN_UNUSED              255
#define S3M_DEFAULT_PAN_LEFT        48
#define S3M_DEFAULT_PAN_RIGHT       207
#define S3M_DEFAULT_PAN_CENTER      128
#define S3M_MAX_SONGTITLE_LENGTH    28
#define S3M_MAX_SAMPLENAME_LENGTH   28
#define S3M_MAX_VOLUME              64
#define S3M_TRACKER_TAG_LENGTH      4
#define S3M_MARKER_PATTERN          254
#define S3M_END_OF_SONG_MARKER      255
#define S3M_ST2VIBRATO_FLAG         1
#define S3M_ST2TEMPO_FLAG           2
#define S3M_AMIGASLIDES_FLAG        4
#define S3M_VOLUME_ZERO_OPT_FLAG    8
#define S3M_AMIGA_LIMITS_FLAG       16
#define S3M_FILTER_ENABLE_FLAG      32
#define S3M_ST300_VOLSLIDES_FLAG    64
#define S3M_CUSTOM_DATA_FLAG        128
#define S3M_TRACKER_MASK            0xF000
#define S3M_TRACKER_VERSION_MASK    0x0FFF
#define S3M_SIGNED_SAMPLE_DATA      1
#define S3M_UNSIGNED_SAMPLE_DATA    2
#define S3M_STEREO_FLAG             128
#define S3M_DEFAULT_PANNING_PRESENT 0xFC
#define S3M_SAMPLE_NOTPACKED_FLAG   0
#define S3M_SAMPLE_PACKED_FLAG      1    // DP30ADPCM, not supported
#define S3M_SAMPLE_LOOP_FLAG        1
#define S3M_SAMPLE_STEREO_FLAG      2
#define S3M_SAMPLE_16BIT_FLAG       4
#define S3M_INSTRUMENT_TYPE_SAMPLE          1
#define S3M_INSTRUMENT_TYPE_ADLIB_MELODY    2
#define S3M_INSTRUMENT_TYPE_ADLIB_DRUM      3


#pragma pack (1) 
struct S3mFileHeader {
    char            songTitle[S3M_MAX_SONGTITLE_LENGTH];
    unsigned char   id;             // 0x1A
    unsigned char   fileType;       // 0x1D for ScreamTracker 3
    unsigned short  reserved1;
    unsigned short  songLength;     // actually songlength - 1
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
    char            dosFilename[12];// 12! not 13!!!
    unsigned char   memSeg;
    unsigned short  memOfs;
    unsigned        length;
    unsigned        loopStart;
    unsigned        loopEnd;
    unsigned char   volume;
    unsigned char   reserved;       // should be 0x1D
    unsigned char   packId;         // should be 0
    unsigned char   flags;
    unsigned        c2Speed;
    unsigned char   reserved2[12];  // useless info for us
    char            name[S3M_MAX_SAMPLENAME_LENGTH];
    char            tag[4];         // "SCRS"
};

struct UnpackedNote {
    unsigned char note;
    unsigned char inst;
    unsigned char volc;
    unsigned char fx;
    unsigned char fxp;
};
#pragma pack (8) 

int Module::loadS3mFile() {
    char                *buf;
    std::ifstream::pos_type  fileSize = 0;
    std::ifstream            s3mFile( 
        fileName_,std::ios::in | std::ios::binary | std::ios::ate );
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
    s3mFileHeader.id = '\0'; // use the EOF dos marker as end of cstring marker
    // ?? s3mFileHeader.songLength++; // counts from zero in file
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
    int nChannels = 0;
    int chnRemap[S3M_MAX_CHANNELS];
    int chnPanVals[S3M_MAX_CHANNELS];
    for ( int chn = 0; chn < S3M_MAX_CHANNELS; chn++ )
    {
        chnPanVals[chn] = S3M_DEFAULT_PAN_CENTER;
        int chnInfo = (int)s3mFileHeader.channelsEnabled[chn];
        if ( chnInfo < 16 ) // channel is used! // x64 FT2 detects weird #chn
        {
            chnRemap[chn] = nChannels;
            if ( chnInfo < 7 )  chnPanVals[nChannels] = S3M_DEFAULT_PAN_LEFT;
            else                chnPanVals[nChannels] = S3M_DEFAULT_PAN_RIGHT;
            nChannels++;
        } else { 
            chnRemap[chn] = S3M_CHN_UNUSED;
        }
    }
#ifdef debug_s3m_loader
    std::cout << std::endl
        << "# channels:         " << std::dec << nChannels;
    std::cout << std::endl << "Order list: ";
#endif
    // initialize internal module data.
    songTitle_ = new char[S3M_MAX_SONGTITLE_LENGTH + 1];
    for ( int i = 0; i < S3M_MAX_SONGTITLE_LENGTH; i++ ) {
        songTitle_[i] = s3mFileHeader.songTitle[i];
    }
    songTitle_[S3M_MAX_SONGTITLE_LENGTH] = '\0';
    trackerTag_ = new char[S3M_TRACKER_TAG_LENGTH + 1];
    for ( int i = 0; i < S3M_TRACKER_TAG_LENGTH; i++ ) {
        trackerTag_[i] = s3mFileHeader.tag[i];
    }
    trackerTag_[S3M_TRACKER_TAG_LENGTH] = '\0';
    useLinearFrequencies_ = true;
    songRestartPosition_ = 0;
    isCustomRepeat_ = false;
    panningStyle_ = PANNING_STYLE_S3M; // to be changed in the player code
    nChannels_ = nChannels;
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
    nPatterns_++;
#ifdef debug_s3m_loader
    std::cout << std::endl
        << "SongLength (corr.): " << songLength_ << std::endl
        << "Number of Patterns: " << nPatterns_ << std::endl;
#endif
    bufOffset += s3mFileHeader.songLength;
    unsigned short *instrParaPtrs = (unsigned short *)(buf + bufOffset);
    bufOffset += s3mFileHeader.nInstruments * 2; // sizeof( unsigned short ) == 2
    unsigned short *ptnParaPtrs = (unsigned short *)(buf + bufOffset);
    bufOffset += s3mFileHeader.nPatterns    * 2; // sizeof( unsigned short ) == 2
    unsigned char *defPanPositions = (unsigned char *)(buf + bufOffset); // 32 bytes
    if ( bufOffset > fileSize ) { 
#ifdef debug_s3m_loader
        std::cout << std::endl
            << "File is too small, exiting.";
#endif
        delete[] buf;
        return 0;
    }
    if ( (int)s3mFileHeader.masterVolume & S3M_STEREO_FLAG )
    {
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defPanPositions[i] = S3M_DEFAULT_PAN_CENTER;
    } else {
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defPanPositions[i] <<= 4;
    }
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
    if ( (int)s3mFileHeader.masterVolume & S3M_STEREO_FLAG )
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
        S3mInstHeader& instHeader = *((S3mInstHeader *)(buf + bufOffset));
        smpDataPtrs[nInst] = 
            16 * (((int)instHeader.memSeg << 16) + (int)instHeader.memOfs);
        if ( !instHeader.c2Speed ) instHeader.c2Speed = 8363;
#ifdef debug_s3m_loader
        instHeader.name[S3M_MAX_SAMPLENAME_LENGTH - 1] = '\0'; // debug only
        instHeader.tag[3] = '\0';                              // debug only
        std::cout << std::endl
            << "Instrument nr " << nInst << " info: " << std::endl
            << "Sample Type:        ";
        if ( instHeader.sampleType == S3M_INSTRUMENT_TYPE_SAMPLE )
            std::cout << "digital sample";
        else if ( instHeader.sampleType == S3M_INSTRUMENT_TYPE_ADLIB_MELODY )
            std::cout << "adlib melody";
        else if ( instHeader.sampleType == S3M_INSTRUMENT_TYPE_ADLIB_DRUM )
            std::cout << "adlib drum";
        std::cout << std::endl
            << "Sample location:    "
            << (int)instHeader.memSeg << ":"
            << (int)instHeader.memOfs << " " << std::endl;
        instHeader.memSeg = '\0'; // so we can read the DOS file name
        std::cout
            << "Sample location:    " << smpDataPtrs[nInst] << std::endl
            << "Length:             " << instHeader.length << std::endl
            << "Loop Start:         " << instHeader.loopStart << std::endl
            << "Loop End:           " << instHeader.loopEnd << std::endl
            << "Volume:             " << (int)instHeader.volume << std::endl
            << "0x1D:             0x" << std::hex << (int)instHeader.reserved << std::endl << std::dec
            << "pack ID:            " << (int)instHeader.packId << std::endl
            << "flags:              " << (int)instHeader.flags << std::endl
            << "C2SPD:              " << instHeader.c2Speed << std::endl
            << "Sample DOS Name:    " << instHeader.dosFilename << std::endl
            << "Name:               " << instHeader.name << std::endl
            << "Tag:                " << instHeader.tag << std::endl;
#endif
        if ( (instHeader.sampleType != 0) &&
            (instHeader.sampleType != S3M_INSTRUMENT_TYPE_SAMPLE) ) {
#ifdef debug_s3m_loader
            std::cout << std::endl
                << "Unable to read adlib samples, exiting!";
#endif
            delete[] buf;
            return 0;
        }
        InstrumentHeader    instrument;
        SampleHeader        sample;
        char                sampleName[S3M_MAX_SAMPLENAME_LENGTH + 1];
        sampleName[S3M_MAX_SAMPLENAME_LENGTH] = '\0';
        strncpy_s( sampleName,S3M_MAX_SAMPLENAME_LENGTH + 1,instHeader.name,S3M_MAX_SAMPLENAME_LENGTH );
        sample.name = sampleName;
        instrument.name = sampleName;
        sample.length = instHeader.length;
        sample.repeatOffset = instHeader.loopStart;
        sample.volume = (int)instHeader.volume;
        // safety checks:
        if ( instHeader.loopEnd >= instHeader.loopStart )
            sample.repeatLength = instHeader.loopEnd - instHeader.loopStart + 1;
        else sample.repeatLength = sample.length;
        if ( sample.volume > S3M_MAX_VOLUME ) sample.volume = S3M_MAX_VOLUME;
        if ( sample.repeatOffset >= sample.length ) sample.repeatOffset = 0;
        if ( sample.repeatOffset + sample.repeatLength > sample.length )
            sample.repeatLength = sample.length - sample.repeatOffset;
        sample.isRepeatSample = (instHeader.flags & S3M_SAMPLE_LOOP_FLAG) != 0;
        if ( (instHeader.sampleType == S3M_INSTRUMENT_TYPE_SAMPLE) &&
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
            instrument.samples[0]->load( sample );
        }
        instruments_[nInst] = new Instrument;
        instruments_[nInst]->load( instrument );
#ifdef debug_s3m_loader
        /*
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
            waveFormatEx.nSamplesPerSec = 8000; // frequency
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
*/
#endif
    }
    // Now load the patterns:
#define S3M_ROWS_PER_PATTERN                64
#define S3M_PTN_END_OF_ROW_MARKER           0
#define S3M_PTN_CHANNEL_MASK                31
#define S3M_PTN_NOTE_INST_FLAG              32
#define S3M_PTN_VOLUME_COLUMN_FLAG          64
#define S3M_PTN_EFFECT_PARAM_FLAG           128
    UnpackedNote *unPackedPtn = new UnpackedNote[S3M_ROWS_PER_PATTERN * nChannels_];
    for ( unsigned iPtn = 0; iPtn < nPatterns_; iPtn++ )
    {
        memset( unPackedPtn,0,S3M_ROWS_PER_PATTERN * nChannels_ * sizeof( UnpackedNote ) );
        char *s3mPtn = buf + 16 * ptnParaPtrs[iPtn];
        unsigned packedSize = ((unsigned short *)s3mPtn)[0];
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
        unsigned char *ptnPtr = (unsigned char *)(s3mPtn + 2);
        int row = 0;
        for ( ; (ptnPtr < (unsigned char *)(s3mPtn + packedSize)) &&
                (row < S3M_ROWS_PER_PATTERN); ) {
            char pack = *ptnPtr++;
            if ( pack == S3M_PTN_END_OF_ROW_MARKER ) row++;                
            else { 
                unsigned chn = chnRemap[pack & S3M_PTN_CHANNEL_MASK];                
                unsigned char note = 0;
                unsigned char inst = 0;
                unsigned char volc = 0;
                unsigned char fx   = 0;
                unsigned char fxp  = 0;
                if ( pack & S3M_PTN_NOTE_INST_FLAG )
                {
                    note = *ptnPtr++;
                    inst = *ptnPtr++;
                    if ( note == 255 ) note = 0;
                }
                if ( pack & S3M_PTN_VOLUME_COLUMN_FLAG ) volc = *ptnPtr++;
                if ( pack & S3M_PTN_EFFECT_PARAM_FLAG )
                {
                    fx = *ptnPtr++;
                    fxp = *ptnPtr++;
                }
                if ( chn < nChannels_ )
                {
                    UnpackedNote& unpackedNote = unPackedPtn[row * nChannels_ + chn];
                    //unpackedNote.note = note;// (note >> 4) * (note & 0xF) * 12;
                    if( note )
                        unpackedNote.note = (note >> 4) * 12 + (note & 0xF) + 24;
                    else unpackedNote.note = 0;  // c-0 can't be saved?
                    unpackedNote.inst = inst;
                    unpackedNote.volc = volc;
                    unpackedNote.fx = fx;
                    unpackedNote.fxp = fxp;
                }                
            }            
        }
/*
MP.PAS:

  PeriodTable: Array[0..14*8] of Word = (
    960,954,948,940,934,926,920,914,
    907,900,894,887,881,875,868,862,856,850,844,838,832,826,820,814,
    808,802,796,791,785,779,774,768,762,757,752,746,741,736,730,725,
    720,715,709,704,699,694,689,684,678,675,670,665,660,655,651,646,
    640,636,632,628,623,619,614,610,604,601,597,592,588,584,580,575,
    570,567,563,559,555,551,547,543,538,535,532,528,524,520,516,513,
    508,505,502,498,494,491,487,484,480,477,474,470,467,463,460,457,
    453,450,447,443,440,437,434,431,428);

Function Note2Period(Note, Finetune: ShortInt): Word;
VAR
  i, j, k, l: LongInt;
BEGIN
  If Note>0 then Dec(Note);
  i:=Word(Note mod 12) shl 3 + ((Finetune + 128) shr 4);
  j:=(Byte(Finetune) and $F) shl 1;
  k:=Note div 12 + 2;
  l:=PeriodTable[8+i];
  Note2Period:=(l shl 5 + (PeriodTable[9+i] - l) * j) shr k;
END;

{лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл}

Function Period2Freq(Period: Word): LongInt; {fn should be ok}
BEGIN
{ If Period<>0 then Period2Freq:=(8363*1712) div (Period)
  Else Period2Freq:=0; }
  If Period<>0 then
    Period2Freq:=7093789 div (Period shl 1)
  Else Period2Freq:=0;
END;
*/

#ifdef debug_s3m_loader
        const char *notetable[/*2 + MAXIMUM_NOTES*/] = { //"---",
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
            "C-B","C#B","D-B","D#B","E-B","F-B","F#B","G-B","G#B","A-B","A#B","B-B"
            //,"off" 
        };
        /*
        char *notetable[] = {
            "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-",
            "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
        };
        */
        std::cout << std::endl << "Pattern nr " << iPtn << ":" << std::endl;
        for ( int row = 0; row < S3M_ROWS_PER_PATTERN; row++ )
        {
            std::cout << std::endl << std::dec << std::setw( 2 ) << row << ":";
            for ( unsigned chn = 0; chn < nChannels_; chn++ )
                if ( chn < 6 )
                {
                    // octave = /16
                    // 
                    UnpackedNote& unpackedNote = unPackedPtn[row * nChannels_ + chn];
                    if ( unpackedNote.note )
                        /*
                        std::cout << notetable[unpackedNote.note % 12]
                                  << (int)(unpackedNote.note / 12);
                        */
                        std::cout << notetable[unpackedNote.note];
                    else std::cout << "---";
                    std::cout << "." << std::setw( 3 ) << (int)unpackedNote.note;
                    /*
                    std::cout << std::hex;
                    if ( unpackedNote.inst ) std::cout << std::setw( 2 ) << (int)unpackedNote.inst;
                    else  std::cout << "  ";
                    if ( unpackedNote.volc ) std::cout << std::setw( 2 ) << (int)unpackedNote.volc;
                    else  std::cout << "  ";
                    if ( unpackedNote.fx ) std::cout << std::setw( 2 ) << (int)unpackedNote.fx;
                    else  std::cout << "  ";
                    if ( unpackedNote.fxp ) std::cout << std::setw( 2 ) << (int)unpackedNote.fxp;
                    else  */std::cout << "  ";
                    std::cout << "|";
                }
        } 
        
#endif
        const unsigned S3MPeriodTab[] = {
            27392,25856,24384,23040,21696,20480,19328,18240,17216,16256,15360,14496,
            13696,12928,12192,11520,10848,10240,9664 ,9120 ,8608 ,8128 ,7680 ,7248 ,
            6848 ,6464 ,6096 ,5760 ,5424 ,5120 ,4832 ,4560 ,4304 ,4064 ,3840 ,3624 ,
            3424 ,3232 ,3048 ,2880 ,2712 ,2560 ,2416 ,2280 ,2152 ,2032 ,1920 ,1812 ,
            1712 ,1616 ,1524 ,1440 ,1356 ,1280 ,1208 ,1140 ,1076 ,1016 ,960  ,906  ,
            856  ,808  ,762  ,720  ,678  ,640  ,604  ,570  ,538  ,508  ,480  ,453  ,
            428  ,404  ,381  ,360  ,339  ,320  ,302  ,285  ,269  ,254  ,240  ,226  ,
            214  ,202  ,190  ,180  ,170  ,160  ,151  ,143  ,135  ,127  ,120  ,113  ,
            107  ,101  ,95   ,90   ,85   ,80   ,75   ,71   ,67   ,63   ,60   ,56   ,
            /*
            These next 2 octaves below,are included if you want to support the entire
            FT2 octave range of.MOD.DOPE.MOD uses these period values for example,
            and are only used when loading.MOD,not for anything else,as S3M doesn't
            even use these octaves. (it does indeed stop at B - 8)
            */
            53   ,50   ,47   ,45   ,42   ,40   ,37   ,35   ,33   ,31   ,30   ,28   ,
            26   ,25   ,23   ,22   ,21   ,20   ,18   ,17   ,16   ,15   ,15   ,14
        };


        // read the pattern into the internal format:
        Note        *iNote,*patternData;
        UnpackedNote* unPackedNote = unPackedPtn;
        patterns_[iPtn] = new Pattern;
        patternData = new Note[nChannels_ * S3M_ROWS_PER_PATTERN];
        patterns_[iPtn]->Initialise( nChannels_,S3M_ROWS_PER_PATTERN,patternData );
        iNote = patternData;
        for ( unsigned n = 0; n < (nChannels_ * S3M_ROWS_PER_PATTERN); n++ ) {
            iNote->note = unPackedNote->note;
            iNote->instrument = unPackedNote->inst;
            // current_period = (8363 * periodtab[note]) / (instrument's C2SPD * 4)
            if ( iNote->note )
            {
                iNote->note -= 20; // !!!!!
                
                S3mInstHeader& instHeader = *((S3mInstHeader *)
                    (buf + 16 * instrParaPtrs[iNote->instrument]));
                iNote->period = (((unsigned)8363 * S3MPeriodTab[iNote->note]) / (instHeader.c2Speed));
                /*
                iNote->period = S3MPeriodTab[iNote->note];
                unsigned j;
                for ( j = 0; j < (132); j++ ) {
                    if ( iNote->period >= periods[j] ) break;
                }
                if ( j >= (132) ) iNote->note = 0;
                else                      iNote->note = j + 1;
                */


            } else iNote->period = 0;

            if ( unPackedNote->volc && (unPackedNote->volc <= S3M_MAX_VOLUME) )
            {
                iNote->effects[0].effect = SET_VOLUME;
                iNote->effects[0].argument = unPackedNote->volc;
            }
            /*
                S3M effect A = 1, B = 2, etc
                do some error checking & effect remapping:
            */
            iNote->effects[1].argument = unPackedNote->fxp;
            switch ( unPackedNote->fx ) {
                case 1:  // set Speed
                {
                    iNote->effects[1].effect = SET_TEMPO_BPM; // to be fixed
                    break;
                }
                case 2:
                {
                    iNote->effects[1].effect = POSITION_JUMP;
                    break;
                }
                case 3:
                {
                    iNote->effects[1].effect = PATTERN_BREAK;
                    break;
                }
                case 4:
                {
                    iNote->effects[1].effect = VOLUME_SLIDE;
                    break;
                }
                case 5:
                {
                    iNote->effects[1].effect = PORTAMENTO_DOWN;
                    break;
                }
                case 6:
                {
                    iNote->effects[1].effect = PORTAMENTO_UP;
                    break;
                }
                case 7:
                {
                    iNote->effects[1].effect = TONE_PORTAMENTO;
                    break;
                }
                case 8:
                {
                    iNote->effects[1].effect = VIBRATO;
                    break;
                }
                case 9:
                {
                    iNote->effects[1].effect = TREMOR;
                    break;
                }
                case 10:
                {
                    iNote->effects[1].effect = ARPEGGIO;
                    break;
                }
                case 11:
                {
                    iNote->effects[1].effect = VIBRATO_AND_VOLUME_SLIDE;
                    break;
                }
                case 12:
                {
                    iNote->effects[1].effect = TONE_PORTAMENTO_AND_VOLUME_SLIDE;
                    break;
                }
                // skip effects 'M' and 'N' here which are not used
                case 15:
                {
                    iNote->effects[1].effect = SET_SAMPLE_OFFSET;
                    break;
                }
                // skip effect 'P'
                case 17:
                {
                    iNote->effects[1].effect = MULTI_NOTE_RETRIG; // retrig + volslide supposedly
                    break;
                }
                case 18:
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
                            iNote->effects[1].argument = xfxp << 4;
                            break;
                        }
                        case 10: // stereo control, new effect to be implemented
                        {
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
                case 20:  // set tempo (bpm)
                {
                    iNote->effects[1].effect = SET_TEMPO_BPM; // to be fixed
                    break;
                }
                case 21:  // fine vibrato, to be fixed
                {
                    iNote->effects[1].effect = VIBRATO;
                    break;
                }
                case 22:
                {
                    iNote->effects[1].effect = SET_GLOBAL_VOLUME;
                    break;
                }
            }

            // next note / channel
            iNote++;
            unPackedNote++;
        }
    }


#ifdef debug_s3m_loader
    _getch();
#endif
    isLoaded_ = true; // temp disabled for debug
    delete[] unPackedPtn;
    delete[] buf;
    return 0;
}