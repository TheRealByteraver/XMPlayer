/*
Thanks must go to:
- PSI (Sami Tammilehto) / Future Crew for creating Scream Tracker 3
- FireLight / Brett Paterson for writing a detailed document explaining how to
  parse .s3m files. Without fs3mdoc.txt this would have been hell. With his
  invaluable document, it was a hell of a lot easier.


  Other sources:
  http://www.shikadi.net/moddingwiki/S3M_Format
  https://wiki.multimedia.cx/index.php?title=Scream_Tracker_3_Module
  https://formats.kaitai.io/s3m/index.html



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

#include "Module.h"
#include "virtualfile.h"

//#define debug_s3m_show_patterns
//#define debug_s3m_play_samples

const int S3M_MAX_CHANNELS                  = 32;
const int S3M_MAX_INSTRUMENTS               = 100;
const int S3M_MAX_PATTERNS                  = 200;
const int S3M_CHN_UNUSED                    = 255;
const int S3M_DEFAULT_PAN_LEFT              = 48;
const int S3M_DEFAULT_PAN_RIGHT             = 207;
const int S3M_DEFAULT_PAN_CENTER            = 128;
const int S3M_MAX_SONGTITLE_LENGTH          = 28;
const int S3M_MAX_SAMPLENAME_LENGTH         = 28;
const int S3M_MAX_VOLUME                    = 64;
const int S3M_TRACKER_TAG_LENGTH            = 4;
const int S3M_MARKER_PATTERN                = 254;
const int S3M_END_OF_SONG_MARKER            = 255;
const int S3M_ST2VIBRATO_FLAG               = 1;
const int S3M_ST2TEMPO_FLAG                 = 2;
const int S3M_AMIGASLIDES_FLAG              = 4;
const int S3M_VOLUME_ZERO_OPT_FLAG          = 8;
const int S3M_AMIGA_LIMITS_FLAG             = 16;
const int S3M_FILTER_ENABLE_FLAG            = 32;
const int S3M_ST300_VOLSLIDES_FLAG          = 64;
const int S3M_CUSTOM_DATA_FLAG              = 128;
const int S3M_TRACKER_MASK                  = 0xF000;
const int S3M_TRACKER_VERSION_MASK          = 0x0FFF;
const int S3M_SIGNED_SAMPLE_DATA            = 1;
const int S3M_UNSIGNED_SAMPLE_DATA          = 2;
const int S3M_STEREO_FLAG                   = 128;
const int S3M_DEFAULT_PANNING_PRESENT       = 0xFC;
const int S3M_SAMPLE_NOTPACKED_FLAG         = 0;
const int S3M_SAMPLE_PACKED_FLAG            = 1;   // DP30ADPCM, not supported
const int S3M_SAMPLE_LOOP_FLAG              = 1;
const int S3M_SAMPLE_STEREO_FLAG            = 2;   // not supported
const int S3M_SAMPLE_16BIT_FLAG             = 4;   
const int S3M_ROWS_PER_PATTERN              = 64;
const int S3M_PTN_END_OF_ROW_MARKER         = 0;
const int S3M_PTN_CHANNEL_MASK              = 31;
const int S3M_PTN_NOTE_INST_FLAG            = 32;
const int S3M_PTN_VOLUME_COLUMN_FLAG        = 64;
const int S3M_PTN_EFFECT_PARAM_FLAG         = 128;
const int S3M_KEY_NOTE_CUT                  = 254;
const int S3M_MAX_NOTE                      = 9 * 12;
const int S3M_INSTRUMENT_TYPE_SAMPLE        = 1;
const int S3M_INSTRUMENT_TYPE_ADLIB_MELODY  = 2;
const int S3M_INSTRUMENT_TYPE_ADLIB_DRUM    = 3;


#pragma pack (1) 
struct S3mFileHeader {
    char            songTitle[S3M_MAX_SONGTITLE_LENGTH];
    std::uint8_t    id;             // 0x1A
    std::uint8_t    fileType;       // 0x1D for ScreamTracker 3
    std::uint16_t   reserved1;
    std::uint16_t   songLength;     // 
    std::uint16_t   nrInstruments;
    std::uint16_t   nrPatterns;      // including marker (unsaved) patterns
    std::uint16_t   flags;          // CWTV == 1320h -> ST3.20
    std::uint16_t   CWTV;           // Created with tracker / version
    std::uint16_t   sampleDataType; // 1 = signed, 2 = unsigned
    char            tag[4];         // SCRM
    std::uint8_t    globalVolume;   // maximum volume == 64
    std::uint8_t    defaultTempo;   // default == 6
    std::uint8_t    defaultBpm;     // default == 125
    std::uint8_t    masterVolume;   // SB PRO master vol, stereo flag
    std::uint8_t    gusClickRemoval;// probably nChannels * 2, min = 16?
    std::uint8_t    useDefaultPanning;// == 0xFC if def. chn pan pos. are present
    std::uint8_t    reserved2[8];
    std::uint16_t   customDataPointer;
    std::uint8_t    channelsEnabled[32];
};

struct S3mInstHeader {
    std::uint8_t    sampleType;
    char            dosFilename[12];// 12, not 13!
    std::uint8_t    memSeg;
    std::uint16_t   memOfs;
    std::uint32_t   length;
    std::uint32_t   loopStart;
    std::uint32_t   loopEnd;
    std::uint8_t    volume;
    std::uint8_t    reserved;       // should be 0x1D
    std::uint8_t    packId;         // should be 0
    std::uint8_t    flags;          // 1 = loop on, 2 = stereo, 4 = 16 bit little endian sample
    std::uint32_t   c4Speed;
    std::uint8_t    reserved2[12];  // useless info for us
    char            name[S3M_MAX_SAMPLENAME_LENGTH];
    char            tag[4];         // "SCRS"
};

const int S3M_MIN_FILESIZE = (sizeof( S3mFileHeader ) + \
                                   sizeof( S3mInstHeader ) + 64 + 16);

struct S3mUnpackedNote {
    std::uint8_t    note;
    std::uint8_t    inst;
    std::uint8_t    volc;
    std::uint8_t    fx;
    std::uint8_t    fxp;
};
#pragma pack (8) 

class S3mDebugShow {
public:
    static void fileHeader( S3mFileHeader& s3mFileHeader );
    static void instHeader( S3mInstHeader& s3mInstHeader );
    static void pattern( S3mUnpackedNote* unPackedPtn,int nChannels );
};

void remapS3mEffects( Effect& remapFx ); // temp forward declaration

int Module::loadS3mFile( VirtualFile& moduleFile ) {
    //isLoaded_ = false; redundant

    // load file into byte buffer and then work on that buffer only
    //VirtualFile s3mFile( fileName_ );
    //if ( s3mFile.getIOError() != VIRTFILE_NO_ERROR ) 
    //    return 0;
    moduleFile.absSeek( 0 );
    VirtualFile& s3mFile = moduleFile;

    S3mFileHeader s3mFileHdr;
    unsigned fileSize = s3mFile.fileSize();
    if ( s3mFile.read( &s3mFileHdr,sizeof( S3mFileHeader ) ) ) 
        return 0;

    // some very basic checking
    if ( (fileSize < S3M_MIN_FILESIZE) ||
         ( ! ((s3mFileHdr.tag[0] == 'S') &&
              (s3mFileHdr.tag[1] == 'C') &&
              (s3mFileHdr.tag[2] == 'R') &&
              (s3mFileHdr.tag[3] == 'M'))
          ) 
        // || (s3mFileHdr.id != 0x1A)
        || (s3mFileHdr.sampleDataType < 1)
        || (s3mFileHdr.sampleDataType > 2)
        ) { 
        if ( showDebugInfo_ )
            std::cout
            << "\nSCRM tag not found or file is too small, exiting.";
        return 0;
    }
    // use the DOS EOF marker as end of cstring marker
    s3mFileHdr.id = '\0'; 
    if ( showDebugInfo_ )
        S3mDebugShow::fileHeader( s3mFileHdr );

    if ( (s3mFileHdr.CWTV == 0x1300) ||
        (s3mFileHdr.flags & S3M_ST300_VOLSLIDES_FLAG) )
        trackerType_ = TRACKER_ST300;
    else    
        trackerType_ = TRACKER_ST321;

    // initialize s3m specific variables:
    minPeriod_ = 56;    // periods[9 * 12 - 1]
    maxPeriod_ = 27392; // periods[0]

    nrChannels_ = 0;
    int chnRemap[S3M_MAX_CHANNELS];
    int chnBackmap[S3M_MAX_CHANNELS];
    int chnPanVals[S3M_MAX_CHANNELS];
    for ( int chn = 0; chn < S3M_MAX_CHANNELS; chn++ ) {
        chnBackmap[chn] = S3M_CHN_UNUSED;
        chnPanVals[chn] = S3M_DEFAULT_PAN_CENTER;
        int chnInfo = (int)s3mFileHdr.channelsEnabled[chn];
        if ( chnInfo < 16 ) { // channel is used! // x64 FT2 detects weird #chn
            chnBackmap[nrChannels_] = chn;
            chnRemap[chn] = nrChannels_;
            if ( chnInfo < 7 )
                chnPanVals[nrChannels_] = PANNING_FULL_LEFT;//S3M_DEFAULT_PAN_LEFT;
            else
                chnPanVals[nrChannels_] = PANNING_FULL_RIGHT;//S3M_DEFAULT_PAN_RIGHT;
            nrChannels_++;
        } 
        else 
            chnRemap[chn] = S3M_CHN_UNUSED;
    }
    if ( showDebugInfo_ )
        std::cout 
            << "\nNr of channels   : " << std::dec << nrChannels_
            << "\nOrder list       : ";

    songTitle_.assign( s3mFileHdr.songTitle,S3M_MAX_SONGTITLE_LENGTH );
    trackerTag_.assign( s3mFileHdr.tag,S3M_TRACKER_TAG_LENGTH );
    
    useLinearFrequencies_ = false;   // S3M uses AMIGA periods
    songRestartPosition_ = 0;
    isCustomRepeat_ = false;
    panningStyle_ = PANNING_STYLE_S3M; 
    nrInstruments_ = s3mFileHdr.nrInstruments;
    nrSamples_ = 0;
    defaultTempo_ = s3mFileHdr.defaultTempo;
    defaultBpm_ = s3mFileHdr.defaultBpm;
    if ( defaultTempo_ == 0 || defaultTempo_ == 255 ) 
        defaultTempo_ = 6;
    if ( defaultBpm_ < 33 ) 
        defaultBpm_ = 125;

    // Read in the Pattern order table:
    memset( patternTable_,0,sizeof( *patternTable_ ) * MAX_PATTERNS );
    nrPatterns_ = 0;
    songLength_ = 0;

    for ( int i = 0; i < s3mFileHdr.songLength; i++ ) {
        unsigned char readOrder;
        if ( s3mFile.read( &readOrder,sizeof( unsigned char ) ) ) 
            return 0;
        unsigned order = readOrder;
        if ( order == S3M_END_OF_SONG_MARKER ) 
            order = END_OF_SONG_MARKER;
        else if ( order >= S3M_MARKER_PATTERN ) 
            order = MARKER_PATTERN;
        else if ( order > nrPatterns_ ) 
            nrPatterns_ = order;
        patternTable_[songLength_] = order;
        songLength_++;        
        if ( showDebugInfo_ )
            std::cout << order << " ";
    }
    nrPatterns_++;
    if ( showDebugInfo_ )
        std::cout
            << "\nSong length (corr.): " << songLength_
            << "\nNr of patterns     : " << nrPatterns_;

    // fix for empty patterns that are not present in the file, not even
    // with a header saying they're 0 bytes big:
    if ( nrPatterns_ > s3mFileHdr.nrPatterns )
        nrPatterns_ = s3mFileHdr.nrPatterns;

    unsigned        instrParaPtrs[S3M_MAX_INSTRUMENTS];
    unsigned        ptnParaPtrs[S3M_MAX_PATTERNS];
    unsigned char   defPanPositions[S3M_MAX_CHANNELS];
    for ( int nInst = 0; nInst < s3mFileHdr.nrInstruments; nInst++ ) {
        unsigned short instPointer;
        if ( s3mFile.read( &instPointer,sizeof( unsigned short ) ) ) 
            return 0;
        instrParaPtrs[nInst] = instPointer;
    }
    for ( int nPtn = 0; nPtn < s3mFileHdr.nrPatterns; nPtn++ ) {
        unsigned short ptnPointer;
        if ( s3mFile.read( &ptnPointer,sizeof( unsigned short ) ) ) 
            return 0;
        ptnParaPtrs[nPtn] = ptnPointer;
    }
    if ( s3mFile.read( &defPanPositions,sizeof( unsigned char ) * S3M_MAX_CHANNELS ) ) 
        return 0;

    s3mFile.relSeek( -(int)(sizeof( unsigned char ) * S3M_MAX_CHANNELS) );

    // to be reviewed (ugly code) --------------------------
    if ( (int)s3mFileHdr.masterVolume & S3M_STEREO_FLAG )
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defaultPanPositions_[i] = (defPanPositions[i] & 0xF) * 16; // convert to 8 bit pan
    else 
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defaultPanPositions_[i] = S3M_DEFAULT_PAN_CENTER;
    
    if ( s3mFileHdr.useDefaultPanning == S3M_DEFAULT_PANNING_PRESENT ) 
        for ( unsigned i = 0; i < nrChannels_; i++ )
            chnPanVals[i] = defaultPanPositions_[chnBackmap[i]];

    for ( unsigned i = 0; i < nrChannels_; i++ )
        defaultPanPositions_[i] = chnPanVals[i];
    // end "to be reviewed" marker -------------------------


    if ( showDebugInfo_ ) {
        std::cout << "\n\nInstrument pointers: ";
        for ( int i = 0; i < s3mFileHdr.nrInstruments; i++ )
            std::cout << instrParaPtrs[i] << " ";
        std::cout << "\n\nPattern pointers: ";
        for ( int i = 0; i < s3mFileHdr.nrPatterns; i++ )
            std::cout << ptnParaPtrs[i] << " ";
        std::cout 
            << "\n\nUse default panning from file: "
            << (s3mFileHdr.useDefaultPanning == S3M_DEFAULT_PANNING_PRESENT 
                ? "yes" : "no");
        std::cout << "\n\nDefault panning positions: ";
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            std::cout << ((int)defPanPositions[i]) << " ";
        if ( ((int)s3mFileHdr.masterVolume & S3M_STEREO_FLAG) == 0 )
            std::cout << "\n\nS3M file is in mono mode.";
        else    
            std::cout << "\n\nS3M file is in stereo mode.";
        std::cout << "\nFinal panning positions: ";
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            std::cout << ((int)chnPanVals[i]) << " ";
        std::cout << "\n";
    }

    // read instruments here
    nrSamples_ = 0;
    unsigned smpDataPtrs[S3M_MAX_INSTRUMENTS];
    if ( s3mFileHdr.nrInstruments > S3M_MAX_INSTRUMENTS )
        s3mFileHdr.nrInstruments = S3M_MAX_INSTRUMENTS;

    for ( int instrumentNr = 1; instrumentNr <= s3mFileHdr.nrInstruments; instrumentNr++ ) {
        S3mInstHeader   s3mInstHdr;
        s3mFile.absSeek( 16 * instrParaPtrs[instrumentNr - 1] );
        if ( s3mFile.read( &s3mInstHdr,sizeof( S3mInstHeader ) ) ) 
            return 0;

        smpDataPtrs[instrumentNr - 1] = (((int)s3mInstHdr.memSeg << 16)
                                      + (int)s3mInstHdr.memOfs) << 4;

        if ( !s3mInstHdr.c4Speed ) 
            s3mInstHdr.c4Speed = (unsigned)NTSC_C4_SPEED;
        
        if ( showDebugInfo_ ) {
            std::cout
                << "\n\nInstrument nr " << instrumentNr << " info:"
                << "\nSample data ptr   : " << smpDataPtrs[instrumentNr - 1];
            S3mDebugShow::instHeader( s3mInstHdr );
        }
        
        if ( (s3mInstHdr.sampleType != 0) &&
            (s3mInstHdr.sampleType != S3M_INSTRUMENT_TYPE_SAMPLE) ) {           
            // exit on error disabled for 2nd_pm.s3m
            if ( showDebugInfo_ ) 
                std::cout
                    << "\nWarning: song contains Adlib instruments!";
        }
        InstrumentHeader    instHdr;
        SampleHeader        smpHdr;

        // S3M has no instruments, only samples
        smpHdr.name.assign( s3mInstHdr.name,S3M_MAX_SAMPLENAME_LENGTH );
        instHdr.name = smpHdr.name;
        //instHdr.nSamples = 1; // redundant?
        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
            instHdr.sampleForNote[i].note = i;            
            instHdr.sampleForNote[i].sampleNr = instrumentNr;
        }
        smpHdr.length = s3mInstHdr.length;

        if ( (s3mInstHdr.sampleType == S3M_INSTRUMENT_TYPE_SAMPLE) &&
            (smpDataPtrs[instrumentNr - 1] != 0)) {
            nrSamples_++;
            s3mFile.absSeek( smpDataPtrs[instrumentNr - 1] );

            bool unsignedData = s3mFileHdr.sampleDataType == S3M_UNSIGNED_SAMPLE_DATA;
            bool is16BitSample = (s3mInstHdr.flags & S3M_SAMPLE_16BIT_FLAG) != 0;
            bool isStereoSample = (s3mInstHdr.flags & S3M_SAMPLE_STEREO_FLAG) != 0;

            smpHdr.dataType = (unsignedData ? 0 : SAMPLEDATA_IS_SIGNED_FLAG) |
                (is16BitSample ? SAMPLEDATA_IS_16BIT_FLAG : 0) |
                (isStereoSample ? SAMPLEDATA_IS_STEREO_FLAG : 0);

            unsigned datalength = smpHdr.length;
            if ( is16BitSample )
                datalength *= 2;
            if( isStereoSample )
                datalength *= 2;

            smpHdr.data = (std::int16_t *)s3mFile.getSafePointer( datalength );

            // missing smpHdr data, see if we can salvage something ;)
            if ( smpHdr.data == nullptr ) {
                if ( smpDataPtrs[instrumentNr - 1] >= (unsigned)s3mFile.fileSize() )
                    datalength = 0;
                else
                    datalength = s3mFile.dataLeft();

                smpHdr.length = datalength;
                if ( is16BitSample )
                    smpHdr.length /= 2;
                if ( isStereoSample )
                    smpHdr.length /= 2;


                if ( smpHdr.length )
                    smpHdr.data = (std::int16_t *)s3mFile.getSafePointer( datalength );

                if ( showDebugInfo_ ) 
                    std::cout << std::dec
                        << "\nMissing smpHdr data for smpHdr nr "
                        << (instrumentNr - 1)
                        << "! Shortening smpHdr."
                        << "\nsample.data:   " << smpHdr.data
                        << "\nsample.length: " << smpHdr.length
                        << "\ndata + dataln: " << (smpHdr.data + datalength)
                        << "\nfilesize:      " << fileSize
                        << "\novershoot:     " << (s3mInstHdr.length - smpHdr.length)
                        << "\n";
            }
            // skip to next instHdr if there is no smpHdr data:
            if ( !smpHdr.length ) {
                continue;
            }

            smpHdr.repeatOffset = s3mInstHdr.loopStart;
            smpHdr.volume = (int)s3mInstHdr.volume;
            //smpHdr.c4Speed = instHeader.c4Speed;
            // safety checks:
            if ( s3mInstHdr.loopEnd >= s3mInstHdr.loopStart )
                smpHdr.repeatLength = s3mInstHdr.loopEnd - s3mInstHdr.loopStart;
            else
                smpHdr.repeatLength = smpHdr.length;
            if ( smpHdr.volume > S3M_MAX_VOLUME )
                smpHdr.volume = S3M_MAX_VOLUME;
            if ( smpHdr.repeatOffset >= smpHdr.length )
                smpHdr.repeatOffset = 0;
            if ( smpHdr.repeatOffset + smpHdr.repeatLength > smpHdr.length )
                smpHdr.repeatLength = smpHdr.length - smpHdr.repeatOffset;
            smpHdr.isRepeatSample = (s3mInstHdr.flags & S3M_SAMPLE_LOOP_FLAG) != 0;

            // finetune + relative note recalc
            unsigned int s3mPeriod = 
                ((unsigned)8363 * periods[4 * 12]) / s3mInstHdr.c4Speed;

            unsigned j;
            for ( j = 0; j < MAXIMUM_NOTES; j++ ) 
                if ( s3mPeriod >= periods[j] ) break;
            
            if ( j < MAXIMUM_NOTES ) {
                smpHdr.relativeNote = j - (4 * 12);
                smpHdr.finetune = (int)round(
                    ((double)(133 - j) - 12.0 * log2( (double)s3mPeriod / 13.375 ))
                     * 128.0) - 128;
            } 
            else { 
                smpHdr.relativeNote = 0;
                smpHdr.finetune = 0;
            }
            if ( showDebugInfo_ )
                std::cout 
                    << "\nRelative note     : " 
                    << noteStrings[4 * 12 + smpHdr.relativeNote]
                    << "\nFinetune          : " << smpHdr.finetune;
            
            if ( smpHdr.length ) 
                samples_[instrumentNr] = std::make_unique<Sample>( smpHdr );
        }
        instruments_[instrumentNr] = std::make_unique <Instrument>( instHdr );

        if ( showDebugInfo_ ) {
#ifdef debug_s3m_play_samples
            if ( !samples_[instrumentNr] ) 
                _getch();
            else
                playSampleNr( instrumentNr );
#endif
        }
    }

    // Now load the patterns:
    std::unique_ptr < S3mUnpackedNote[] > unPackedPtn  = 
        std::make_unique < S3mUnpackedNote[] > ( S3M_ROWS_PER_PATTERN * nrChannels_ );

    for ( unsigned patternNr = 0; patternNr < nrPatterns_; patternNr++ ) {

        memset( unPackedPtn.get(),0,S3M_ROWS_PER_PATTERN * nrChannels_ * sizeof( S3mUnpackedNote ) );

        // empty patterns are not stored
        if ( ptnParaPtrs[patternNr] ) { 
            s3mFile.absSeek( (unsigned)16 * (unsigned)ptnParaPtrs[patternNr] );
            unsigned ptnMaxSize = s3mFile.dataLeft();
            char *s3mPtn = (char *)s3mFile.getSafePointer( ptnMaxSize );

            // temp DEBUG:
            if ( s3mPtn == nullptr ) {
                return 0;
            }

            unsigned packedSize = *((unsigned short *)s3mPtn);
            if( packedSize > ptnMaxSize ) {
                if ( showDebugInfo_ )
                    std::cout
                        << "\nMissing pattern data while reading file"
                        << ", exiting!\n";
                return 0;
            }

            unsigned char *ptnPtr = (unsigned char *)s3mPtn + 2;
            int row = 0;
            for ( ; (ptnPtr < (unsigned char *)s3mPtn + packedSize) &&
                (row < S3M_ROWS_PER_PATTERN); ) {
                char pack = *ptnPtr++;
                if ( pack == S3M_PTN_END_OF_ROW_MARKER ) 
                    row++;
                else {
                    unsigned chn = chnRemap[pack & S3M_PTN_CHANNEL_MASK]; // may return 255               
                    unsigned char note = 0;
                    unsigned char inst = 0;
                    unsigned char volc = 0;
                    unsigned char fx = 0;
                    unsigned char fxp = 0;
                    bool newNote = false;
                    if ( pack & S3M_PTN_NOTE_INST_FLAG ) {
                        newNote = true;
                        note = *ptnPtr++;
                        inst = *ptnPtr++;
                    }
                    // we add 0x10 to the volume column so we know an effect is there
                    if ( pack & S3M_PTN_VOLUME_COLUMN_FLAG ) 
                        volc = 0x10 + *ptnPtr++;

                    if ( pack & S3M_PTN_EFFECT_PARAM_FLAG ) {
                        fx = *ptnPtr++;
                        fxp = *ptnPtr++;
                    }
                    if ( chn < nrChannels_ ) {
                        S3mUnpackedNote& unpackedNote = unPackedPtn[row * nrChannels_ + chn];
                        if ( newNote ) {
                            if ( note == S3M_KEY_NOTE_CUT ) 
                                unpackedNote.note = KEY_NOTE_CUT;
                            else if ( note != 0xFF ) {
                                unpackedNote.note = (note >> 4) * 12 + (note & 0xF) + 1 + 12;
                                if ( unpackedNote.note > S3M_MAX_NOTE )
                                    unpackedNote.note = 0;
                            } 
                            else 
                                note = 0; // added: 0 or 255 means no note
                        }
                        unpackedNote.inst = inst;
                        unpackedNote.volc = volc;
                        unpackedNote.fx = fx;
                        unpackedNote.fxp = fxp;
                    }
                }
            }
        }
        if ( showDebugInfo_ ) {
#ifdef debug_s3m_show_patterns
            std::cout << std::dec << "\nPattern nr " << patternNr << ":\n";
            S3mDebugShow::pattern( unPackedPtn,nChannels_ );            
#endif
        }
        // read the pattern into the internal format:
        S3mUnpackedNote* unPackedNote = unPackedPtn.get();
        std::vector<Note> patternData( nrChannels_* S3M_ROWS_PER_PATTERN );
        std::vector<Note>::iterator iNote = patternData.begin();

        for ( unsigned n = 0; n < (nrChannels_ * S3M_ROWS_PER_PATTERN); n++ ) {
            iNote->note = unPackedNote->note;
            iNote->instrument = unPackedNote->inst;

            if ( unPackedNote->volc ) {
                unPackedNote->volc -= 0x10;
                if ( unPackedNote->volc <= S3M_MAX_VOLUME ) {
                    iNote->effects[0].effect = SET_VOLUME;
                    iNote->effects[0].argument = unPackedNote->volc;
                } 
                else {
                    if ( unPackedNote->volc >= 128 &&
                         unPackedNote->volc <= 192 ) {
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
            iNote->effects[1].effect = unPackedNote->fx;
            iNote->effects[1].argument = unPackedNote->fxp;

            remapS3mEffects( iNote->effects[1] );

            // next note / channel:
            iNote++;
            unPackedNote++;
        }
        //patterns_[patternNr] = new Pattern( nChannels_,S3M_ROWS_PER_PATTERN,patternData );
        patterns_[patternNr] = std::make_unique < Pattern >
            ( nrChannels_,S3M_ROWS_PER_PATTERN,patternData );
    }
    isLoaded_ = true;
    return 0;
}

void remapS3mEffects( Effect& remapFx )
{
    switch ( remapFx.effect ) {
        case 1:  // A: set Speed
        {
            remapFx.effect = SET_TEMPO;
            if ( remapFx.argument == 0 ) 
                remapFx.effect = NO_EFFECT;
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
            remapFx.effect = VOLUME_SLIDE;
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
        // skip effects 'M' and 'N' here which are not used
        case 15: // O
        {
            remapFx.effect = SET_SAMPLE_OFFSET;
            break;
        }
        // skip effect 'P'
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
            // moved parser to player because of effect memory handling
            /*
            int xfxp = unPackedNote->fxp & 0xF;
            remapFx.argument = xfxp;
            switch( unPackedNote->fxp >> 4 )
            {
            // effect 0 = set filter => same as .mod
            case 1:
            {
            remapFx.argument = (SET_GLISSANDO_CONTROL << 4) + xfxp;
            break;
            }
            case 2:
            {
            remapFx.argument = (SET_FINETUNE << 4) + xfxp;
            break;
            }
            case 3:
            {
            remapFx.argument = (SET_VIBRATO_CONTROL << 4) + xfxp;
            break;
            }
            case 4:
            {
            remapFx.argument = (SET_TREMOLO_CONTROL << 4) + xfxp;
            break;
            }
            case 8:
            {
            remapFx.effect = SET_FINE_PANNING;
            remapFx.argument = xfxp * 16; // * 17;
            break;
            }
            case 10: // stereo control, for panic.s3m :s
            {

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

            remapFx.effect = SET_FINE_PANNING;
            if ( xfxp > 7 ) xfxp -= 8;
            else            xfxp += 8;
            remapFx.argument = xfxp * 16; // * 17;
            break;
            }
            case 11:
            {
            remapFx.argument = (SET_PATTERN_LOOP << 4) + xfxp;
            break;
            }
            // other extended effect commands are again same as in .mod
            }
            */
            break;
        }
        case 20: // T
        {
            remapFx.effect = SET_BPM;
            if ( remapFx.argument < 0x20 ) {
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
            if ( remapFx.argument == 0xA4 )
                break;
            remapFx.effect = SET_FINE_PANNING;
            remapFx.argument <<= 1;
            if ( remapFx.argument > 0xFF )
                remapFx.argument = 0xFF;
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

// DEBUG helper functions that write verbose output to the screen:
void S3mDebugShow::fileHeader( S3mFileHeader& s3mFileHeader )
{
    std::cout
        << "\nHeader info:"
        << "\nSong title       : " << s3mFileHeader.songTitle
        << "\nFile type        : " << std::hex << (int)s3mFileHeader.fileType
        << "\nSong length      : " << std::dec << (int)s3mFileHeader.songLength
        << "\nNr of instruments: " << (int)s3mFileHeader.nrInstruments
        << "\nNr of patterns   : " << (int)s3mFileHeader.nrPatterns
        << "\nFlags            : " << std::bitset<16>( s3mFileHeader.flags )
        << "\nMade w/ (CWTV)   : " << std::hex << s3mFileHeader.CWTV
        << "\nSample data type : ";
    if ( (int)s3mFileHeader.sampleDataType == S3M_UNSIGNED_SAMPLE_DATA )
        std::cout << "unsigned (standard)";
    else if ( (int)s3mFileHeader.sampleDataType == S3M_SIGNED_SAMPLE_DATA )
        std::cout << "signed (non-standard)";
    else std::cout << "illegal value: error in file!";
    std::cout
        << "\nGlobal volume    : " << std::dec << (int)s3mFileHeader.globalVolume
        << "\nDefault tempo    : " << (int)s3mFileHeader.defaultTempo
        << "\nDefault bpm      : " << (int)s3mFileHeader.defaultBpm
        << "\nMaster volume    : " << (int)s3mFileHeader.masterVolume
        << "\nGus click removal: " << (int)s3mFileHeader.gusClickRemoval
        << "\nUse def. panning : " << (int)s3mFileHeader.useDefaultPanning
        << "\nCustom data ptr  : " << (int)s3mFileHeader.customDataPointer
        << "\nChannels enabled : \n" << std::dec;
    for ( int chn = 0; chn < 32; chn++ ) {
        std::cout << std::setw( 2 ) << chn << ":" << std::setw( 3 )
            << (int)s3mFileHeader.channelsEnabled[chn] << " ";
        if ( (chn + 1) % 8 == 0 ) std::cout << "\n";
    }
}

void S3mDebugShow::instHeader( S3mInstHeader& s3mInstHeader )
{
    s3mInstHeader.name[S3M_MAX_SAMPLENAME_LENGTH - 1] = '\0'; // debug only
    s3mInstHeader.tag[3] = '\0';                              // debug only
    std::cout << "\nSample Type       : ";

    if ( s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_SAMPLE )
        std::cout << "digital sample";
    else if ( s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_ADLIB_MELODY )
        std::cout << "adlib melody";
    else if ( s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_ADLIB_DRUM )
        std::cout << "adlib drum";

    std::cout
        << "\nSample location   : "
        << (int)s3mInstHeader.memSeg << ":"
        << (int)s3mInstHeader.memOfs << " ";
    s3mInstHeader.memSeg = '\0'; // so we can read the DOS file name
    std::cout
        << "\nLength            : " << s3mInstHeader.length
        << "\nLoop Start        : " << s3mInstHeader.loopStart
        << "\nLoop End          : " << s3mInstHeader.loopEnd
        << "\nVolume            : " << (int)s3mInstHeader.volume
        << std::hex
        << "\n0x1D              : 0x" << (int)s3mInstHeader.reserved
        << std::dec
        << "\npack ID           : " << (int)s3mInstHeader.packId
        << "\nflags             : " << (int)s3mInstHeader.flags
        << "\nC2SPD             : " << s3mInstHeader.c4Speed
        << "\nSample DOS Name   : " << s3mInstHeader.dosFilename
        << "\nName              : " << s3mInstHeader.name
        << "\nTag               : " << s3mInstHeader.tag;

}

void S3mDebugShow::pattern( S3mUnpackedNote* unPackedPtn,int nChannels )
{    
    for ( int row = 0; row < S3M_ROWS_PER_PATTERN; row++ ) {
        std::cout 
            << "\n" << std::hex << std::setw( 2 ) << row << ":" << std::dec;
        for ( int chn = 0; chn < nChannels; chn++ ) {
            if ( chn < 8 ) {
                S3mUnpackedNote& unpackedNote = unPackedPtn[row * nChannels + chn];
                if ( unpackedNote.note ) {
                    if ( unpackedNote.note < (12 * 11) )
                        std::cout << noteStrings[unpackedNote.note];
                    else if ( unpackedNote.note == KEY_OFF ) std::cout << "===";
                    else
                        std::cout << "!!!";
                } 
                else
                    std::cout << "---";

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
    }
}

