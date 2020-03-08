/*
    Loader variable name conventions (should be same among all loaders):
    - Index in instrument loop: instrumentNr
    - Index in sample loop: sampleNr
    - Index in pattern loop: patternNr
    - short name for instrumentHeader: instHdr
    - short name for sampleHeader: smpHdr


    Supported:
    Taketracker 1..3 channel, 5 / 7 / 9 / 11 / 13 / 15 channel modules
    Fasttracker 2..32 channel modules
    Startrekker 8 channel (FLT8) modules
    CD81, OCTA 8 channel tags
    .WOW (mod's grave) files
    15 instrument Noisetracker files
    31 instrument modules with unknown tag & unknown nr of channels

    Not Supported:
    PP20 compressed mod's, or other variants of compression
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
#include <bitset>
#include <iomanip>
#include <vector>
#include <iterator>
#include <algorithm>

#include "Module.h"
#include "virtualfile.h"

//#define debug_mod_show_patterns
//#define debug_mod_play_samples

const int MOD_DEBUG_SHOW_PATTERN_NO = 8; // pattern to be shown

// Constants for the .MOD Format:
const int MOD_MAX_ILLEGAL_CHARS = 8;    // nr of illegal chars permitted in smp names
const int MOD_ROWS = 64;                // always 64 rows in a MOD pattern
const int MOD_MAX_SONGNAME_LENGTH = 20;
const int MOD_MAX_SAMPLENAME_LENGTH = 22;
const int MOD_MAX_PATTERNS = 128;
const int MOD_MAX_CHANNELS = 32;
const int MOD_DEFAULT_BPM = 125;
const int MOD_DEFAULT_TEMPO = 6;
const int MOD_MAX_PERIOD = 7248;        // chosen a bit arbitrarily!;

// =============================================================================
// These structures represent exactly the layout of a MOD file:
// the pragma directive prevents the compiler from enlarging the struct with 
// dummy bytes for performance purposes

#pragma pack (1) 
struct ModSampleHeader { 
    char            name[MOD_MAX_SAMPLENAME_LENGTH];
    std::uint16_t   length;         // Big-End Word; * 2 = samplelength in bytes
    char            finetune;       // This is in fact a signed nibble 
    std::uint8_t    linearVolume;
    std::uint16_t   repeatOffset;   // Big-End Word; * 2 = RepeatOffset in bytes
    std::uint16_t   repeatLength;   // Big-End Word; * 2 = RepeatLength in bytes 
};  // size == 30 bytes

// NST header layout:
struct HeaderNST {
    char            songTitle[MOD_MAX_SONGNAME_LENGTH];          
    ModSampleHeader samples[15];            // 15 * 30 = 450
    std::uint8_t    songLength;
    std::uint8_t    restartPosition;
    std::uint8_t    patternTable[128];
}; // size ==  22 + 15 * 30 + 2 + 128 = 600

// M.K. header layout:
struct HeaderMK {
    char            songTitle[MOD_MAX_SONGNAME_LENGTH];
    ModSampleHeader samples[31];
    std::uint8_t    songLength;
    std::uint8_t    restartPosition;
    std::uint8_t    patternTable[128];
    char            tag[4];     
};  // size == 600 + 16 * 30 + 4 = 1084

// restore default alignment
#pragma pack (8)                            

// this fn swaps a 16 bit word's low and high order bytes
inline std::uint16_t SwapW( std::uint16_t d ) {
    return (d >> 8) | (d << 8);
}

class ModHelperFn {
public:
    static int      badCommentCnt( const char* comment );
    static int      getNrSamples( const HeaderMK& headerMK );
    static int      getTagInfo( const std::string& getTagInfo,bool& flt8Err,unsigned& trackerType );
    static bool     isWowFile( std::string fileName );
    static int      convertFlt8Pattern( VirtualFile& modFile );
    static void     remapEffects( Effect& remapFx );
};

int Module::loadModFile( VirtualFile& moduleFile )
{
    //VirtualFile modFile( fileName_ );
    //if ( modFile.getIOError() != VIRTFILE_NO_ERROR ) 
    //    return 0;

    moduleFile.absSeek( 0 );
    VirtualFile& modFile = moduleFile;    
    unsigned fileSize = modFile.fileSize();

    // initialize mod specific variables:
    minPeriod_ = 14;    // periods[11 * 12 - 1]
    maxPeriod_ = 27392; // periods[0]  // ?

    HeaderMK    modHeader;
    HeaderNST&  headerNST = (HeaderNST& )modHeader;
    HeaderMK&   headerMK  = modHeader;

    modFile.read( &modHeader,sizeof( HeaderMK ) );
    if ( modFile.getIOError() > VIRTFILE_EOF ) 
        return 0;

    // check extension for the wow factor ;)
    bool wowFile = ModHelperFn::isWowFile( fileName_ );

    // check if a valid tag is present and get the MOD type
    bool flt8Err;
    trackerTag_.assign( headerMK.tag,4 );
    nrChannels_ = ModHelperFn::getTagInfo( trackerTag_,flt8Err,trackerType_ );
    bool tagErr = nrChannels_ == 0;

    nrInstruments_ = ModHelperFn::getNrSamples( headerMK );
    bool nstErr = nrInstruments_ == 15; // if only 1st 15 smp names are ok (NST file)
    bool smpErr = nrInstruments_ == 0;  // if even 1st 15 smp names are garbage

    if ( tagErr && nstErr && (!smpErr) ) {
        nrChannels_ = 4;
        trackerTag_ = "NST";
    }
    else
        nstErr = false;

    // let's calculate the total size taken by the patterns
    // first, subtract the size of the header
    unsigned patternDataSize = fileSize 
        - (nstErr ? sizeof( HeaderNST ) : sizeof( HeaderMK ));

    nrSamples_ = 0;
    // calculate the total size of the samples
    unsigned sampleDataSize = 0;

    // take care of the endianness and calculate total sample data size
    for( unsigned i = 0; i < nrInstruments_; i++ ) {
        headerMK.samples[i].length       = 
            SwapW( headerMK.samples[i].length       );
        headerMK.samples[i].repeatOffset = 
            SwapW( headerMK.samples[i].repeatOffset );
        headerMK.samples[i].repeatLength = 
            SwapW( headerMK.samples[i].repeatLength );
        sampleDataSize += ((unsigned)headerMK.samples[i].length) << 1;
    }
    // and this finally gives us the total size taken by the pattern data:
    patternDataSize -= sampleDataSize;

    // we use two different ways to calculate the pattern size
    unsigned calcPatternCnt = 0;
    unsigned patternDivideRest;
    if ( nrChannels_ ) {
        // pattern size in bytes = nrChannels * 64 rows * 4 bytes/note
        int patternSize = nrChannels_ * MOD_ROWS * 4;   
        patternDivideRest = patternDataSize % patternSize; // should be 0
        calcPatternCnt = patternDataSize / patternSize;
    }

    // verify nr of patterns using pattern table
    unsigned hdrPatternCnt = 0;
    if ( nstErr ) {
        for ( int i = 0; i < MOD_MAX_PATTERNS; i++ ) {
            unsigned idx = patternTable_[i] = headerNST.patternTable[i];
            if ( idx > hdrPatternCnt )
                hdrPatternCnt = idx;
        }
        songRestartPosition_ = headerNST.restartPosition;
        songLength_ = headerNST.songLength;
    } 
    else {
        if ( flt8Err )
            for ( int i = 0; i < MOD_MAX_PATTERNS; i++ ) 
                headerMK.patternTable[i] >>= 1;
        for ( int i = 0; i < MOD_MAX_PATTERNS; i++ ) {
            unsigned idx = patternTable_[i] = headerMK.patternTable[i];
            if ( idx > hdrPatternCnt )
                hdrPatternCnt = idx;
        }
        songRestartPosition_ = headerMK.restartPosition;
        songLength_ = headerMK.songLength;
    }

    // patterns are numbered starting from zero
    hdrPatternCnt++;
    if ( hdrPatternCnt > MOD_MAX_PATTERNS ) {
        if ( showDebugInfo_ )
            std::cout << "\nPattern table has illegal values, exiting.\n";
        return 0;
    }

    panningStyle_ = PANNING_STYLE_MOD;
    defaultTempo_ = MOD_DEFAULT_TEMPO;
    defaultBpm_   = MOD_DEFAULT_BPM;
    useLinearFrequencies_ = false;

    // we can't have more than 128 patterns in a mod file
    bool ptnErr = hdrPatternCnt > MOD_MAX_PATTERNS;

    if ( (songRestartPosition_ > songLength_) ||
         (songRestartPosition_ == 127)) 
        songRestartPosition_ = 0;

    isCustomRepeat_ = songRestartPosition_ != 0;

    // now interpret the obtained info
    // this is not a .MOD file, or it's compressed
    if ( ptnErr && tagErr && smpErr )  
        return 0;

    // check file integrity, correct nr of channels if necessary
    nrPatterns_ = hdrPatternCnt;
    if ( !tagErr ) {              // ptnCalc and chn were initialised
        if ( wowFile && ((hdrPatternCnt * 2) == calcPatternCnt) ) 
            nrChannels_ = 8;
    } 
    else {
        if ( !nstErr ) {         // ptnCalc and chn were not initialised
            patternDivideRest = calcPatternCnt % (MOD_ROWS * 4);
            calcPatternCnt >>= 8;
            nrChannels_ = calcPatternCnt / hdrPatternCnt; 
            if ( (nrChannels_ > MOD_MAX_CHANNELS) || (!nrChannels_) ) {
                if ( showDebugInfo_ )
                    std::cout 
                    << "\nUnable to detect nr of channels, exiting.\n";
                return 0;
            }
        }
    }
    //if ((calcPatternCnt < nPatterns_) && (!patternDivideRest)) nPatterns_ = calcPatternCnt;
    unsigned patternDataOffset = nstErr ? sizeof( HeaderNST ) : sizeof( HeaderMK );
    unsigned sampleDataOffset = patternDataOffset 
                + nrPatterns_ * nrChannels_ * MOD_ROWS * 4;

    if ( (sampleDataOffset + sampleDataSize) > fileSize ) {
        unsigned missingData = (sampleDataOffset + sampleDataSize) - (int)fileSize;
        unsigned lastInstrument = nrInstruments_;
        if ( showDebugInfo_ )
            std::cout 
                << "\nWarning! File misses Sample Data!\n"
                << "\nnPatterns          = " << nrPatterns_
                << "\nPatternHeader      = " << hdrPatternCnt
                << "\nPatternDataSize    = " << patternDataSize
                << "\nCalcPatternCnt     = " << calcPatternCnt
                << "\nSample Data Offset = " << sampleDataOffset
                << "\nSample Data Size   = " << sampleDataSize
                << "\nOffset + Data      = " 
                << (sampleDataOffset + sampleDataSize)
                << "\nFile Size          = " << fileSize
                << "\nDifference         = " << missingData;

        for ( ; missingData && lastInstrument; ) {
            ModSampleHeader* sample = &(headerMK.samples[lastInstrument - 1]);
            unsigned length       = ((unsigned)sample->length      ) * 2;
            unsigned repeatOffset = ((unsigned)sample->repeatOffset) * 2;
            unsigned repeatLength = ((unsigned)sample->repeatLength) * 2;

            if ( missingData > length ) {
                missingData -= length;
                length = 0;
                repeatOffset = 0;
                repeatLength = 0;
            }
            else {
                length -= missingData;
                missingData = 0;
                if ( repeatOffset > length ) {
                    repeatOffset = 0;
                    repeatLength = 0;
                }
                else {
                    if ( (repeatOffset + repeatLength) > length )
                        repeatLength = length - repeatOffset;
                }
            }
            sample->length = (unsigned)length >> 1;
            sample->repeatOffset = (unsigned)repeatOffset >> 1;
            sample->repeatLength = (unsigned)repeatLength >> 1;
            lastInstrument--;
        }
        if ( missingData ) {
            if ( showDebugInfo_ )
                std::cout 
                    << "\nNo Sample data! Some pattern data is missing!\n";
            return 0;
        }
    }  
    // take care of default panning positions:
    for ( unsigned i = 0; i < nrChannels_; i++ ) 
        if( ((i & 3) == 0) || ((i & 3) == 3) )
            defaultPanPositions_[i] = PANNING_FULL_LEFT;
        else
            defaultPanPositions_[i] = PANNING_FULL_RIGHT;

    // *******************************
    //
    // Now start with copying the data
    //
    // *******************************

    // we start with the song title :)
    songTitle_.assign( headerMK.songTitle,MOD_MAX_SONGNAME_LENGTH );
    
    // now, the sample headers & sample data.
    unsigned fileOffset = sampleDataOffset;
    for ( unsigned sampleNr = 1; sampleNr <= nrInstruments_; sampleNr++ ) {
        InstrumentHeader    instHdr;
        SampleHeader        smpHdr;

        smpHdr.name.clear();        
        smpHdr.name.append( 
            headerMK.samples[sampleNr - 1].name,
            MOD_MAX_SAMPLENAME_LENGTH );
        instHdr.name = smpHdr.name;
        //instrument.nSamples = 1; // redundant?
        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
            instHdr.sampleForNote[i].note = i;
            instHdr.sampleForNote[i].sampleNr = sampleNr;
        }
        smpHdr.length       = 
            ((unsigned)headerMK.samples[sampleNr - 1].length)       << 1;
        smpHdr.repeatOffset = 
            ((unsigned)headerMK.samples[sampleNr - 1].repeatOffset) << 1;
        smpHdr.repeatLength = 
            ((unsigned)headerMK.samples[sampleNr - 1].repeatLength) << 1;
        smpHdr.volume       = headerMK.samples[sampleNr - 1].linearVolume;

        if ( showDebugInfo_ )
            std::cout
            << "\n\nSample " << sampleNr << " data:"
            << "\nRepeatOffset                 : " << smpHdr.repeatOffset
            << "\nRepeatLength                 : " << smpHdr.repeatLength
            << "\nRepeat Offset + Repeat Length: " << (smpHdr.repeatOffset + smpHdr.repeatLength)
            << "\nLength                       : " << smpHdr.length
            << "\nVolume                       : " << (unsigned)smpHdr.volume;

        if ( smpHdr.volume > MAX_VOLUME ) 
            smpHdr.volume = MAX_VOLUME;
        if ( smpHdr.repeatOffset > smpHdr.length ) 
            smpHdr.repeatOffset = 3;
        smpHdr.isRepeatSample = (smpHdr.repeatLength > 2);
        if ( (smpHdr.repeatOffset + smpHdr.repeatLength) > smpHdr.length )
            smpHdr.repeatLength = smpHdr.length - smpHdr.repeatOffset;

        if ( showDebugInfo_ )
            std::cout
            << "\nAfter correction:"
            << "\nRepeatOffset                 : " << smpHdr.repeatOffset
            << "\nRepeatLength                 : " << smpHdr.repeatLength
            << "\nRepeat Offset + Repeat Length: " << (smpHdr.repeatOffset + smpHdr.repeatLength)
            << "\nLength                       : " << smpHdr.length << " (unchanged)"
            << "\nVolume                       : " << (unsigned)smpHdr.volume;

        // convert signed nibble to int and scale it up
        smpHdr.finetune = 
            (signed char)(headerMK.samples[sampleNr - 1].finetune << 4);

        if ( smpHdr.length > 2 ) {
            nrSamples_++;
            modFile.absSeek( fileOffset );
            smpHdr.data = (std::int16_t *)modFile.getSafePointer( smpHdr.length );
            if ( smpHdr.data == nullptr )
                return 0;            // temp DEBUG: exit on missing smpHdr data

            //smpHdr.dataType = SAMPLEDATA_SIGNED_8BIT;
            smpHdr.dataType = SAMPLEDATA_IS_SIGNED_FLAG; // 8 bit signed mono

            samples_[sampleNr] = std::make_unique<Sample>( smpHdr );

        }   
        fileOffset += smpHdr.length; // avoid if  length <= 2 ?
        instruments_[sampleNr] = std::make_unique <Instrument>( instHdr );

#ifdef debug_mod_play_samples
        if ( showDebugInfo_ )
            playSampleNr( sampleNr );
#endif
    }
    // And now the patterns. First, go to the right offset
    modFile.absSeek( patternDataOffset );
    if ( modFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;

    // Now read the patterns and convert them into the internal format
    modFile.absSeek( patternDataOffset );
    for ( unsigned patternNr = 0; patternNr < nrPatterns_; patternNr++ ) {
        // to redo in a safer way
        if ( flt8Err )
            if ( ModHelperFn::convertFlt8Pattern( modFile ) )
                return 0;  // exit on error
        if ( loadModPattern( modFile,patternNr ) )
            return 0;
    }
    
    isLoaded_ = true;

    // Apotheosaic debug info ;)
    if ( showDebugInfo_ )
        std::cout << std::dec
            << "\n"
            << "\nFilename             = " << fileName_
            << "\nis Loaded            = " << (isLoaded() ? "Yes" : "No")
            << "\nnChannels            = " << nrChannels_
            << "\nnInstruments         = " << nrInstruments_
            << "\nnSamples             = " << nrSamples_
            << "\nnPatterns            = " << nrPatterns_
            << "\nSong Title           = " << songTitle_
            << "\nis CustomRepeat      = " << (isCustomRepeat() ? "Yes" : "No")
            << "\nSong Length          = " << songLength_
            << "\nSong Restart Positn. = " << songRestartPosition_
            << "\nNST File             = " << (nstErr ? "Yes" : "No")
            << "\n.WOW File            = " << (wowFile ? "Yes" : "No")
            << "\nFile Tag             = " << trackerTag_
            << "\nTotal Samples Size   = " << sampleDataSize
            << "\nptnHdr               = " << hdrPatternCnt
            << "\nPatternDataSize      = " << patternDataSize
            << "\nCalcPatternCnt       = " << calcPatternCnt
            << "\nRest from Divide     = " 
            << patternDivideRest << " (should be zero)";
    return 0;
}

// convert 8 chn startrekker patterns to regular ones
int ModHelperFn::convertFlt8Pattern( VirtualFile& modFile )
{
    unsigned flt[8 * MOD_ROWS];   // 4 bytes / note * 8 channels * 64 rows
    unsigned *p = (unsigned *)modFile.getSafePointer( sizeof( flt ) );
    if ( !p )
        return -1; 

    // save data to a temp buffer
    memcpy( flt,p,sizeof( flt ) );

    unsigned* p1 = flt;                // idx to 1st ptn inside flt buf
    unsigned* p2 = flt + 4 * MOD_ROWS; // idx to 2nd ptn inside flt buf
    for ( int i = 0; i < MOD_ROWS; i++ ) {
        // 4 channels of 1st pattern
        *p++ = *p1++;
        *p++ = *p1++;
        *p++ = *p1++;
        *p++ = *p1++;
        // 4 channels of 2nd pattern
        *p++ = *p2++;
        *p++ = *p2++;
        *p++ = *p2++;
        *p++ = *p2++;
    }
    return 0; 
}

int Module::loadModPattern( VirtualFile& modFile,int patternNr )
{
    std::vector<Note> patternData( nrChannels_ * MOD_ROWS );
    std::vector<Note>::iterator iNote = patternData.begin();
    for ( unsigned n = 0; n < (nrChannels_ * MOD_ROWS); n++ ) {
        unsigned char j1,j2,j3,j4;
        modFile.read( &j1,sizeof( unsigned char ) );
        modFile.read( &j2,sizeof( unsigned char ) );
        modFile.read( &j3,sizeof( unsigned char ) );
        modFile.read( &j4,sizeof( unsigned char ) );

        // if we read the samples without issue the patterns should be fine?
        if ( modFile.getIOError() != VIRTFILE_NO_ERROR )
            return -1;

        iNote->effects[1].effect = j3 & 0xF;
        iNote->effects[1].argument = j4;
        unsigned period = j2;
        j4 = (j1 & 0xF0) + (j3 >> 4);
        iNote->instrument = j4 & 0x1F;
        period += ((j1 & 0xF) << 8) + ((j4 >> 5) << 12);

        // convert period to note:
        iNote->note = 0;
        if ( period ) {
            unsigned j;
            for ( j = 0; j < (MAXIMUM_NOTES); j++ )
                if ( period >= periods[j] )
                    break;

            // ***** added for believe.mod:
            int periodA;
            int periodB;
            if ( j )
                periodA = (int)periods[j - 1];
            else
                periodA = MOD_MAX_PERIOD;
            if ( j < MAXIMUM_NOTES )
                periodB = (int)periods[j];
            else
                periodB = 0;
            int diffA = periodA - (int)period;
            int diffB = (int)period - periodB;
            if ( diffA < diffB )
                j--;
            // ***** end of addition                

            if ( j < MAXIMUM_NOTES )
                iNote->note = j + 1 - 36; // three octaves down
        }
        // do some error checking & effect remapping:
        ModHelperFn::remapEffects( iNote->effects[1] );

#ifdef debug_mod_show_patterns
        if ( showDebugInfo_ )
            if ( i == MOD_DEBUG_SHOW_PATTERN_NO ) {
                if ( (n % nChannels_) == 0 )
                    std::cout << "\n";
                else
                    std::cout << "|";
                std::cout
                    << noteStrings[iNote->note] << ":" << std::dec;
                std::cout << std::setw( 2 ) << iNote->instrument;
                std::cout << std::hex;
                if ( iNote->effects[1].effect > 0xF )
                    std::cout << (char)(iNote->effects[1].effect + 55);
                else
                    std::cout << (unsigned)iNote->effects[1].effect;
                std::cout << std::setw( 2 ) << iNote->effects[1].argument;
            }
#endif
        iNote++;
    }
    patterns_[patternNr] = std::make_unique < Pattern >
        ( nrChannels_,MOD_ROWS,patternData );
    return 0;
}

// read sample names, this is how we differentiate a 31 instruments file 
// from an old format 15 instruments file
// if there is no tag then this is probably not a MOD file!
int ModHelperFn::getNrSamples( const HeaderMK& headerMK )
{
    int count = 0;
    for ( int i = 0; i < 15; i++ )
        count += ModHelperFn::badCommentCnt( headerMK.samples[i].name );

    // this is probably not a MOD file
    if ( count > MOD_MAX_ILLEGAL_CHARS )
        return 0;

    // If only the 1st 15 sample names are valid we prolly have a NST file
    // So we check the validity of sample names 16 - 31:
    count = 0;
    for ( int i = 15; i < 31; i++ )
        count += ModHelperFn::badCommentCnt( headerMK.samples[i].name );

    // this is an NST MOD file
    if ( count > MOD_MAX_ILLEGAL_CHARS )
        return 15;
    // this is a normal MOD file
    else
        return 31;
}

void ModHelperFn::remapEffects( Effect& remapFx )
{
    switch ( remapFx.effect ) {
        case 0: // arpeggio
        {
            if ( remapFx.argument )
                remapFx.effect = ARPEGGIO;
            break;
        }
        /*
        These effects have no effect memory in this format but
        they might have effect memory in different formats so
        we remove them here
        */
        case PORTAMENTO_UP:
        case PORTAMENTO_DOWN:
        {
            if ( !remapFx.argument )
                remapFx.effect = NO_EFFECT;
            break;
        }
        case VOLUME_SLIDE:
        {
            unsigned char& argument = remapFx.argument;
            if ( !argument )
                remapFx.effect = NO_EFFECT;
            else {
                // in .mod & .xm files volume slide up has
                // priority over volume slide down
                unsigned volUp = argument & 0xF0;
                unsigned volDn = argument & 0x0F;
                if ( volUp && volDn ) argument = volUp;
            }
            break;
        }
        case TONE_PORTAMENTO_AND_VOLUME_SLIDE:
        {
            unsigned char& argument = remapFx.argument;
            if ( !argument )
                remapFx.effect = TONE_PORTAMENTO;
            else {
                // in .mod & .xm files volume slide up has
                // priority over volume slide down
                unsigned volUp = argument & 0xF0;
                unsigned volDn = argument & 0x0F;
                if ( volUp && volDn ) argument = volUp;
            }
            break;
        }
        case VIBRATO_AND_VOLUME_SLIDE:
        {
            unsigned char& argument = remapFx.argument;
            if ( !argument )
                remapFx.effect = VIBRATO;
            else {
                // in .mod & .xm files volume slide up has
                // priority over volume slide down
                unsigned volUp = argument & 0xF0;
                unsigned volDn = argument & 0x0F;
                if ( volUp && volDn ) argument = volUp;
            }
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
            if ( remapFx.argument == 0 ) {
                remapFx.effect = NO_EFFECT;
            } else {
                if ( remapFx.argument > 0x1F ) {
                    remapFx.effect = SET_BPM;
                }
            }
            break;
        }
    }
}

/*
this fn returns the number of improbable characters for a sample comment.
In the olden days, the  ASCII control characters 13 and 14 (D and E in
hexadecimal) represented single and double music notes, and some BBS's
inserted these in the sample name strings to show the mod was downloaded
from there.
*/
int ModHelperFn::badCommentCnt( const char *comment )
{
    char allowed[] = "\xD\xE !\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`" \
        "abcdefghijklmnopqrstuvwxyz{|}~";
    int r = 0;
    for ( int i = 0; i < MOD_MAX_SAMPLENAME_LENGTH; i++ )
        if ( !strchr( allowed,tolower( comment[i] ) ) ) 
            r++;
    return r;
}

// This fn returns the number of channels based on the format tag (0 on error)
// and sets the flt8Err flag if an 8 channel startrekker mod file was detected.
// It will set the tracker type as well (only support for 
// TRACKER_PROTRACKER yet)
int ModHelperFn::getTagInfo( const std::string& tagID,bool& flt8Err,unsigned& trackerType )
{
    int     chn;
    trackerType = TRACKER_PROTRACKER;
    flt8Err = false;
    if ( tagID == "M.K." ) chn = 4;
    else if ( tagID == "M!K!" ) chn = 4;
    else if ( tagID == "FLT4" ) chn = 4;
    else if ( tagID == "FLT8" ) {
        chn = 8;
        flt8Err = true;
    }
    else if ( tagID == "OCTA" ) chn = 8;
    else if ( tagID == "CD81" ) chn = 8;
    else if ( tagID.substr( 2,2 ) == "CH" ) {
        chn = (tagID[0] - '0') * 10 + (tagID[1] - '0');
        if ( (chn > 32) || (chn < 10) )
            chn = 0;
    } 
    else if ( tagID.substr( 0,3 ) == "TDZ" ) {
        chn = tagID[3] - '0';
        if ( (chn < 1) || (chn > 3) )
            chn = 0; // only values 1..3 are valid
                     //else trackerType = TRACKER_FT2;
    }
    else if ( tagID.substr( 1,3 ) == "CHN" ) {
        chn = tagID[0] - '0';
        if ( (chn < 5) || (chn > 9) )
            chn = 0; // only values 5..9 are valid
                     //else trackerType = TRACKER_FT2;
    }
    else
        chn = 0;
    return chn;
}

// returns true if file has extension .WOW
// wow files are exactly like M.K. files except they can have 8 channels :s
bool ModHelperFn::isWowFile( std::string fileName )
{
    int len = (int)fileName.length();
    assert( len > 4 );
    std::string strBuf( fileName );

    // put everything in upper case for easy comparing
    std::transform( strBuf.begin(),strBuf.end(),strBuf.begin(),::toupper );

    return strBuf.substr( len - 3,3 ) == "WOW";
}