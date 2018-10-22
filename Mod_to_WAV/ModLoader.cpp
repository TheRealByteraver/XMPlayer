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

using namespace std;

//#define debug_mod_loader

extern const char *noteStrings[2 + MAXIMUM_NOTES];

// Constants for the .MOD Format:
#define LIMIT                               8    // nr of illegal chars permitted in smp names
#define MOD_ROWS                            64   // always 64 rows in a MOD pattern
#define MOD_MAX_SONGNAME_LENGTH             20
#define MOD_MAX_PATTERNS                    128
#define MOD_DEFAULT_BPM                     125
#define MOD_DEFAULT_TEMPO                   6
//#define MAX_SAMPLES_SIZE_NST  (0xFFFF * 2 * 15)
//#define MAX_SAMPLES_SIZE_MK   (0xFFFF * 2 * 31)

#pragma pack (1) 
struct ModSampleHeader { 
    char            name[MAX_SAMPLENAME_LENGTH];
    AMIGAWORD       length;         // Big-End Word; * 2 = samplelength in bytes
    char            finetune;       // This is in fact a signed nibble 
    unsigned char   linearVolume;
    AMIGAWORD       repeatOffset;   // Big-End Word; * 2 = RepeatOffset in bytes
    AMIGAWORD       repeatLength;   // Big-End Word; * 2 = RepeatLength in bytes 
};

// NST header layout:
struct HeaderNST {
    char            songTitle[MOD_MAX_SONGNAME_LENGTH];          
    ModSampleHeader samples[15];            // 15 * (22+2+6)  = 15 * 30 = 450
    unsigned char   songLength;             // 1
    unsigned char   restartPosition;        // 1
    unsigned char   patternTable[128];      // 128 -> total = 472+128 = 600
};

// M.K. header layout:
struct HeaderMK {
    char            songTitle[MOD_MAX_SONGNAME_LENGTH];
    ModSampleHeader samples[31];
    unsigned char   songLength;
    unsigned char   restartPosition;
    unsigned char   patternTable[128];
    char            tag[4];                 // total = 600 + 4 + 16 * 30 = 604 + 480 = 1084
};

// restore default alignment
#pragma pack (8)                            

// this fn swaps a 16 bit word's low and high order bytes
inline AMIGAWORD SwapW(AMIGAWORD d) {
    return (d >> 8) + (d << 8);
}

// this fn returns the number of unprobable characters for a sample comment
int nBadComment(char *comment) {
    char allowed[] = "abcdefghijklmnopqrstuvwxyz 0123456789&@$*#!?:;.,/=+-~()[]{}\\";
    int r = 0;
    for (int i = 0; i < MAX_SAMPLENAME_LENGTH; i++)
                                if (!strchr(allowed, tolower(comment[i]))) r++;
    return r;
}

// this fn returns the number of channels based on the format tag (0 on error)
int tagID(char *dstr, bool &flt8Err) {
    int     chn; // chn = number of channels
    flt8Err = false;
    if      (!strcmp(dstr, "M.K.")) chn = 4;
    else if (!strcmp(dstr, "M!K!")) chn = 4;
    else if (!strcmp(dstr, "FLT4")) chn = 4;
    else if (!strcmp(dstr, "FLT8")) { chn = 8; flt8Err = true; }
    else if (!strcmp(dstr, "OCTA")) chn = 8;
    else if (!strcmp(dstr, "CD81")) chn = 8;
    else if ((dstr[2] == 'C') && (dstr[3] == 'H')) {
            chn = ((unsigned char)dstr[0] - 48) * 10 +
                   (unsigned char)dstr[1] - 48;
            if ((chn > 32) || (chn < 10)) chn = 0; 
    }
    else if ((dstr[0] == 'T') && (dstr[1] == 'D') && (dstr[2] == 'Z')) {
            chn = (unsigned char)dstr[3] - 48;
            if ((chn < 1) || (chn > 3)) chn = 0; // only values 1..3 are valid
    }
    else if ((dstr[1] == 'C') && (dstr[2] == 'H') && (dstr[3] == 'N')) {
            chn = (unsigned char)dstr[0] - 48;
            if ((chn < 5) || (chn > 9)) chn = 0; // only values 5..9 are valid
    } else chn = 0;
    return chn;
}

// returns true if file has extension .WOW
bool isWowFile(char *p) {
    int     i = 0;

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
    char        *buf, *bufp;
    HeaderNST   *headerNST;
    HeaderMK    *headerMK;
    ifstream::pos_type  fileSize = 0;
    ifstream            modFile(fileName_, ios::in|ios::binary|ios::ate);

    // load file into byte buffer and then work on that buffer only
    isLoaded_ = false;
    if (!modFile.is_open()) return 0; // exit on I/O error
    fileSize = modFile.tellg();
    buf = new char [fileSize];
    modFile.seekg (0, ios::beg);
    modFile.read (buf, fileSize);
    modFile.close();
    headerNST = (HeaderNST *)buf;
    headerMK  = (HeaderMK  *)buf;
    // check extension for the wow factor ;)
    wowFile = isWowFile(fileName_);
    // check if a valid tag is present
    trackerTag_ = new char[4 + 1];
    trackerTag_[4] = '\0';
    strncpy_s(trackerTag_, 5, headerMK->tag, 4);
    nChannels_ = tagID(trackerTag_, flt8Err);
    if (!nChannels_) tagErr = true;
    // read sample names, this is how we differentiate a 31 instruments file 
    // from an old format 15 instruments file
    // if there is no tag then this is probably not a MOD file!
    k = 0;
    for (int i = 0 ; i < 15; i++) k += nBadComment(headerMK->samples[i].name);
    if(k > LIMIT) smpErr = true; 
    // no tag means this is an old NST - MOD file!
    k = 0;
    for (int i = 15; i < 31; i++) k += nBadComment(headerMK->samples[i].name);
    if(k > LIMIT) nstErr = true; 
    if (tagErr && nstErr && (!smpErr)) { 
        nstFile = true; 
        nChannels_ = 4; 
        strncpy_s(trackerTag_, 5, "NST", _TRUNCATE);
    }

    patternCalc = (unsigned)fileSize; // I have yet to encounter a > 4 gb xm file
    if (nstFile)    { nInstruments_ = 15; patternCalc -= sizeof(HeaderNST); } 
    else            { nInstruments_ = 31; patternCalc -= sizeof(HeaderMK ); }
    nSamples_ = 0;

    sampleDataSize = 0; 
    for(unsigned i = 0; i < nInstruments_; i++) {
        headerMK->samples[i].length       = SwapW(headerMK->samples[i].length      );
        headerMK->samples[i].repeatOffset = SwapW(headerMK->samples[i].repeatOffset);
        headerMK->samples[i].repeatLength = SwapW(headerMK->samples[i].repeatLength);
        sampleDataSize += ((unsigned)headerMK->samples[i].length) << 1;
        //sampleDataSize += ((unsigned)SwapW(headerMK->samples[i].length)) << 1;
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
        if (flt8Err)
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

    panningStyle_ = PANNING_STYLE_MOD;
    defaultTempo_ = MOD_DEFAULT_TEMPO;
    defaultBpm_   = MOD_DEFAULT_BPM;
    useLinearFrequencies_ = false;

    if (patternHeader > MOD_MAX_PATTERNS) ptnErr = true;
    if (songRestartPosition_ > songLength_) songRestartPosition_ = 0;
    isCustomRepeat_ = songRestartPosition_ != 0;
    // now interpret the obtained info
    // this is not a .MOD file, or it's compressed
    if (ptnErr && tagErr && smpErr) { 
        delete buf;
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
        }
    }

    //if ((patternCalc < nPatterns_) && (!patternDivideRest)) nPatterns_ = patternCalc;
    patternDataOffset = (nstFile ? sizeof(HeaderNST) : sizeof(HeaderMK));
    sampleDataOffset = patternDataOffset + nPatterns_ * nChannels_ * MOD_ROWS * 4;
    if ((sampleDataOffset + sampleDataSize) > (unsigned)fileSize) {
        unsigned    missingData = (sampleDataOffset + sampleDataSize) - fileSize;
        unsigned    lastInstrument = nInstruments_;
#ifdef debug_mod_loader
        cout << "\nWarning! File misses Sample Data!\n";
        cout << "\nnPatterns          = " << nPatterns_;
        cout << "\nPatternHeader      = " << patternHeader;
        cout << "\nPatternCalc        = " << patternCalc;
        cout << "\nSample Data Offset = " << sampleDataOffset;
        cout << "\nSample Data Size   = " << sampleDataSize;
        cout << "\nOffset + Data      = " << (sampleDataOffset + sampleDataSize);
        cout << "\nFile Size          = " << fileSize;
        cout << "\nDifference         = " << missingData;
#endif
        while (missingData && lastInstrument) {
            ModSampleHeader *sample = &(headerMK->samples[lastInstrument - 1]);
            /*
            unsigned length       = ((unsigned)SwapW(sample->length      ) << 1);
            unsigned repeatOffset = ((unsigned)SwapW(sample->repeatOffset) << 1);
            unsigned repeatLength = ((unsigned)SwapW(sample->repeatLength) << 1);
            */
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
        if (missingData) {
#ifdef debug_mod_loader
        cout << "\nNo Sample data! Some pattern data is missing!\n";
#endif
            delete buf;
            return 0;
        }
    }  
    // Now start with copying the data
    // we start with the song title :)
    songTitle_ = new char[MOD_MAX_SONGNAME_LENGTH + 1];
    songTitle_[MOD_MAX_SONGNAME_LENGTH] = '\0';
    strncpy_s(songTitle_, MOD_MAX_SONGNAME_LENGTH + 1, headerMK->songTitle, MOD_MAX_SONGNAME_LENGTH);
    //for (int i = 0; i < MOD_MAX_SONGNAME_LENGTH; i++) { songTitle_[i] = headerMK->songTitle[i]; }

    // now, the sample headers & sample data.
    fileOffset = sampleDataOffset;
    for (unsigned i = 0; i < nInstruments_; i++) {
        InstrumentHeader    instrument;
        SampleHeader        sample;
        char                sampleName[MAX_SAMPLENAME_LENGTH + 1];

        sampleName[MAX_SAMPLENAME_LENGTH] = '\0';
        strncpy_s(sampleName, MAX_SAMPLENAME_LENGTH + 1, headerMK->samples[i].name, MAX_SAMPLENAME_LENGTH);
        sample.name     = sampleName;
        instrument.name = sampleName; 
/*
        sample.length       = 
            ((unsigned)SwapW(headerMK->samples[i].length))       << 1;
        sample.repeatOffset = 
            ((unsigned)SwapW(headerMK->samples[i].repeatOffset)) << 1;
        sample.repeatLength = 
            ((unsigned)SwapW(headerMK->samples[i].repeatLength)) << 1;
        sample.volume       = headerMK->samples[i].linearVolume;
*/
        sample.length       = 
            ((unsigned)headerMK->samples[i].length)       << 1;
        sample.repeatOffset = 
            ((unsigned)headerMK->samples[i].repeatOffset) << 1;
        sample.repeatLength = 
            ((unsigned)headerMK->samples[i].repeatLength) << 1;
        sample.volume       = headerMK->samples[i].linearVolume;
        // better be safe than GPF ;)
        if (sample.volume > MAX_VOLUME) sample.volume = MAX_VOLUME;
        if (sample.repeatOffset > sample.length) sample.repeatOffset = 3;
        sample.isRepeatSample = (sample.repeatLength > 2);
        if ((sample.repeatOffset + sample.repeatLength) > sample.length)
            sample.repeatLength = sample.length - sample.repeatOffset;

        // convert signed nibble to int and scale it up
        sample.finetune = (signed char)(headerMK->samples[i].finetune << 4);

        if (sample.length > 2) {
            nSamples_++;
            sample.data = (SHORT *)(buf + fileOffset);
            instrument.samples[0] = new Sample;
            sample.dataType = SIGNED_EIGHT_BIT_SAMPLE;
            instrument.samples[0]->load(sample);
        }
        fileOffset += sample.length;
        instruments_[i] = new Instrument;
        instruments_[i]->load(instrument);

#ifdef debug_mod_loader
        cout << "\nSample " << i << ": name     = " << instruments_[i]->getName();
        if (!instruments_[i]->getSample(0)) _getch();

        if (instruments_[i]->getSample(0)) {
            HWAVEOUT        hWaveOut;
            WAVEFORMATEX    waveFormatEx;
            MMRESULT        result;
            WAVEHDR         waveHdr;

            cout << "\nSample " << i << ": length   = " << instruments_[i]->getSample(0)->getLength();
            cout << "\nSample " << i << ": rep ofs  = " << instruments_[i]->getSample(0)->getRepeatOffset();
            cout << "\nSample " << i << ": rep len  = " << instruments_[i]->getSample(0)->getRepeatLength();
            cout << "\nSample " << i << ": volume   = " << instruments_[i]->getSample(0)->getVolume();
            cout << "\nSample " << i << ": finetune = " << instruments_[i]->getSample(0)->getFinetune();

            // not very elegant but hey, is debug code lol
            if (!instruments_[i]->getSample(0)->getData()) break; 

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
                if (!i) cout << "\nError opening wave mapper!\n";
            } else {
                int retry = 0;
                if (!i) cout << "\nWave mapper successfully opened!\n";
                waveHdr.dwBufferLength = instruments_[i]->getSample(0)->getLength () * 
                                         waveFormatEx.nBlockAlign;
                waveHdr.lpData = (LPSTR)(instruments_[i]->getSample(0)->getData ());
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
/*
                while(waveOutUnprepareHeader(hWaveOut, &waveHdr, 
                                  sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) 
                {
                    Sleep(50);
                }                
*/
                waveOutReset(hWaveOut);
                waveOutClose(hWaveOut);
            }
		}
#endif
    }
    // read the patterns now
    bufp = buf + patternDataOffset;
    // convert 8 chn startrekker patterns to regular ones
    if (flt8Err) {
        unsigned    flt[8 * MOD_ROWS];   // 4 bytes / note * 8 channels * 64 rows
        unsigned    *ps, *pd, *p1, *p2;

        ps = (unsigned *)bufp; 
        pd = ps;
        for (unsigned ptn = 0; ptn < nPatterns_; ptn++) {
            for (int i = 0; i < 2 * 4 * MOD_ROWS; i++) {// copy data to tmp buf
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
    for (unsigned  i = 0; i < nPatterns_; i++) {
        Note        *iNote, *patternData;
        unsigned    j1, j2, j3, j4;

        patterns_[i] = new Pattern;

        patternData = new Note[nChannels_ * MOD_ROWS];
        patterns_[i]->Initialise(nChannels_, MOD_ROWS, patternData);
        iNote = patternData;
        for (unsigned n = 0; n < (nChannels_ * MOD_ROWS); n++) {
            j1 = *((unsigned char *)bufp++);
            j2 = *((unsigned char *)bufp++);
            j3 = *((unsigned char *)bufp++);
            j4 = *((unsigned char *)bufp++);

            iNote->effects[0].effect = j3 & 0xF;
            iNote->effects[0].argument = j4;
            iNote->period = j2;
            j4 = (j1 & 0xF0) + (j3 >> 4);
            iNote->instrument = j4 & 0x1F;
            iNote->period += ((j1 & 0xF) << 8) + ((j4 >> 5) << 12);
            iNote->note = 0;
            if (iNote->period) {
                unsigned j;
                for (j = 0; j < (MAXIMUM_NOTES); j++) {
                  if (iNote->period >= periods[j]) break;
                }
                if (j >= (MAXIMUM_NOTES)) iNote->note = 0;
                else                      iNote->note = j + 1;
            }

            for (unsigned fxloop = 1; fxloop < MAX_EFFECT_COLUMS; fxloop++) {
                iNote->effects[fxloop].effect   = 0; 
                iNote->effects[fxloop].argument = 0; // not used in mod files
            }
            // do some error checking & effect remapping:
            switch (iNote->effects[0].effect) {
/*
                case    0x0 :
                    {
                        if (iNote->effects[0].argument)
                            iNote->effects[0].effect = ARPEGGIO;
                        break;
                    }
*/
/*
                case    EXTENDED_EFFECTS : 
                    {
                        iNote->effects[0].effect = 0xE0 + 
                        (iNote->effects[0].argument >> 4);
                        iNote->effects[0].argument &= 0xF;
                        // correct MTM panning
                        if (iNote->effects[0].effect == 0xE8) {
                            iNote->effects[0].effect = 0x8;
                            iNote->effects[0].argument <<= 4;
                        }
                        break;
                    }
*/
                case    SET_VOLUME :
                    {
                        if (iNote->effects[0].argument > MAX_VOLUME)
                            iNote->effects[0].argument = MAX_VOLUME;
                        break;
                    }
                case    VOLUME_SLIDE :
                case    SET_TEMPO_BPM :
//                case    SET_SAMPLE_OFFSET :
                    {
                        if (!iNote->effects[0].argument) {
                            iNote->effects[0].effect = 0;
                        }
                        break;
                    }
            }
#ifdef debug_mod_loader
            if (i == 0) {
                char    hex[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

                if (!(n % nChannels_)) cout << "\n";
                else cout << "|";
                cout << noteStrings[iNote->note];
                cout << ":";
                if (iNote->instrument < 10) cout << "0";
                cout << iNote->instrument;

                if (iNote->effects[0].effect > 0xF) 
                        cout << (char)(iNote->effects[0].effect + 55);
                else    cout << hex[iNote->effects[0].effect];
                cout << hex[iNote->effects[0].argument >> 4];
                cout << hex[iNote->effects[0].argument & 0xF];
            }
#endif
            iNote++;
        }
    }
    delete buf;
    isLoaded_ = true;

#ifdef debug_mod_loader
    cout << "\n";
    cout << "\nFilename             = " << fileName_;
    cout << "\nis Loaded            = " << (isLoaded() ? "Yes" : "No");
    cout << "\nnChannels            = " << nChannels_;
    cout << "\nnInstruments         = " << nInstruments_;
    cout << "\nnSamples             = " << nSamples_;
    cout << "\nnPatterns            = " << nPatterns_;
    cout << "\nSong Title           = " << songTitle_;
    cout << "\nis CustomRepeat      = " << (isCustomRepeat() ? "Yes" : "No");
    cout << "\nSong Length          = " << songLength_;
    cout << "\nSong Restart Positn. = " << songRestartPosition_;
    cout << "\nNST File             = " << (nstFile ? "Yes" : "No");
    cout << "\n.WOW File            = " << (wowFile ? "Yes" : "No");
    cout << "\nFile Tag             = " << trackerTag_;
    cout << "\nTotal Samples Size   = " << sampleDataSize;
    cout << "\nptnHdr               = " << patternHeader;
    cout << "\nptnCalc              = " << patternCalc;
    cout << "\nRest from Divide     = " << patternDivideRest; // should be 0
#endif
    return 0;
}