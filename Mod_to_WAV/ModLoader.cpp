/*
    Supported:
    Taketracker 1..3 channel, 5 / 7 / 9 / 11 / 13 / 15 channel modules
    Fasttracker 2..32 channel modules
    Startrekker 8 channel (FLT8) modules
    CD81, OCTA 8 channel tags
    .WOW (mod's grave) files
    15 instrument Noisetracker files
    31 instrument modules with unknown tag & unknown nr of channels
*/
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
#include <iterator>

#include "Module.h"
#include "virtualfile.h"

//#define debug_mod_show_patterns
//#define debug_mod_play_samples

#define MOD_DEBUG_SHOW_PATTERN_NO           8 // pattern to be shown

// Constants for the .MOD Format:
#define MOD_LIMIT                           8    // nr of illegal chars permitted in smp names
#define MOD_ROWS                            64   // always 64 rows in a MOD pattern
#define MOD_MAX_SONGNAME_LENGTH             20
#define MOD_MAX_PATTERNS                    128
#define MOD_MAX_CHANNELS                    32
#define MOD_DEFAULT_BPM                     125
#define MOD_DEFAULT_TEMPO                   6
#define MOD_MAX_PERIOD                      7248 // chosen a bit arbitrarily!


// =============================================================================
// These structures represent exactly the layout of a MOD file:
// the pragma directive prevents the compiler from enlarging the struct with 
// dummy bytes for performance purposes

#pragma pack (1) 
struct ModSampleHeader { 
    char            name[MAX_SAMPLENAME_LENGTH];
    AMIGAWORD       length;         // Big-End Word; * 2 = samplelength in bytes
    char            finetune;       // This is in fact a signed nibble 
    unsigned char   linearVolume;
    AMIGAWORD       repeatOffset;   // Big-End Word; * 2 = RepeatOffset in bytes
    AMIGAWORD       repeatLength;   // Big-End Word; * 2 = RepeatLength in bytes 
};  // size == 30 bytes

// NST header layout:
struct HeaderNST {
    char            songTitle[MOD_MAX_SONGNAME_LENGTH];          
    ModSampleHeader samples[15];            // 15 * 30 = 450
    unsigned char   songLength;             
    unsigned char   restartPosition;        
    unsigned char   patternTable[128];      
}; // size ==  22 + 15 * 30 + 2 + 128 = 600

// M.K. header layout:
struct HeaderMK {
    char            songTitle[MOD_MAX_SONGNAME_LENGTH];
    ModSampleHeader samples[31];
    unsigned char   songLength;
    unsigned char   restartPosition;
    unsigned char   patternTable[128];
    char            tag[4];     
};  // size == 600 + 16 * 30 + 4 = 1084

// restore default alignment
#pragma pack (8)                            

// this fn swaps a 16 bit word's low and high order bytes
inline AMIGAWORD SwapW( AMIGAWORD d ) {
    return (d >> 8) | (d << 8);
}

// forward declarations for little helper procedures
int     nBadComment( char *comment );
int     tagID( std::string tagID,bool &flt8Err,unsigned& trackerType );
bool    isWowFile( std::string fileName );
int     convertFlt8Pattern( VirtualFile& modFile );
void    remapModEffects( Effect& remapFx );


int Module::loadModFile() 
{
    bool        smpErr   = false;   // if first 15 smp names are garbage
    bool        nstErr   = false;   // if next  16 smp names are garbage
    bool        ptnErr   = false;   // if there seem to be an invalid nr of ptns
    bool        tagErr   = false;   // true if no valid tag was found
    bool        flt8Err  = false;   // if FLT8 pattern conversion will be needed
    bool        wowFile  = false;   // if the file extension was .WOW rather than .MOD
    bool        nstFile  = false;   // if it is an NST file (15 instruments)
    unsigned    patternHeader;
    unsigned    patternCalc;
    unsigned    patternDivideRest;
    unsigned    k;
    unsigned    sampleDataSize;
    unsigned    sampleDataOffset;
    unsigned    patternDataOffset;
    unsigned    fileOffset;
    unsigned    fileSize;

    isLoaded_ = false;

    VirtualFile modFile( fileName_ );
    if ( modFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;
    fileSize = modFile.fileSize();

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
    wowFile = isWowFile(fileName_);

    // check if a valid tag is present
    trackerTag_.clear();
    trackerTag_.append( headerMK.tag,4 );
    nChannels_ = tagID( trackerTag_,flt8Err,trackerType_ );

    if ( !nChannels_ ) 
        tagErr = true;

    // read sample names, this is how we differentiate a 31 instruments file 
    // from an old format 15 instruments file
    // if there is no tag then this is probably not a MOD file!
    k = 0;
    for ( int i = 0 ; i < 15; i++ ) 
        k += nBadComment(headerMK.samples[i].name);
    if(k > MOD_LIMIT ) 
        smpErr = true;
    // no tag could mean this is an old NST - MOD file!
    k = 0;
    for ( int i = 15; i < 31; i++ ) 
        k += nBadComment(headerMK.samples[i].name);
    if( k > MOD_LIMIT ) 
        nstErr = true;
    if ( tagErr && nstErr && (!smpErr) ) { 
        nstFile = true; 
        nChannels_ = 4; 
        trackerTag_ = "NST";
    }
    patternCalc = fileSize; 
    if ( nstFile ) { 
        nInstruments_ = 15; 
        patternCalc -= sizeof( HeaderNST ); 
    } 
    else { 
        nInstruments_ = 31; 
        patternCalc -= sizeof( HeaderMK ); 
    }
    nSamples_ = 0;

    sampleDataSize = 0; 
    for( unsigned i = 0; i < nInstruments_; i++ ) {
        headerMK.samples[i].length       = 
            SwapW( headerMK.samples[i].length       );
        headerMK.samples[i].repeatOffset = 
            SwapW( headerMK.samples[i].repeatOffset );
        headerMK.samples[i].repeatLength = 
            SwapW( headerMK.samples[i].repeatLength );
        sampleDataSize += ((unsigned)headerMK.samples[i].length) << 1;
    }
    patternCalc -= sampleDataSize;
    if ( nChannels_ ) {
        // pattern size = 64 rows * 4 b. per note * nChn = 256 * nChn
        int i = ( nChannels_ << 8 );   
        patternDivideRest = patternCalc % i;
        patternCalc /= i;
    }
    // verify nr of patterns using pattern table
    patternHeader = 0;
    if ( nstFile ) {
        for ( int i = 0; i < MOD_MAX_PATTERNS; i++ ) {
            k = patternTable_[i] = headerNST.patternTable[i];
            if ( k > patternHeader ) 
                patternHeader = k;
        }
        songRestartPosition_ = headerNST.restartPosition;
        songLength_ = headerNST.songLength;
    } 
    else {
        if ( flt8Err )
            for ( int i = 0; i < MOD_MAX_PATTERNS; i++ ) 
                headerMK.patternTable[i] >>= 1;
        for ( int i = 0; i < MOD_MAX_PATTERNS; i++ ) {
            k = patternTable_[i] = headerMK.patternTable[i];
            if ( k > patternHeader ) 
                patternHeader = k;
        }
        songRestartPosition_ = headerMK.restartPosition;
        songLength_ = headerMK.songLength;
    }

    // patterns are numbered starting from zero
    patternHeader++;
    if ( patternHeader > MOD_MAX_PATTERNS ) {
        if ( showDebugInfo_ )
            std::cout << "\nPattern table has illegal values, exiting.\n";
        return 0;
    }

    panningStyle_ = PANNING_STYLE_MOD;
    defaultTempo_ = MOD_DEFAULT_TEMPO;
    defaultBpm_   = MOD_DEFAULT_BPM;
    useLinearFrequencies_ = false;

    if (patternHeader > MOD_MAX_PATTERNS) 
        ptnErr = true;
    if ( (songRestartPosition_ > songLength_) ||
         (songRestartPosition_ == 127)) 
        songRestartPosition_ = 0;
    isCustomRepeat_ = songRestartPosition_ != 0;

    // now interpret the obtained info
    // this is not a .MOD file, or it's compressed
    if ( ptnErr && tagErr && smpErr )  
        return 0;

    // check file integrity, correct nr of channels if necessary
    nPatterns_ = patternHeader;
    if ( !tagErr ) {              // ptnCalc and chn were initialised
        if (wowFile && ((patternHeader << 1) == patternCalc)) 
            nChannels_ = 8;
    } 
    else {
        if ( !nstFile ) {         // ptnCalc and chn were not initialised
            patternDivideRest = patternCalc % (MOD_ROWS * 4);
            patternCalc >>= 8;
            nChannels_ = patternCalc / patternHeader; 
            if ( (nChannels_ > MOD_MAX_CHANNELS) || (!nChannels_) ) {
                if ( showDebugInfo_ )
                    std::cout 
                    << "\nUnable to detect nr of channels, exiting.\n";
                return 0;
            }
        }
    }
    //if ((patternCalc < nPatterns_) && (!patternDivideRest)) nPatterns_ = patternCalc;
    patternDataOffset = (nstFile ? sizeof( HeaderNST ) : sizeof( HeaderMK) );
    sampleDataOffset = patternDataOffset 
        + nPatterns_ * nChannels_ * MOD_ROWS * 4;
    if ( (sampleDataOffset + sampleDataSize) > fileSize ) {
        unsigned missingData = (sampleDataOffset + sampleDataSize) - (int)fileSize;
        unsigned lastInstrument = nInstruments_;
        if ( showDebugInfo_ )
            std::cout 
                << "\nWarning! File misses Sample Data!\n"
                << "\nnPatterns          = " << nPatterns_
                << "\nPatternHeader      = " << patternHeader
                << "\nPatternCalc        = " << patternCalc
                << "\nSample Data Offset = " << sampleDataOffset
                << "\nSample Data Size   = " << sampleDataSize
                << "\nOffset + Data      = " 
                << (sampleDataOffset + sampleDataSize)
                << "\nFile Size          = " << fileSize
                << "\nDifference         = " << missingData;
        while ( missingData && lastInstrument ) {
            ModSampleHeader *sample = &(headerMK.samples[lastInstrument - 1]);
            unsigned length       = ((unsigned)sample->length      ) << 1;
            unsigned repeatOffset = ((unsigned)sample->repeatOffset) << 1;
            unsigned repeatLength = ((unsigned)sample->repeatLength) << 1;

            if ( missingData > length ) {
                missingData -= length;
                length       = 0;
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
            sample->length       = (unsigned)length       >> 1;
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
    for ( unsigned i = 0; i < nChannels_; i++ ) 
        if( ((i & 3) == 0) || ((i & 3) == 3) )
            defaultPanPositions_[i] = PANNING_FULL_LEFT;
        else
            defaultPanPositions_[i] = PANNING_FULL_RIGHT;

    // Now start with copying the data
    // we start with the song title :)
    songTitle_.clear();
    songTitle_.append( headerMK.songTitle,MOD_MAX_SONGNAME_LENGTH );
    // now, the sample headers & sample data.
    fileOffset = sampleDataOffset;
    for ( unsigned iSample = 1; iSample <= nInstruments_; iSample++ ) {
        InstrumentHeader    instrument;
        SampleHeader        sample;

        sample.name.clear();        
        sample.name.append( 
            headerMK.samples[iSample - 1].name,
            MAX_SAMPLENAME_LENGTH );
        instrument.name = sample.name;
        //instrument.nSamples = 1; // redundant?
        for ( int i = 0; i < MAXIMUM_NOTES; i++ ) {
            instrument.sampleForNote[i].note = i;
            instrument.sampleForNote[i].sampleNr = iSample;
        }
        sample.length       = 
            ((unsigned)headerMK.samples[iSample - 1].length)       << 1;
        sample.repeatOffset = 
            ((unsigned)headerMK.samples[iSample - 1].repeatOffset) << 1;
        sample.repeatLength = 
            ((unsigned)headerMK.samples[iSample - 1].repeatLength) << 1;
        sample.volume       = headerMK.samples[iSample - 1].linearVolume;
        if ( sample.volume > MAX_VOLUME ) 
            sample.volume = MAX_VOLUME;
        if ( sample.repeatOffset > sample.length ) 
            sample.repeatOffset = 3;
        sample.isRepeatSample = (sample.repeatLength > 2);
        if ( (sample.repeatOffset + sample.repeatLength) > sample.length )
            sample.repeatLength = sample.length - sample.repeatOffset;

        // convert signed nibble to int and scale it up
        sample.finetune = 
            (signed char)(headerMK.samples[iSample - 1].finetune << 4);

        if ( sample.length > 2 ) {
            nSamples_++;
            modFile.absSeek( fileOffset );
            sample.data = (SHORT *)modFile.getSafePointer( sample.length );
            if ( sample.data == nullptr )
                return 0;            // temp DEBUG:

            samples_[iSample] = new Sample;
            sample.dataType = SAMPLEDATA_SIGNED_8BIT;
            samples_[iSample]->load( sample );
        }   
        fileOffset += sample.length; // ??
        instruments_[iSample] = new Instrument;
        instruments_[iSample]->load( instrument );

#ifdef debug_mod_play_samples
        if ( showDebugInfo_ )
            playSampleNr( iSample );
#endif
    }
    // And now the patterns. First, go to the right offset
    modFile.absSeek( patternDataOffset );
    if ( modFile.getIOError() != VIRTFILE_NO_ERROR ) 
        return 0;

    // Now read the patterns and convert them into the internal format
    modFile.absSeek( patternDataOffset );
    for ( unsigned patternNr = 0; patternNr < nPatterns_; patternNr++ ) {
        // to redo in a safer way
        if ( flt8Err )
            if ( convertFlt8Pattern( modFile ) )
                return 0;
        if ( loadModPattern( modFile,patternNr ) )
            return 0;
    }
    
    isLoaded_ = true;

    // Apotheosaic debug info ;)
    if ( showDebugInfo_ )
        std::cout << std::dec
            << "\n"
            << "\nFilename             = " << fileName_.c_str()
            << "\nis Loaded            = " << (isLoaded() ? "Yes" : "No")
            << "\nnChannels            = " << nChannels_
            << "\nnInstruments         = " << nInstruments_
            << "\nnSamples             = " << nSamples_
            << "\nnPatterns            = " << nPatterns_
            << "\nSong Title           = " << songTitle_.c_str()
            << "\nis CustomRepeat      = " << (isCustomRepeat() ? "Yes" : "No")
            << "\nSong Length          = " << songLength_
            << "\nSong Restart Positn. = " << songRestartPosition_
            << "\nNST File             = " << (nstFile ? "Yes" : "No")
            << "\n.WOW File            = " << (wowFile ? "Yes" : "No")
            << "\nFile Tag             = " << trackerTag_.c_str()
            << "\nTotal Samples Size   = " << sampleDataSize
            << "\nptnHdr               = " << patternHeader
            << "\nptnCalc              = " << patternCalc
            << "\nRest from Divide     = " 
            << patternDivideRest << " (should be zero)";
    return 0;
}



// convert 8 chn startrekker patterns to regular ones
int convertFlt8Pattern( VirtualFile& modFile )
{
    unsigned flt[8 * MOD_ROWS];   // 4 bytes / note * 8 channels * 64 rows
    unsigned *p = (unsigned *)modFile.getSafePointer( sizeof( flt ) );
    if ( p == nullptr )
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
    std::vector<Note> patternData( nChannels_ * MOD_ROWS );
    //Note        *iNote,*patternData;   // old
    unsigned char j1,j2,j3,j4;

    std::vector<Note>::iterator iNote = patternData.begin();

    //patterns_[patternNr] = new Pattern;              
    //patternData = new Note[nChannels_ * MOD_ROWS];   // old
    //patterns_[patternNr]->initialise( nChannels_,MOD_ROWS,patternData ); // old
    //iNote = patternData; // old

    for ( unsigned n = 0; n < (nChannels_ * MOD_ROWS); n++ ) {
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
        remapModEffects( iNote->effects[1] );

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
    //patterns_[patternNr] = new Pattern( nChannels_,MOD_ROWS,std::move( pattern ) );
    patterns_[patternNr] = new Pattern( nChannels_,MOD_ROWS,patternData );
    return 0;
}

void remapModEffects( Effect& remapFx )
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
int nBadComment( char *comment )
{
    char allowed[] = "\xD\xE !\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`" \
        "abcdefghijklmnopqrstuvwxyz{|}~";
    int r = 0;
    for ( int i = 0; i < MAX_SAMPLENAME_LENGTH; i++ )
        if ( !strchr( allowed,tolower( comment[i] ) ) ) 
            r++;
    return r;
}

// this fn returns the number of channels based on the format tag (0 on error)
int tagID( std::string tagID,bool &flt8Err,unsigned& trackerType )
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
    } else if ( tagID == "OCTA" ) chn = 8;
    else if ( tagID == "CD81" ) chn = 8;
    else if ( (tagID[2] == 'C') && (tagID[3] == 'H') ) {
        chn = ((unsigned char)tagID[0] - 48) * 10 +
            (unsigned char)tagID[1] - 48;
        if ( (chn > 32) || (chn < 10) )
            chn = 0;
        //else trackerType = TRACKER_FT2;
    } else if ( (tagID[0] == 'T') && (tagID[1] == 'D') && (tagID[2] == 'Z') ) {
        chn = (unsigned char)tagID[3] - 48;
        if ( (chn < 1) || (chn > 3) )
            chn = 0; // only values 1..3 are valid
                     //else trackerType = TRACKER_FT2;
    } else if ( (tagID[1] == 'C') && (tagID[2] == 'H') && (tagID[3] == 'N') ) {
        chn = (unsigned char)tagID[0] - 48;
        if ( (chn < 5) || (chn > 9) )
            chn = 0; // only values 5..9 are valid
                     //else trackerType = TRACKER_FT2;
    } else
        chn = 0;
    return chn;
}

// returns true if file has extension .WOW
bool isWowFile( std::string fileName )
{
    int     i = 0;
    const char *p = fileName.c_str();
    while ( *p && (i < 255) ) {
        p++;
        i++;
    }
    if ( (i < 4) || (i >= 255) )
        return false;
    p -= 4;
    return (!_stricmp( p,".wow" )); // case insensitive compare
}