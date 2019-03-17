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

#include "Module.h"
#include "virtualfile.h"

#define debug_mod_loader

#ifdef debug_mod_loader
extern const char *noteStrings[2 + MAXIMUM_NOTES];
#endif

// Constants for the .MOD Format:
#define MOD_LIMIT                           8    // nr of illegal chars permitted in smp names
#define MOD_ROWS                            64   // always 64 rows in a MOD pattern
#define MOD_MAX_SONGNAME_LENGTH             20
#define MOD_MAX_PATTERNS                    128
#define MOD_MAX_CHANNELS                    32
#define MOD_DEFAULT_BPM                     125
#define MOD_DEFAULT_TEMPO                   6
#define MOD_MAX_PERIOD                      7248 // chosen a bit arbitrarily!
//#define MAX_SAMPLES_SIZE_NST  (0xFFFF * 2 * 15)
//#define MAX_SAMPLES_SIZE_MK   (0xFFFF * 2 * 31)

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
inline AMIGAWORD SwapW(AMIGAWORD d) {
    return (d >> 8) | (d << 8);
}

/* 
    this fn returns the number of unprobable characters for a sample comment.
    In the olden days, the  ASCII control characters 13 and 14 (D and E in 
    hexadecimal) represented single and double music notes, and some BBS's 
    inserted these in the sample name strings to show the mod was downloaded 
    from there.
*/
int nBadComment(char *comment) {
    char allowed[] =    "\xD\xE !\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`" \
                        "abcdefghijklmnopqrstuvwxyz{|}~";
    int r = 0;
    for (int i = 0; i < MAX_SAMPLENAME_LENGTH; i++)
                                if (!strchr(allowed, tolower(comment[i]))) r++;
    return r;
}

// this fn returns the number of channels based on the format tag (0 on error)
int tagID(std::string tagID, bool &flt8Err, unsigned& trackerType) {
    int     chn;     
    trackerType = TRACKER_PROTRACKER;
    flt8Err = false;
    if ( tagID == "M.K." ) chn = 4;
    else if ( tagID == "M!K!" ) chn = 4; 
    else if ( tagID == "FLT4" ) chn = 4;
    else if ( tagID == "FLT8" ) { chn = 8; flt8Err = true; } 
    else if ( tagID == "OCTA" ) chn = 8;
    else if ( tagID == "CD81" ) chn = 8;
    else if ( (tagID[2] == 'C') && (tagID[3] == 'H') ) {
        chn = ((unsigned char)tagID[0] - 48) * 10 +
               (unsigned char)tagID[1] - 48;
        if ( (chn > 32) || (chn < 10) ) chn = 0;
        //else trackerType = TRACKER_FT2;
    } else if ( (tagID[0] == 'T') && (tagID[1] == 'D') && (tagID[2] == 'Z') ) {
        chn = (unsigned char)tagID[3] - 48;
        if ( (chn < 1) || (chn > 3) ) chn = 0; // only values 1..3 are valid
        //else trackerType = TRACKER_FT2;
    } else if ( (tagID[1] == 'C') && (tagID[2] == 'H') && (tagID[3] == 'N') ) {
        chn = (unsigned char)tagID[0] - 48;
        if ( (chn < 5) || (chn > 9) ) chn = 0; // only values 5..9 are valid
        //else trackerType = TRACKER_FT2;
    } else chn = 0;
    return chn;
}

// returns true if file has extension .WOW
bool isWowFile(std::string fileName) {
    int     i = 0;
    const char *p = fileName.c_str();
    while(*p && ( i < 255)) { p++; i++; }
    if ((i < 4) || (i >= 255)) { return false; } 
    p -= 4; 
    return (!_stricmp(p, ".wow")); // case insensitive compare
}

int Module::loadModFile() {
    bool        smpErr   = false;   // if first 15 smp names are garbage
    bool        nstErr   = false;   // if next  16 smp names are garbage
    bool        ptnErr   = false;   // if there seem to be an invalid nr of ptns
    bool        tagErr   = false;   // true if no valid tag was found
    bool        flt8Err  = false;   // if FLT8 pattern conversion will be needed
    bool        wowFile  = false;   // if the file extension was .WOW rather than .MOD
    bool        nstFile  = false;   // if it is an NST file (15 instruments)
    unsigned    patternHeader, patternCalc, patternDivideRest;
    unsigned    k, sampleDataSize, sampleDataOffset, patternDataOffset, fileOffset;
    unsigned    fileSize;
    /*
    char        *buf, *bufp;
    HeaderNST   *headerNST;
    HeaderMK    *headerMK;
    */
    VirtualFile modFile( fileName_ );
    if ( modFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;
    fileSize = modFile.fileSize();

    /*
    std::ifstream::pos_type  fileSize = 0;
    std::ifstream            modFile(fileName_,std::ios::in| std::ios::binary| std::ios::ate);
    */
    // initialize mod specific variables:
    minPeriod_ = 14;    // periods[11 * 12 - 1]
    maxPeriod_ = 27392; // periods[0]  // ?

    isLoaded_ = false;
    /*
    // load file into byte buffer and then work on that buffer only
    if (!modFile.is_open()) return 0; // exit on I/O error
    fileSize = modFile.tellg();
    buf = new char [(int)fileSize];
    modFile.seekg ( 0,std::ios::beg );
    modFile.read ( buf, fileSize );
    modFile.close();
    headerNST = (HeaderNST *)buf;
    headerMK  = (HeaderMK  *)buf;
    */
    HeaderMK    modHeader;
    HeaderNST  *headerNST = (HeaderNST *)(&modHeader);
    HeaderMK   *headerMK  = &modHeader;

    modFile.read( &modHeader,sizeof( HeaderMK ) );
    if ( modFile.getIOError() > VIRTFILE_EOF ) return 0;

    // check extension for the wow factor ;)
    wowFile = isWowFile(fileName_);
    // check if a valid tag is present
    trackerTag_ = "";
    trackerTag_ += headerMK->tag[0];
    trackerTag_ += headerMK->tag[1];
    trackerTag_ += headerMK->tag[2];
    trackerTag_ += headerMK->tag[3];
    nChannels_ = tagID( trackerTag_,flt8Err,trackerType_ );
    if (!nChannels_) tagErr = true;
    // read sample names, this is how we differentiate a 31 instruments file 
    // from an old format 15 instruments file
    // if there is no tag then this is probably not a MOD file!
    k = 0;
    for (int i = 0 ; i < 15; i++) k += nBadComment(headerMK->samples[i].name);
    if(k > MOD_LIMIT ) smpErr = true;
    // no tag means this is an old NST - MOD file!
    k = 0;
    for (int i = 15; i < 31; i++) k += nBadComment(headerMK->samples[i].name);
    if(k > MOD_LIMIT ) nstErr = true;
    if (tagErr && nstErr && (!smpErr)) { 
        nstFile = true; 
        nChannels_ = 4; 
        trackerTag_ = "NST";
    }
    patternCalc = fileSize; 
    if (nstFile)    { nInstruments_ = 15; patternCalc -= sizeof(HeaderNST); } 
    else            { nInstruments_ = 31; patternCalc -= sizeof(HeaderMK ); }
    nSamples_ = 0;

    sampleDataSize = 0; 
    for(unsigned i = 0; i < nInstruments_; i++) {
        headerMK->samples[i].length       = SwapW(headerMK->samples[i].length      );
        headerMK->samples[i].repeatOffset = SwapW(headerMK->samples[i].repeatOffset);
        headerMK->samples[i].repeatLength = SwapW(headerMK->samples[i].repeatLength);
        sampleDataSize += ((unsigned)headerMK->samples[i].length) << 1;
    }
    patternCalc -= sampleDataSize;
    if (nChannels_) {
        // pattern size = 64 rows * 4 b. per note * nChn = 256 * nChn
        int     i = (nChannels_ << 8);   
        patternDivideRest = patternCalc % i;
        patternCalc /= i;
    }
    // verify nr of patterns using pattern table
    patternHeader = 0;
    if (nstFile) {
        for (int i = 0; i < MOD_MAX_PATTERNS; i++) {
            k = patternTable_[i] = headerNST->patternTable[i];
            if ( k > patternHeader) patternHeader = k;
        }
        songRestartPosition_ = headerNST->restartPosition;
        songLength_ = headerNST->songLength;
    } else {
        if ( flt8Err )
            for (int i = 0; i < MOD_MAX_PATTERNS; i++) 
                                            headerMK->patternTable[i] >>= 1;
        for (int i = 0; i < MOD_MAX_PATTERNS; i++) {
            k = patternTable_[i] = headerMK->patternTable[i];
            if ( k > patternHeader) patternHeader = k;
        }
        songRestartPosition_ = headerMK->restartPosition;
        songLength_ = headerMK->songLength;
    }
    // patterns are numbered starting from zero
    patternHeader++;
    if ( patternHeader > MOD_MAX_PATTERNS )
    {
#ifdef debug_mod_loader
        std::cout << "\nPattern table has illegal values, exiting.\n";
#endif
        return 0;
    }

    panningStyle_ = PANNING_STYLE_MOD;
    defaultTempo_ = MOD_DEFAULT_TEMPO;
    defaultBpm_   = MOD_DEFAULT_BPM;
    useLinearFrequencies_ = false;

    if (patternHeader > MOD_MAX_PATTERNS) ptnErr = true;
    if ( (songRestartPosition_ > songLength_) ||
         (songRestartPosition_ == 127)) songRestartPosition_ = 0;
    isCustomRepeat_ = songRestartPosition_ != 0;
    // now interpret the obtained info
    // this is not a .MOD file, or it's compressed
    if (ptnErr && tagErr && smpErr) { 
        //delete buf;
        return 0;
    }
    // check file integrity, correct nr of channels if necessary
    nPatterns_ = patternHeader;
    if (!tagErr) {              // ptnCalc and chn were initialised
        if (wowFile && ((patternHeader << 1) == patternCalc)) nChannels_ = 8;
    } else {
        if (!nstFile) {         // ptnCalc and chn were not initialised
            patternDivideRest = patternCalc % (MOD_ROWS * 4);
            patternCalc >>= 8;
            nChannels_ = patternCalc / patternHeader; 
            if ( nChannels_ > MOD_MAX_CHANNELS )
            {
#ifdef debug_mod_loader
                std::cout << "\nUnable to detect # of channels, exiting.\n";
#endif
                return 0;
            }
        }
    }

    //if ((patternCalc < nPatterns_) && (!patternDivideRest)) nPatterns_ = patternCalc;
    patternDataOffset = (nstFile ? sizeof(HeaderNST) : sizeof(HeaderMK));
    sampleDataOffset = patternDataOffset + nPatterns_ * nChannels_ * MOD_ROWS * 4;
    if ((sampleDataOffset + sampleDataSize) > fileSize) {
        unsigned    missingData = (sampleDataOffset + sampleDataSize) - (int)fileSize;
        unsigned    lastInstrument = nInstruments_;
#ifdef debug_mod_loader
        std::cout << "\nWarning! File misses Sample Data!\n";
        std::cout << "\nnPatterns          = " << nPatterns_;
        std::cout << "\nPatternHeader      = " << patternHeader;
        std::cout << "\nPatternCalc        = " << patternCalc;
        std::cout << "\nSample Data Offset = " << sampleDataOffset;
        std::cout << "\nSample Data Size   = " << sampleDataSize;
        std::cout << "\nOffset + Data      = " << (sampleDataOffset + sampleDataSize);
        std::cout << "\nFile Size          = " << fileSize;
        std::cout << "\nDifference         = " << missingData;
#endif
        while (missingData && lastInstrument) {
            ModSampleHeader *sample = &(headerMK->samples[lastInstrument - 1]);
            unsigned length       = ((unsigned)sample->length      ) << 1;
            unsigned repeatOffset = ((unsigned)sample->repeatOffset) << 1;
            unsigned repeatLength = ((unsigned)sample->repeatLength) << 1;

            if (missingData > length) {
                missingData -= length;
                length       = 0;
                repeatOffset = 0;
                repeatLength = 0;
            } else {
                length -= missingData;
                missingData = 0;
                if (repeatOffset > length) {
                    repeatOffset = 0;
                    repeatLength = 0;
                } else {
                    if ((repeatOffset + repeatLength) > length) {
                        repeatLength = length - repeatOffset;
                    }
                }
            }
            sample->length       = (unsigned)length       >> 1;
            sample->repeatOffset = (unsigned)repeatOffset >> 1;
            sample->repeatLength = (unsigned)repeatLength >> 1;
            lastInstrument--;
        }
        if ( missingData ) {
#ifdef debug_mod_loader
            std::cout << "\nNo Sample data! Some pattern data is missing!\n";
#endif
            //delete buf;
            return 0;
        }
    }  
    // take care of default panning positions:
    //std::cout << "  nChannels = " << nChannels_ << " " << std::endl;
    for ( unsigned i = 0; i < nChannels_; i++ )
    {
        switch ( i & 3 )
        {
            case 0:
            case 3:
            {
                defaultPanPositions_[i] = PANNING_FULL_LEFT;
                break;
            }
            case 1:
            case 2:
            {
                defaultPanPositions_[i] = PANNING_FULL_RIGHT;
                break;
            }
        }
    }
    // Now start with copying the data
    // we start with the song title :)
    songTitle_ = ""; 
    for ( int i = 0; i < MOD_MAX_SONGNAME_LENGTH; i++ ) songTitle_ += headerMK->songTitle[i]; 

    // now, the sample headers & sample data.
    fileOffset = sampleDataOffset;
    for ( unsigned iSample = 1; iSample <= nInstruments_; iSample++ ) {
        InstrumentHeader    instrument;
        SampleHeader        sample;
        for ( char *c = headerMK->samples[iSample - 1].name; 
            c < headerMK->samples[iSample - 1].name + MAX_SAMPLENAME_LENGTH;
            c++ )
        {
            sample.name += *c;
            instrument.name += *c;
        }
        instrument.nSamples = 1; // redundant?
        for ( int i = 0; i < MAXIMUM_NOTES; i++ )
            instrument.sampleForNote[i] = iSample;
        sample.length       = 
            ((unsigned)headerMK->samples[iSample - 1].length)       << 1;
        sample.repeatOffset = 
            ((unsigned)headerMK->samples[iSample - 1].repeatOffset) << 1;
        sample.repeatLength = 
            ((unsigned)headerMK->samples[iSample - 1].repeatLength) << 1;
        sample.volume       = headerMK->samples[iSample - 1].linearVolume;
        if ( sample.volume > MAX_VOLUME ) sample.volume = MAX_VOLUME;
        if ( sample.repeatOffset > sample.length ) sample.repeatOffset = 3;
        sample.isRepeatSample = (sample.repeatLength > 2);
        if ( (sample.repeatOffset + sample.repeatLength) > sample.length )
            sample.repeatLength = sample.length - sample.repeatOffset;
        // convert signed nibble to int and scale it up
        sample.finetune = (signed char)(headerMK->samples[iSample - 1].finetune << 4);

        if (sample.length > 2) {
            nSamples_++;
            
            //sample.data = (SHORT *)(buf + fileOffset);
            modFile.absSeek( fileOffset );
            //MemoryBlock<char> sampleData = modFile.getPointer<char>( sample.length );
            sample.data = (SHORT *)modFile.getSafePointer( sample.length );
            // temp DEBUG:
            if ( sample.data == nullptr )
            {
                return 0;
            }

            samples_[iSample] = new Sample;
            sample.dataType = SAMPLEDATA_SIGNED_8BIT;
            samples_[iSample]->load( sample );
            //fileOffset += sample.length; // ??
        }   
        fileOffset += sample.length; // ??
        instruments_[iSample] = new Instrument;
        instruments_[iSample]->load( instrument );
#ifdef debug_mod_loader
        /*
        std::cout << "\nSample " << iSample << ": name     = " << instruments_[iSample - 1]->getName().c_str();
        if (!instruments_[iSample]->getSample(0)) _getch();

        if (instruments_[iSample]->getSample(0)) {
            HWAVEOUT        hWaveOut;
            WAVEFORMATEX    waveFormatEx;
            MMRESULT        result;
            WAVEHDR         waveHdr;

            std::cout << "\nSample " << iSample << ": length   = " << instruments_[iSample - 1]->getSample(0)->getLength();
            std::cout << "\nSample " << iSample << ": rep ofs  = " << instruments_[iSample - 1]->getSample(0)->getRepeatOffset();
            std::cout << "\nSample " << iSample << ": rep len  = " << instruments_[iSample - 1]->getSample(0)->getRepeatLength();
            std::cout << "\nSample " << iSample << ": volume   = " << instruments_[iSample - 1]->getSample(0)->getVolume();
            std::cout << "\nSample " << iSample << ": finetune = " << instruments_[iSample - 1]->getSample(0)->getFinetune();

            // not very elegant but hey, is debug code lol
            if (!instruments_[i]->getSample(0)->getData()) break; 

            waveFormatEx.wFormatTag     = WAVE_FORMAT_PCM;
            waveFormatEx.nChannels      = 1;
            waveFormatEx.nSamplesPerSec = 8363; // frequency
            waveFormatEx.wBitsPerSample = 16;
            waveFormatEx.nBlockAlign    = waveFormatEx.nChannels * 
                                         (waveFormatEx.wBitsPerSample >> 3);
            waveFormatEx.nAvgBytesPerSec= waveFormatEx.nSamplesPerSec * 
                                          waveFormatEx.nBlockAlign;
            waveFormatEx.cbSize         = 0;

            result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormatEx, 
                                 0, 0, CALLBACK_NULL);
            if (result != MMSYSERR_NOERROR) {
                if (!i) std::cout << "\nError opening wave mapper!\n";
            } else {
                int retry = 0;
                if (!i) std::cout << "\nWave mapper successfully opened!\n";
                waveHdr.dwBufferLength = instruments_[i]->getSample(0)->getLength () * 
                                         waveFormatEx.nBlockAlign;
                waveHdr.lpData = (LPSTR)(instruments_[i]->getSample(0)->getData ());
                waveHdr.dwFlags = 0;

                result = waveOutPrepareHeader(hWaveOut, &waveHdr, 
                                                              sizeof(WAVEHDR));
                while ((result != MMSYSERR_NOERROR) && (retry < 10)) {
                    retry++;
                    std::cout << "\nError preparing wave mapper header!";
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

                //while(waveOutUnprepareHeader(hWaveOut, &waveHdr, 
                //                  sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) { Sleep(50); }                

                waveOutReset(hWaveOut);
                waveOutClose(hWaveOut);
            }
		}
    */
#endif
    }
    // read the patterns now
    //bufp = buf + patternDataOffset;
    modFile.absSeek( patternDataOffset );
    if ( modFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;
    // convert 8 chn startrekker patterns to regular ones
    if ( flt8Err ) // to redo in a safer way
    {
        unsigned    flt[8 * MOD_ROWS];   // 4 bytes / note * 8 channels * 64 rows
        unsigned    *ps, *pd, *p1, *p2;

        //ps = (unsigned *)bufp; 
        ps = (unsigned *)modFile.getSafePointer( sizeof( flt ) * nPatterns_ );
        pd = ps;
        for ( unsigned ptn = 0; ptn < nPatterns_; ptn++ ) 
        {
            for ( int i = 0; i < 2 * 4 * MOD_ROWS; i++ ) 
            {// copy data to tmp buf
                flt[i] = *ps++;                  // 2 ptn * 4 chn * 64 rows
            }
            p1 = flt;                            // idx to 1st ptn inside flt buf
            p2 = flt + 4 * MOD_ROWS;             // idx to 2nd ptn inside flt buf
            for (int i = 0; i < MOD_ROWS; i++) {
                *pd++ = *p1++; *pd++ = *p1++; *pd++ = *p1++; *pd++ = *p1++;
                *pd++ = *p2++; *pd++ = *p2++; *pd++ = *p2++; *pd++ = *p2++;
            }
        }
    }
    // Now read them and convert them into the internal format
    modFile.absSeek( patternDataOffset );
    for (unsigned  i = 0; i < nPatterns_; i++) {
        Note        *iNote, *patternData;
        //unsigned    j1, j2, j3, j4;
        unsigned char j1,j2,j3,j4;

        patterns_[i] = new Pattern;
        patternData = new Note[nChannels_ * MOD_ROWS];
        patterns_[i]->initialise(nChannels_, MOD_ROWS, patternData);
        iNote = patternData;
        for (unsigned n = 0; n < (nChannels_ * MOD_ROWS); n++) {
            /*
            j1 = *((unsigned char *)bufp++);
            j2 = *((unsigned char *)bufp++);
            j3 = *((unsigned char *)bufp++);
            j4 = *((unsigned char *)bufp++);
            */
            modFile.read( &j1,sizeof( unsigned char ) );
            modFile.read( &j2,sizeof( unsigned char ) );
            modFile.read( &j3,sizeof( unsigned char ) );
            modFile.read( &j4,sizeof( unsigned char ) );
            // if we read the samples without issue the patterns should be fine?
            if ( modFile.getIOError() != VIRTFILE_NO_ERROR ) return 0;

            iNote->effects[1].effect = j3 & 0xF;
            iNote->effects[1].argument = j4;
            //iNote->period = j2;
            unsigned period = j2;
            j4 = (j1 & 0xF0) + (j3 >> 4);
            iNote->instrument = j4 & 0x1F;
            /* iNote-> */period += ((j1 & 0xF) << 8) + ((j4 >> 5) << 12);

            iNote->note = 0;
            if ( /* iNote-> */ period ) {
                unsigned j;
                for (j = 0; j < (MAXIMUM_NOTES); j++) {
                  if ( /* iNote-> */ period >= periods[j]) break;
                }
                // ***** added for believe.mod:
                int periodA;
                int periodB;                
                if ( j ) periodA = (int)periods[j - 1];
                else periodA = MOD_MAX_PERIOD;
                if( j < MAXIMUM_NOTES ) periodB = (int)periods[j];
                else periodB = 0;
                int diffA = periodA - (int)period;
                int diffB = (int)period - periodB;
                if ( diffA < diffB ) j--;
                // ***** end of addition                
                if (j >= (MAXIMUM_NOTES)) iNote->note = 0;
                else                      iNote->note = j + 1 - 24; // two octaves down
            }
            // do some error checking & effect remapping:
            switch ( iNote->effects[1].effect ) {
                case 0: // arpeggio
                {
                    if ( iNote->effects[1].argument )
                        iNote->effects[1].effect = ARPEGGIO;
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
                    if ( !iNote->effects[1].argument )
                        iNote->effects[1].effect = NO_EFFECT;
                    break;
                }
                case VOLUME_SLIDE:
                {
                    unsigned& argument = iNote->effects[1].argument;
                    if ( !argument )
                        iNote->effects[1].effect = NO_EFFECT;
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
                    unsigned& argument = iNote->effects[1].argument;
                    if ( !argument )
                        iNote->effects[1].effect = TONE_PORTAMENTO;
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
                    unsigned& argument = iNote->effects[1].argument;
                    if ( !argument )
                        iNote->effects[1].effect = VIBRATO;
                    {
                        // in .mod & .xm files volume slide up has
                        // priority over volume slide down
                        unsigned volUp = argument & 0xF0;
                        unsigned volDn = argument & 0x0F;
                        if ( volUp && volDn ) argument = volUp;
                    }
                    break;
                }
                case SET_VOLUME :
                {
                    if ( iNote->effects[1].argument > MAX_VOLUME )
                            iNote->effects[1].argument = MAX_VOLUME;
                    break;
                }
                case SET_TEMPO :
                {
                    if ( iNote->effects[1].argument == 0 ) {
                        iNote->effects[1].effect = NO_EFFECT;
                    } else {
                        if ( iNote->effects[1].argument > 0x1F ) {
                            iNote->effects[1].effect = SET_BPM;
                        }
                    }
                    break;
                }
            }
#ifdef debug_mod_loader
            if (i == 0) 
            {
                char    hex[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

                if (!(n % nChannels_)) std::cout << "\n";
                else std::cout << "|";
                std::cout << noteStrings[iNote->note];
                std::cout << ":";
                if (iNote->instrument < 10) std::cout << "0";
                std::cout << iNote->instrument;

                if (iNote->effects[1].effect > 0xF) 
                    std::cout << (char)(iNote->effects[1].effect + 55);
                else    std::cout << hex[iNote->effects[1].effect];
                std::cout << hex[iNote->effects[1].argument >> 4];
                std::cout << hex[iNote->effects[1].argument & 0xF];
            }
#endif
            iNote++;
        }
    }
    //delete buf;
    isLoaded_ = true;

#ifdef debug_mod_loader
    std::cout << "\n";
    std::cout << "\nFilename             = " << fileName_.c_str();
    std::cout << "\nis Loaded            = " << (isLoaded() ? "Yes" : "No");
    std::cout << "\nnChannels            = " << nChannels_;
    std::cout << "\nnInstruments         = " << nInstruments_;
    std::cout << "\nnSamples             = " << nSamples_;
    std::cout << "\nnPatterns            = " << nPatterns_;
    std::cout << "\nSong Title           = " << songTitle_.c_str();
    std::cout << "\nis CustomRepeat      = " << (isCustomRepeat() ? "Yes" : "No");
    std::cout << "\nSong Length          = " << songLength_;
    std::cout << "\nSong Restart Positn. = " << songRestartPosition_;
    std::cout << "\nNST File             = " << (nstFile ? "Yes" : "No");
    std::cout << "\n.WOW File            = " << (wowFile ? "Yes" : "No");
    std::cout << "\nFile Tag             = " << trackerTag_.c_str();
    std::cout << "\nTotal Samples Size   = " << sampleDataSize;
    std::cout << "\nptnHdr               = " << patternHeader;
    std::cout << "\nptnCalc              = " << patternCalc;
    std::cout << "\nRest from Divide     = " << patternDivideRest << " (should be zero)";
#endif
    return 0;
}
