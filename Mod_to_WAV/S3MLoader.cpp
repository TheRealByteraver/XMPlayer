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
#include <bitset>
#include <iomanip>

#include "Module.h"
#include "virtualfile.h"

//#define debug_s3m_show_patterns
//#define debug_s3m_play_samples

#define S3M_MIN_FILESIZE                    (sizeof(S3mFileHeader) + \
                                             sizeof(S3mInstHeader) + 64 + 16)
#define S3M_MAX_CHANNELS                    32
#define S3M_MAX_INSTRUMENTS                 100
#define S3M_MAX_PATTERNS                    200
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
#define S3M_KEY_NOTE_CUT                    254
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

class S3mDebugShow {
public:
    static void fileHeader( S3mFileHeader& s3mFileHeader );
    static void instHeader( S3mInstHeader& s3mInstHeader );
    static void pattern( S3mUnpackedNote* unPackedPtn,int nChannels );
};

void remapS3mEffects( Effect& remapFx ); // temp forward declaration

int Module::loadS3mFile() {
    isLoaded_ = false;

    // load file into byte buffer and then work on that buffer only
    VirtualFile s3mFile( fileName_ );
    if ( s3mFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;
    S3mFileHeader s3mFileHeader;
    unsigned fileSize = s3mFile.fileSize();
    if ( s3mFile.read( &s3mFileHeader,sizeof( S3mFileHeader ) ) ) 
        return 0;

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
        if ( showDebugInfo_ )
            std::cout
            << "\nSCRM tag not found or file is too small, exiting.";
        return 0;
    }
    // use the DOS EOF marker as end of cstring marker
    s3mFileHeader.id = '\0'; 
    if ( showDebugInfo_ )
        S3mDebugShow::fileHeader( s3mFileHeader );

    if ( (s3mFileHeader.CWTV == 0x1300) ||
        (s3mFileHeader.flags & S3M_ST300_VOLSLIDES_FLAG) )
        trackerType_ = TRACKER_ST300;
    else    
        trackerType_ = TRACKER_ST321;

    // initialize s3m specific variables:
    minPeriod_ = 56;    // periods[9 * 12 - 1]
    maxPeriod_ = 27392; // periods[0]

    nChannels_ = 0;
    int chnRemap[S3M_MAX_CHANNELS];
    int chnBackmap[S3M_MAX_CHANNELS];
    int chnPanVals[S3M_MAX_CHANNELS];
    for ( int chn = 0; chn < S3M_MAX_CHANNELS; chn++ ) {
        chnBackmap[chn] = S3M_CHN_UNUSED;
        chnPanVals[chn] = S3M_DEFAULT_PAN_CENTER;
        int chnInfo = (int)s3mFileHeader.channelsEnabled[chn];
        if ( chnInfo < 16 ) { // channel is used! // x64 FT2 detects weird #chn
            chnBackmap[nChannels_] = chn;
            chnRemap[chn] = nChannels_;
            if ( chnInfo < 7 )
                chnPanVals[nChannels_] = PANNING_FULL_LEFT;//S3M_DEFAULT_PAN_LEFT;
            else
                chnPanVals[nChannels_] = PANNING_FULL_RIGHT;//S3M_DEFAULT_PAN_RIGHT;
            nChannels_++;
        } 
        else 
            chnRemap[chn] = S3M_CHN_UNUSED;
    }
    if ( showDebugInfo_ )
        std::cout 
            << "\nNr of channels   : " << std::dec << nChannels_
            << "\nOrder list       : ";

    for ( int i = 0; i < S3M_MAX_SONGTITLE_LENGTH; i++ ) 
        songTitle_ += s3mFileHeader.songTitle[i];
    
    for ( int i = 0; i < S3M_TRACKER_TAG_LENGTH; i++ ) 
        trackerTag_ += s3mFileHeader.tag[i];
    
    useLinearFrequencies_ = false;   // S3M uses AMIGA periods
    songRestartPosition_ = 0;
    isCustomRepeat_ = false;
    panningStyle_ = PANNING_STYLE_S3M; 
    nInstruments_ = s3mFileHeader.nInstruments;
    nSamples_ = 0;
    defaultTempo_ = s3mFileHeader.defaultTempo;
    defaultBpm_ = s3mFileHeader.defaultBpm;
    if ( defaultTempo_ == 0 || defaultTempo_ == 255 ) 
        defaultTempo_ = 6;
    if ( defaultBpm_ < 33 ) 
        defaultBpm_ = 125;

    // Read in the Pattern order table:
    memset( patternTable_,0,sizeof( *patternTable_ ) * MAX_PATTERNS );
    nPatterns_ = 0;
    songLength_ = 0;

    for ( int i = 0; i < s3mFileHeader.songLength; i++ ) {
        unsigned char readOrder;
        if ( s3mFile.read( &readOrder,sizeof( unsigned char ) ) ) 
            return 0;
        unsigned order = readOrder;
        if ( order == S3M_END_OF_SONG_MARKER ) 
            order = END_OF_SONG_MARKER;
        else if ( order >= S3M_MARKER_PATTERN ) 
            order = MARKER_PATTERN;
        else if ( order > nPatterns_ ) 
            nPatterns_ = order;
        patternTable_[songLength_] = order;
        songLength_++;        
        if ( showDebugInfo_ )
            std::cout << order << " ";
    }
    nPatterns_++;
    if ( showDebugInfo_ )
        std::cout
            << "\nSong length (corr.): " << songLength_
            << "\nNr of patterns     : " << nPatterns_;
    // fix for empty patterns that are not present in the file, not even
    // with a header saying they're 0 bytes big:
    if ( nPatterns_ > s3mFileHeader.nPatterns )
        nPatterns_ = s3mFileHeader.nPatterns;

    unsigned        instrParaPtrs[S3M_MAX_INSTRUMENTS];
    unsigned        ptnParaPtrs[S3M_MAX_PATTERNS];
    unsigned char   defPanPositions[S3M_MAX_CHANNELS];
    for ( int nInst = 0; nInst < s3mFileHeader.nInstruments; nInst++ ) {
        unsigned short instPointer;
        if ( s3mFile.read( &instPointer,sizeof( unsigned short ) ) ) 
            return 0;
        instrParaPtrs[nInst] = instPointer;
    }
    for ( int nPtn = 0; nPtn < s3mFileHeader.nPatterns; nPtn++ ) {
        unsigned short ptnPointer;
        if ( s3mFile.read( &ptnPointer,sizeof( unsigned short ) ) ) 
            return 0;
        ptnParaPtrs[nPtn] = ptnPointer;
    }
    if ( s3mFile.read( &defPanPositions,sizeof( unsigned char ) * S3M_MAX_CHANNELS ) ) 
        return 0;

    s3mFile.relSeek( -(int)(sizeof( unsigned char ) * S3M_MAX_CHANNELS) );

    // to be reviewed (ugly code) --------------------------
    if ( (int)s3mFileHeader.masterVolume & S3M_STEREO_FLAG )
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defaultPanPositions_[i] = (defPanPositions[i] & 0xF) * 16; // convert to 8 bit pan
    else 
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            defaultPanPositions_[i] = S3M_DEFAULT_PAN_CENTER;
    
    if ( s3mFileHeader.useDefaultPanning == S3M_DEFAULT_PANNING_PRESENT ) 
        for ( unsigned i = 0; i < nChannels_; i++ )
            chnPanVals[i] = defaultPanPositions_[chnBackmap[i]];

    for ( unsigned i = 0; i < nChannels_; i++ )
        defaultPanPositions_[i] = chnPanVals[i];
    // end "to be reviewed" marker -------------------------


    if ( showDebugInfo_ ) {
        std::cout << "\n\nInstrument pointers: ";
        for ( int i = 0; i < s3mFileHeader.nInstruments; i++ )
            std::cout << instrParaPtrs[i] << " ";
        std::cout << "\n\nPattern pointers: ";
        for ( int i = 0; i < s3mFileHeader.nPatterns; i++ )
            std::cout << ptnParaPtrs[i] << " ";
        std::cout 
            << "\n\nUse default panning from file: "
            << (s3mFileHeader.useDefaultPanning == S3M_DEFAULT_PANNING_PRESENT 
                ? "yes" : "no");
        std::cout << "\n\nDefault panning positions: ";
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            std::cout << ((int)defPanPositions[i]) << " ";
        if ( ((int)s3mFileHeader.masterVolume & S3M_STEREO_FLAG) == 0 )
            std::cout << "\n\nS3M file is in mono mode.";
        else    
            std::cout << "\n\nS3M file is in stereo mode.";
        std::cout << "\nFinal panning positions: ";
        for ( int i = 0; i < S3M_MAX_CHANNELS; i++ )
            std::cout << ((int)chnPanVals[i]) << " ";
        std::cout << "\n";
    }

    // read instruments here
    nSamples_ = 0;
    unsigned smpDataPtrs[S3M_MAX_INSTRUMENTS];
    if ( s3mFileHeader.nInstruments > S3M_MAX_INSTRUMENTS )
        s3mFileHeader.nInstruments = S3M_MAX_INSTRUMENTS;

    for ( int nInst = 1; nInst <= s3mFileHeader.nInstruments; nInst++ ) {
        S3mInstHeader   s3mInstHeader;
        s3mFile.absSeek( 16 * instrParaPtrs[nInst - 1] );
        if ( s3mFile.read( &s3mInstHeader,sizeof( S3mInstHeader ) ) ) 
            return 0;

        smpDataPtrs[nInst - 1] = 16 * (((int)s3mInstHeader.memSeg << 16)
                                      + (int)s3mInstHeader.memOfs);

        if ( !s3mInstHeader.c4Speed ) 
            s3mInstHeader.c4Speed = (unsigned)NTSC_C4_SPEED;
        
        if ( showDebugInfo_ ) {
            std::cout
                << "\n\nInstrument nr " << nInst << " info:"
                << "\nSample data ptr   : " << smpDataPtrs[nInst - 1];
            S3mDebugShow::instHeader( s3mInstHeader );
        }
        
        if ( (s3mInstHeader.sampleType != 0) &&
            (s3mInstHeader.sampleType != S3M_INSTRUMENT_TYPE_SAMPLE) ) {           
            // exit on error disabled for 2nd_pm.s3m
            if ( showDebugInfo_ ) 
                std::cout
                    << "\nWarning: song contains Adlib instruments!";
        }
        InstrumentHeader    instrument;
        SampleHeader        sample;

        for ( int i = 0; i < S3M_MAX_SAMPLENAME_LENGTH; i++ )
            sample.name += s3mInstHeader.name[i];

        instrument.name = sample.name;
        //instrument.nSamples = 1; // redundant?
        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
            instrument.sampleForNote[i].note = i;            
            instrument.sampleForNote[i].sampleNr = nInst;
        }
        sample.length = s3mInstHeader.length;

        if ( (s3mInstHeader.sampleType == S3M_INSTRUMENT_TYPE_SAMPLE) &&
            (smpDataPtrs[nInst - 1] != 0)) {
            nSamples_++;
            s3mFile.absSeek( smpDataPtrs[nInst - 1] );
            sample.data = (SHORT *)s3mFile.getSafePointer( sample.length );

            // missing sample data, see if we can salvage something ;)
            if ( sample.data == nullptr ) {
                int smpSize;
                if ( smpDataPtrs[nInst - 1] >= s3mFile.fileSize() )
                    smpSize = 0;
                else
                    smpSize = s3mFile.dataLeft();
                sample.length = smpSize;
                if ( sample.length )
                    sample.data = (SHORT *)s3mFile.getSafePointer( sample.length );

                if ( showDebugInfo_ ) 
                    std::cout << std::dec
                        << "\nMissing sample data for sample nr "
                        << (nInst - 1)
                        << "! Shortening sample."
                        << "\nsample.data:   " << (unsigned)sample.data
                        << "\nsample.length: " << sample.length
                        << "\ndata + length: " << (unsigned)(sample.data + sample.length)
                        << "\nfilesize:      " << fileSize
                        << "\novershoot:     " << (s3mInstHeader.length - sample.length)
                        << "\n";
            }
            // skip to next instrument if there is no sample data:
            if ( !sample.length ) {
                //sample.repeatOffset = 0;
                continue;
            }

            sample.repeatOffset = s3mInstHeader.loopStart;
            sample.volume = (int)s3mInstHeader.volume;
            //sample.c4Speed = instHeader.c4Speed;
            // safety checks:
            if ( s3mInstHeader.loopEnd >= s3mInstHeader.loopStart )
                sample.repeatLength = s3mInstHeader.loopEnd - s3mInstHeader.loopStart;
            else
                sample.repeatLength = sample.length;
            if ( sample.volume > S3M_MAX_VOLUME )
                sample.volume = S3M_MAX_VOLUME;
            if ( sample.repeatOffset >= sample.length )
                sample.repeatOffset = 0;
            if ( sample.repeatOffset + sample.repeatLength > sample.length )
                sample.repeatLength = sample.length - sample.repeatOffset;
            sample.isRepeatSample = (s3mInstHeader.flags & S3M_SAMPLE_LOOP_FLAG) != 0;



            // convert sample data from unsigned to signed:
            sample.dataType = SAMPLEDATA_SIGNED_8BIT;
            if ( s3mFileHeader.sampleDataType == S3M_UNSIGNED_SAMPLE_DATA ) {
                unsigned char *s = (unsigned char *)sample.data;
                for ( unsigned i = 0; i < sample.length; i++ ) *s++ ^= 128;
            }            
            // finetune + relative note recalc
            unsigned int s3mPeriod = 
                ((unsigned)8363 * periods[4 * 12]) / s3mInstHeader.c4Speed;

            unsigned j;
            for ( j = 0; j < MAXIMUM_NOTES; j++ ) 
                if ( s3mPeriod >= periods[j] ) break;
            
            if ( j < MAXIMUM_NOTES ) {
                sample.relativeNote = j - (4 * 12);
                sample.finetune = (int)round(
                    ((double)(133 - j) - 12.0 * log2( (double)s3mPeriod / 13.375 ))
                     * 128.0) - 128;
            } 
            else { 
                sample.relativeNote = 0;
                sample.finetune = 0;
            }
            if ( showDebugInfo_ )
                std::cout 
                    << "\nRelative note     : " 
                    << noteStrings[4 * 12 + sample.relativeNote]
                    << "\nFinetune          : " << sample.finetune;
            
            if ( sample.length ) {
                samples_[nInst] = new Sample;
                sample.dataType = SAMPLEDATA_SIGNED_8BIT;
                samples_[nInst]->load( sample );
            }
        }
        instruments_[nInst] = new Instrument;
        instruments_[nInst]->load( instrument );

        if ( showDebugInfo_ ) {
#ifdef debug_s3m_play_samples
            if ( !samples_[nInst] ) 
                _getch();
            else
                playSampleNr( nInst );
#endif
        }
    }

    // Now load the patterns:
    S3mUnpackedNote *unPackedPtn = new S3mUnpackedNote[S3M_ROWS_PER_PATTERN * nChannels_];

    for ( unsigned iPtn = 0; iPtn < nPatterns_; iPtn++ ) {

        memset( unPackedPtn,0,S3M_ROWS_PER_PATTERN * nChannels_ * sizeof( S3mUnpackedNote ) );

        // empty patterns are not stored
        if ( ptnParaPtrs[iPtn] ) { 
            s3mFile.absSeek( (unsigned)16 * (unsigned)ptnParaPtrs[iPtn] );
            unsigned ptnMaxSize = s3mFile.dataLeft();
            char *s3mPtn = (char *)s3mFile.getSafePointer( ptnMaxSize );

            // temp DEBUG:
            if ( s3mPtn == nullptr ) {
                delete[] unPackedPtn;
                return 0;
            }

            unsigned packedSize = *((unsigned short *)s3mPtn);
            if( packedSize > ptnMaxSize ) {
                if ( showDebugInfo_ )
                    std::cout
                        << "\nMissing pattern data while reading file"
                        << ", exiting!\n";
                delete[] unPackedPtn;
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
                    if ( chn < nChannels_ ) {
                        S3mUnpackedNote& unpackedNote = unPackedPtn[row * nChannels_ + chn];
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
            std::cout << std::dec << "\nPattern nr " << iPtn << ":\n";
            S3mDebugShow::pattern( unPackedPtn,nChannels_ );            
#endif
        }
        // read the pattern into the internal format:
        Note        *iNote,*patternData;
        S3mUnpackedNote* unPackedNote = unPackedPtn;
        patterns_[iPtn] = new Pattern;
        patternData = new Note[nChannels_ * S3M_ROWS_PER_PATTERN];
        patterns_[iPtn]->initialise( nChannels_,S3M_ROWS_PER_PATTERN,patternData );
        iNote = patternData;
        for ( unsigned n = 0; n < (nChannels_ * S3M_ROWS_PER_PATTERN); n++ ) {
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
    }
    delete[] unPackedPtn;
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
        << "\nNr of instruments: " << (int)s3mFileHeader.nInstruments
        << "\nNr of patterns   : " << (int)s3mFileHeader.nPatterns
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

