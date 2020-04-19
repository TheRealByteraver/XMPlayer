#include "Module.h"
#include "Mixer.h"

#include "conio.h"
#include "../../../../../../../Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/ucrt/conio.h"

// ****************************************************************************
// ****************************************************************************
// ******* Start of Benchmarking code *****************************************
// ****************************************************************************
// ****************************************************************************

enum TimerToUseType { ttuUnknown,ttuHiRes,ttuClock };
TimerToUseType TimerToUse = ttuUnknown;
LARGE_INTEGER PerfFreq;     // ticks per second
int PerfFreqAdjust;         // in case Freq is too big
int OverheadTicks;          // overhead  in calling timer

void DunselFunction() { return; }

void DetermineTimer()
{
    void( *pFunc )() = DunselFunction;

    // Assume the worst
    TimerToUse = ttuClock;
    if ( QueryPerformanceFrequency( &PerfFreq ) )
    {
        // We can use hires timer, determine overhead
        TimerToUse = ttuHiRes;
        OverheadTicks = 200;
        for ( int i = 0; i < 20; i++ )
        {
            LARGE_INTEGER b,e;
            int Ticks;
            QueryPerformanceCounter( &b );
            (*pFunc)();
            QueryPerformanceCounter( &e );
            Ticks = e.LowPart - b.LowPart;
            if ( Ticks >= 0 && Ticks < OverheadTicks )
                OverheadTicks = Ticks;
        }
        // See if Freq fits in 32 bits; if not lose some precision
        PerfFreqAdjust = 0;
        int High32 = PerfFreq.HighPart;
        while ( High32 )
        {
            High32 >>= 1;
            PerfFreqAdjust++;
        }
    }
    return;
}

//double DoBench( void( *funcp )() )
double DoBench( Mixer &mixer )
{
    double time;      /* Elapsed time */

                      // Let any other stuff happen before we start
    MSG msg;
    PeekMessage( &msg,NULL,NULL,NULL,PM_NOREMOVE );
    Sleep( 0 );

    if ( TimerToUse == ttuUnknown )
        DetermineTimer();

    if ( TimerToUse == ttuHiRes )
    {
        LARGE_INTEGER tStart,tStop;
        LARGE_INTEGER Freq = PerfFreq;
        int Oht = OverheadTicks;
        int ReduceMag = 0;
        SetThreadPriority( GetCurrentThread(),
            THREAD_PRIORITY_TIME_CRITICAL );
        QueryPerformanceCounter( &tStart );

        //(*funcp)();   //call the actual function being timed

        /*
        mixer.doMixBuffer( mixer.waveBuffers[0] );
        for ( int bench = 0; bench < BENCHMARK_REPEAT_ACTION - 1; bench++ )
        {
            mixer.resetSong();  // does not reset everything?
            //std::cout << mixer.channels[0].
            mixer.doMixBuffer( mixer.waveBuffers[0] );
        }
        */

        QueryPerformanceCounter( &tStop );
        SetThreadPriority( GetCurrentThread(),THREAD_PRIORITY_NORMAL );
        // Results are 64 bits but we only do 32
        unsigned int High32 = tStop.HighPart - tStart.HighPart;
        while ( High32 )
        {
            High32 >>= 1;
            ReduceMag++;
        }
        if ( PerfFreqAdjust || ReduceMag )
        {
            if ( PerfFreqAdjust > ReduceMag )
                ReduceMag = PerfFreqAdjust;
            tStart.QuadPart = Int64ShrlMod32( tStart.QuadPart,ReduceMag );
            tStop.QuadPart = Int64ShrlMod32( tStop.QuadPart,ReduceMag );
            Freq.QuadPart = Int64ShrlMod32( Freq.QuadPart,ReduceMag );
            Oht >>= ReduceMag;
        }

        // Reduced numbers to 32 bits, now can do the math
        if ( Freq.LowPart == 0 )
            time = 0.0;
        else
            time = ((double)(tStop.LowPart - tStart.LowPart
                - Oht)) / Freq.LowPart;
    } else
    {
        long stime,etime;
        SetThreadPriority( GetCurrentThread(),
            THREAD_PRIORITY_TIME_CRITICAL );
        stime = clock();
        //(*funcp)();
        mixer.startReplay();
        etime = clock();
        SetThreadPriority( GetCurrentThread(),THREAD_PRIORITY_NORMAL );
        time = ((double)(etime - stime)) / CLOCKS_PER_SEC;
    }

    return (time);
}

void startReplay( Mixer &mixer ) {
    mixer.startReplay();
}

// ****************************************************************************
// ****************************************************************************
// ******* End of benchmarking code *******************************************
// ****************************************************************************
// ****************************************************************************

/*
1 pixel = 1 tick, ft2 envelope window width == 6 sec
vibrato is active even if envelope is not
vibrato sweep: amount of ticks before vibrato reaches max. amplitude
*/

int main( int argc, char *argv[] )  
{ 
    std::vector< std::string > filePaths;
    char        *modPaths[] = {
        "D:\\MODS\\M2W_BUGTEST\\blue_valclicktest.s3m",
        "D:\\MODS\\M2W_BUGTEST\\dope_clicktest2.mod",
        "D:\\MODS\\dosprog\\stardstm.mod",
        //"D:\\MODS\\M2W_BUGTEST\\ETANOLbiditest.xm",
        "D:\\MODS\\dosprog\\dope.mod",
        "C:\\Users\\Erland-i5\\Desktop\\mods\\Jazz3\\Bart\\05-rocket.it",
        "D:\\MODS\\S3M\\Karsten Koch\\blue_val.s3m",
        "D:\\MODS\\M2W_BUGTEST\\rs_stereo_sample.it",
        "D:\\MODS\\dosprog\\mods\\over2bgloop.xm",
        //"D:\\MODS\\dosprog\\chipmod\\mental.mod",
        "D:\\MODS\\dosprog\\chipmod\\crain.mod",
        "D:\\MODS\\dosprog\\chipmod\\toybox.mod",
        "D:\\MODS\\dosprog\\chipmod\\etanol.mod",
        "D:\\MODS\\dosprog\\chipmod\\sac09.mod",
        "D:\\MODS\\dosprog\\chipmod\\1.mod",
        "D:\\MODS\\dosprog\\chipmod\\bbobble.mod",
        "D:\\MODS\\dosprog\\chipmod\\asm94.mod",
        "D:\\MODS\\dosprog\\chipmod\\4ma.mod",
        //"D:\\MODS\\dosprog\\china1.mod",
        //"D:\\MODS\\mod_to_wav\\cd2part4.mod",
        "D:\\MODS\\M2W_BUGTEST\\Alertia_envtest.it",
        //"D:\\MODS\\M2W_BUGTEST\\Alertia_envtest.xm",
		"D:\\MODS\\M2W_BUGTEST\\AQU-INGO-16b_samp.S3M",
  //      "D:\\MODS\\dosprog\\mods\\probmod\\veena.wow",
        "D:\\MODS\\dosprog\\MYRIEH.XM",
        //"D:\\MODS\\M2W_BUGTEST\\cd2part2b.mod",
        //"D:\\MODS\\M2W_BUGTEST\\women2.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\menutune3.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\ssi2.S3m",
        //"D:\\MODS\\M2W_BUGTEST\\menutune3.xm",
        //"D:\\MODS\\M2W_BUGTEST\\pullmax-portatest.xm",
        //"D:\\MODS\\M2W_BUGTEST\\appeal.mod",
        //"D:\\MODS\\M2W_BUGTEST\\againstptnloop.MOD",
        //"D:\\MODS\\M2W_BUGTEST\\againstptnloop.xm",
        //"D:\\MODS\\MOD\\hoffman_and_daytripper_-_professional_tracker.mod",
        "D:\\MODS\\M2W_BUGTEST\\4wd.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\k_hippo.s3m",
        //"D:\\MODS\\S3M\\Karsten Koch\\aryx.s3m",
        //"D:\\MODS\\S3M\\Purple Motion\\inside.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\WORLD-vals.S3M",
        //"D:\\MODS\\M2W_BUGTEST\\WORLD-vals.xm",
        //"D:\\MODS\\M2W_BUGTEST\\2nd_pm-porta.s3m",

        //"D:\\MODS\\dosprog\\mods\\demotune.mod", // xm = wrong, ptn loop tester
        //"D:\\MODS\\dosprog\\ode2pro.MOD",
        //"D:\\MODS\\M2W_BUGTEST\\alf_-_no-mercy-SampleOffsetBug.mod",
        //"D:\\MODS\\M2W_BUGTEST\\against-retrigtest.s3m",
        //"D:\\MODS\\S3M\\Purple Motion\\zak.s3m",
        //"D:\\MODS\\dosprog\\audiopls\\YEO.MOD",
        //"D:\\MODS\\dosprog\\mods\\over2bg.xm",
        //"D:\\MODS\\M2W_BUGTEST\\resolution-loader-corrupts-sample-data.xm",
        //"D:\\MODS\\M2W_BUGTEST\\resolution-loader-corrupts-sample-data2.mod",
        //"D:\\MODS\\M2W_BUGTEST\\believe.mod",
        //"D:\\MODS\\M2W_BUGTEST\\believe-wrong notes.mod",
        //"D:\\MODS\\M2W_BUGTEST\\ParamMemory2.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\global trash 3 v2-songrepeat-error.mod",
        //"D:\\MODS\\M2W_BUGTEST\\CHINA1.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\Creagaia.it",   // impulse tracker unknown
        
        //"D:\\MODS\\M2W_BUGTEST\\qd-anoth-noload.xm",
        //"D:\\MODS\\M2W_BUGTEST\\Crystals.wow",
        "D:\\MODS\\M2W_BUGTEST\\china1.it",
        "D:\\MODS\\M2W_BUGTEST\\Creagaia-nocomp.it",
        "D:\\MODS\\M2W_BUGTEST\\menuralli.it",
        //"D:\\MODS\\dosprog\\mods\\starsmuz.xm",
        //"D:\\MODS\\dosprog\\mods\\pullmax.xm",
        "D:\\MODS\\M2W_BUGTEST\\womeni.it",
        "D:\\MODS\\dosprog\\backward.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\Creagaia-nocomp.it",
        //"D:\\MODS\\M2W_BUGTEST\\Crea2.it",      // impulse tracker v1.6
        //"D:\\MODS\\M2W_BUGTEST\\Crea.it",         // impulse tracker v2.0+
        //"D:\\MODS\\M2W_BUGTEST\\WOMEN.xm",
        //"D:\\MODS\\M2W_BUGTEST\\module1.mptm",
        "D:\\MODS\\M2W_BUGTEST\\finalreality-credits.it",
        "D:\\MODS\\M2W_BUGTEST\\BACKWARD.IT",

        //"D:\\MODS\\mod_to_wav\\CHINA1.MOD",
        //"D:\\MODS\\MOD\\Jogeir Liljedahl\\slow-motion.mod",
        //"D:\\MODS\\M2W_BUGTEST\\slow-motion-pos15-porta.mod",
        //"D:\\MODS\\dosprog\\MUSIC\\S3M\\2nd_pm.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\sundance-fantomnotes.mod",
        //"D:\\MODS\\M2W_BUGTEST\\vibtest.mod",
        //"D:\\MODS\\M2W_BUGTEST\\menutune4.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\ssi.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\menutune2.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\algrhyth2.mod",
        //"D:\\MODS\\M2W_BUGTEST\\algrhyth2.s3m",
        //"D:\\MODS\\dosprog\\audiopls\\ALGRHYTH.MOD",
        //"D:\\MODS\\dosprog\\mods\\probmod\\nowwhat3.mod",
        //"D:\\MODS\\dosprog\\mods\\probmod\\xenolog1.mod",
        //"D:\\MODS\\dosprog\\mods\\menutune.s3m",
        //"D:\\MODS\\M2W_BUGTEST\\ssi.s3m",
        //"D:\\MODS\\mod_to_wav\\XM JL\\BIZARE.XM",
        //"D:\\MODS\\S3M\\Karsten Koch\\aryx.s3m",
        //"D:\\MODS\\dosprog\\mods\\women.s3m",
        //"D:\\MODS\\dosprog\\audiopls\\ALGRHYTH.MOD",
        //"c:\\Users\\Erland-i5\\desktop\\morning.mod",
        //"D:\\MODS\\dosprog\\china1-okt.s3m",
        //"D:\\MODS\\dosprog\\2nd_pm.xm",
        //"D:\\MODS\\dosprog\\lchina.s3m",
        //"D:\\MODS\\dosprog\\mods\\againstr.s3m",
        //"D:\\MODS\\dosprog\\mods\\againstr.mod",
        //"D:\\MODS\\dosprog\\mods\\bluishbg2.xm",
        //"D:\\MODS\\dosprog\\mods\\un-land2.s3m",
        //"D:\\MODS\\dosprog\\mods\\un-land.s3m",
        //"D:\\MODS\\dosprog\\mods\\un-vectr.s3m",
        //"D:\\MODS\\dosprog\\mods\\un-worm.s3m",
        //"D:\\MODS\\dosprog\\chipmod\\mental.mod",
        //"D:\\MODS\\dosprog\\mods\\theend.mod",
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\jz-scpsm2.xm",
        //"D:\\MODS\\dosprog\\music\\xm\\united_7.xm",
        //"D:\\MODS\\dosprog\\ctstoast.xm",
        //"D:\\MODS\\dosprog\\mods\\probmod\\xenolog1.mod",
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\mech8.s3m",
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\jazz1\\Tubelectric.S3M",
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\jazz1\\bonus.S3M",        
        //"C:\\Users\\Erland-i5\\Desktop\\mods\\Silverball\\fantasy.s3m",
        //"D:\\MODS\\dosprog\\women.xm",
        
        //"D:\\MODS\\dosprog\\mods\\menutune.s3m",
        //"D:\\MODS\\dosprog\\mods\\track1.s3m",
        //"D:\\MODS\\dosprog\\mods\\track2.s3m",
        //"D:\\MODS\\dosprog\\mods\\track3.s3m",
        //"D:\\MODS\\dosprog\\mods\\track4.s3m",
        //"D:\\MODS\\dosprog\\mods\\track5.s3m",
        //"D:\\MODS\\dosprog\\mods\\track6.s3m",
        //"D:\\MODS\\dosprog\\mods\\track7.s3m",
        //"D:\\MODS\\dosprog\\mods\\track8.s3m",
        //"D:\\MODS\\dosprog\\mods\\track9.s3m",
        
        //"D:\\MODS\\dosprog\\mods\\ssi.s3m",
        //"D:\\MODS\\dosprog\\mods\\ssi.xm",
        //"D:\\MODS\\dosprog\\mods\\pori.s3m",
        //"D:\\MODS\\dosprog\\mods\\tearhate.s3m",
        //"D:\\MODS\\dosprog\\mods\\starsmuz.s3m",
        
        //"D:\\MODS\\MOD\\beastsong.mod",
        //"D:\\MODS\\dosprog\\mods\\over2bg.xm",
        //"D:\\MODS\\dosprog\\chipmod\\mental.mod",
        //"D:\\MODS\\dosprog\\mods\\probmod\\chipmod\\mental.xm",
        //"D:\\MODS\\dosprog\\mods\\probmod\\chipmod\\MENTALbidi.xm",
        "D:\\MODS\\dosprog\\mods\\baska.mod",
        //"d:\\Erland Backup\\C_SCHIJF\\erland\\bp7\\bin\\exe\\cd2part2.mod",
//        "D:\\MODS\\dosprog\\audiopls\\crmx-trm.mod",
        //"D:\\MODS\\dosprog\\ctstoast.xm",
        //"D:\\MODS\\dosprog\\dope.mod",
//        "D:\\MODS\\dosprog\\smokeoutstripped.xm",
        //"D:\\MODS\\dosprog\\smokeout.xm",
        //"D:\\MODS\\dosprog\\KNGDMSKY.XM",
        //"D:\\MODS\\dosprog\\KNGDMSKY-mpt.XM",
        //"D:\\MODS\\dosprog\\myrieh.xm",

        "D:\\MODS\\dosprog\\mods\\explorat.xm",
        "D:\\MODS\\dosprog\\mods\\devlpr94.xm",
        //"D:\\MODS\\dosprog\\mods\\bj-eyes.xm",
        "D:\\MODS\\dosprog\\mods\\1993.mod",
        "D:\\MODS\\dosprog\\mods\\1993.xm",
        //"D:\\MODS\\dosprog\\mods\\baska.mod",
        //"D:\\MODS\\dosprog\\mods\\bj-love.xm",
//        "D:\\MODS\\dosprog\\mods\\probmod\\3demon.mod",
        "D:\\MODS\\dosprog\\mods\\probmod\\flt8_1.mod",
        /* */
        nullptr
    };


    /*
        working formula:    
    
        int range = (127 - globalPanning) * 2 + 1;

        int attenuation = (256 - range) / 2;

        int panning = globalPanning;

        int finalPan = attenuation + (range * (panning + 1)) / 256;
    
    */

    /*
    std::cout
        << "\nPanning        |Global Panning | Final Panning0| Final Pan 255 | Final Pan"
        << "\n---------------+---------------+---------------+---------------|-----------|";
    for ( int globalPanning = 0; globalPanning <= MXR_PANNING_FULL_RIGHT; globalPanning++ ) {

        int range = (127 - globalPanning) * 2 + 1;

        int attenuation = (256 - range) / 2;

        int panning = globalPanning;

        int finalPan    = attenuation + (range * (panning + 1)) / 256;
        int finalPan0   = attenuation + (range * (0       + 1)) / 256;
        int finalPan255 = attenuation + (range * (255     + 1)) / 256;

        std::cout << "\n"
            << std::setw( 4 ) << panning << "           |"
            << std::setw( 4 ) << globalPanning << "           |"
            << std::setw( 4 ) << finalPan0 << "           |"
            << std::setw( 4 ) << finalPan255 << "           |"
            << std::setw( 4 ) << finalPan 
            ;
    }
    std::cout << "\n\nHit any key to continue...\n";
    _getch();
    */

    /*
    for ( float i = -2.0; i < 2.2; i += 0.1 )
        std::cout
        << "\ni = " << std::setw( 4 ) << i << "  -> " 
        << "(int)i = " << std::setw( 4 ) << (int)i
        << "  but floor( i ) = " << std::floor( i );
    std::cout << "\n";
    getch();
    */


    /*
    const float freqInc = 0.70000001f;
    const float smpOfs = 0.2999999f;
    const int nrLoops = 1000;
    const float freqInc4 = freqInc * 4.0f;
    float test1 = smpOfs;
    float test2 = smpOfs;


    std::cout
        << "\nfreqInc = " << freqInc
        << "\nsmpOfs  = " << smpOfs
        << "\nnrLoops = " << nrLoops
        << "\nfreqInc * 4 = " << freqInc4
        ;

    for ( int i = 0; i < nrLoops; i++ ) {
        test1 += freqInc;
        if ( (i % 4) == 0 )
            test2 += freqInc4;
    }

    std::cout
        << "\n\n" << smpOfs << " + freqInc (" << nrLoops << "x) = " << test1
        << "\n" << smpOfs << " + freqInc * 4 (" << (nrLoops >> 2) << "x) = " << test2
        << "\n" << smpOfs << " + freqInc * " << nrLoops << " = " << (smpOfs + freqInc * nrLoops)
        ;
    _getch();
    */









    if (argc > 1) {
        for ( int i = 1; i < argc; i++ ) 
            filePaths.push_back( argv[i] );
    } 
    else {
        /*
        if ( (!strcmp(argv[0], 
            "C:\\Users\\Erland-i5\\Documents\\Visual Studio 2019\\Projects\\Mod_to_WAV\\Debug\\Mod_to_WAV.exe")) ||
            (!strcmp(argv[0], 
            "C:\\Users\\Erland-i5\\Documents\\Visual Studio 2019\\Projects\\Mod_to_WAV\\Release\\Mod_to_WAV.exe")) ||
            (!strcmp(argv[0], 
            "C:\\Dev-Cpp\\Projects\\Mod2Wav.exe"))) 
        */
        if( true ) {
            for ( int i = 0; modPaths[i] !=nullptr; i++ ) 
                filePaths.push_back( modPaths[i] );
        } 
        else {
            unsigned    slen = (unsigned)strlen( argv[0] );
            char        *exeName = ( argv[0] + slen - 1 );

            while ( slen && (*exeName != '\\') ) { 
                slen--; 
                exeName--; 
            }
            exeName++;
            // dirty hack for the intro:
            if ( strcmp( exeName,"modplay.exe" ) != 0 ) {
                std::cout << "\n\nUsage: ";
                std::cout << exeName << " + <modfile.mod>\n\n";
                _getch();
                return 0;
            }
        }
    }

	Mixer       mixer;
    for (unsigned i = 0; i < filePaths.size(); i++) {

        Module      moduleFile;
        moduleFile.enableDebugMode();
        moduleFile.loadFile( filePaths[i] );

        std::cout << "\n\nLoading " << filePaths[i] // moduleFilename
                  << ": " << (moduleFile.isLoaded() ? "Success." : "Error!\n");

        if ( moduleFile.isLoaded () ) {
            /*
            unsigned s = BLOCK_SIZE / (MIXRATE * 2); // * 2 for stereo
            std::cout << "\nCompiling module \"" 
                      << moduleFile.getSongTitle()  
                      << "\" into " << (s / 60) << "m "
                      << (s % 60) << "s of 16 bit WAVE data\n" 
                      << "Hit any key to start mixing.\n";
            */      
            // show instruments
            for ( unsigned i = 1; i <= moduleFile.getnInstruments(); i++ ) {
                std::cout 
                    << "\n" << i << ":" 
                    << moduleFile.getInstrument( i ).getName();
            }
            std::cout << "\n\n";
            mixer.assignModule( &moduleFile );

            /*
            unsigned frameNr = 0;
            for ( int i = 0; i < 220; i++ ) {

                bool keyReleased = (i < 90) ? false : true;

                std::cout
                    << std::setw( 4 ) << i << ","
                    << std::setw( 3 ) << frameNr;
                    moduleFile.getInstrument( 10 ).getVolumeEnvelope().getEnvelopeVal( frameNr,keyReleased );
                    ;
                    //moduleFile.getInstrument( 10 ).getVolumeEnvelope().getInterpolatedVal( frameNr );
                frameNr++;
            }
            std::cout << "\nHit a key to continue...\n";
            _getch();
            */



            mixer.startReplay();

            while ( !_kbhit() ) {
                mixer.updateWaveBuffers();
                Sleep( 10 ); // give time slices back to windows
            }
            _getch();
            mixer.stopReplay();

            /*
            std::cout 
                << "\nA " << moduleFile.getnChannels() 
                << " channel module was rendered to " 
                << BUFFER_LENGTH_IN_MINUTES << " min of wave data in " 
                << benchTime << " seconds."
                << "\nEstimated realtime cpu charge is " 
                << (benchTime * 100) / (BUFFER_LENGTH_IN_MINUTES * 60) 
                << " percent.\nOn average " 
                << (benchTime * 1000.0 / moduleFile.getnChannels()) 
                    / BUFFER_LENGTH_IN_MINUTES 
                << " milliseconds per channel per minute."
                << "\n\nPlaying... Hit any key to stop.\n";
            */

            // EXECUTE our little demo here :)
            // only if run without arguments though.
            //if( argc == 1 )
            //    system( ".\\nosound.exe" );
            //else 
            //    _getch();  
            //mixer.stopReplay();
            //if ( argc == 1 ) // exit if the purpose was to run the intro
            //    return 0;
        }
        std::cout << "\nHit any key to load next / exit...";
        _getch();  
    }
/*
    std::cout << "\nHit any key to start memory leak test.";
    _getch();
    for (int i = 0; i < 40; i++) { 
        Module moduleFile;
        moduleFile.setFileName(modPaths[3]);
        moduleFile.loadFile();
        std::cout << "\nisLoaded = " << ((moduleFile.isLoaded ()) ? "Yes" : "No");
        std::cout << ", for the " << (i + 1) << "th time";
    }
    */
    std::cout << "\nHit any key to exit program.";
    _getch();
	return 0;
}

   
