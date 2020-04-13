/*
        Thanks for this project fly out to (in no particular order):

        - Jeffrey Lim (Pulse) for creating the awesome Impulse Tracker: the .IT
        format is seriously next level compared to what preceded it! Too bad it was
        a real mode app. Should've been a Windows app from the start...
        - Johannes - Jojo - Schultz (Saga Musix) for helping me out with all kinds of
        questions related to the .IT format and module replay in general
        - Tammo Hinrichs for the .IT sample decompression routines itsex.c which I
        used in my .IT loader. Thanks for explaining how the algorithm works! Source:
        https://github.com/nicolasgramlich/AndEngineMODPlayerExtension/blob/master/jni/loaders/itsex.c

        - PSI (Sami Tammilehto) / Future Crew for creating Scream Tracker 3
        - FireLight / Brett Paterson for writing a detailed document explaining how to
        parse .s3m files. Without fs3mdoc.txt this would have been hell. With his
        invaluable document, it was a hell of a lot easier

        - David Overton for his great tutorial on the windows Wave Mapper 
        functions
        http://www.planet-source-code.com/vb/scripts/ShowCode.asp?txtCodeId=4422&lngWId=3

        - Fredrik Huss aka "Mr H." from the demo group Triton (now Starbreeze) 
        for helping me out with my XM player back in the '90s, and for creating 
        FastTracker II in the first place of course!!!
        http://www.starbreeze.com

        - Kurt Kennett for his thorough description of the .MOD file format
        http://www.wotsit.org

        - ADVANCED GRAVIS for making the Gravis UltraSound. My beloved GUS 
        introduced me to the fascinating PC demo scene (www.scene.org), and
        because it's so easy to program it encouraged me to write my first
        .MOD player (source available: google for "tnt-mp11.zip" or get a more
        complete (protected mode) version from github:
        https://github.com/TheRealByteraver/tnt-mp12.zip

        - Olivier Lapicque for creating ModPlug Tracker & ModPlug player
        - The whole OpenMPT team, Saga Musix especially!
        https://openmpt.org

        - Chili for re-igniting my interest in C++, Albinopapa for pulling me through :)
        https://planetchili.net

        - The Cherno Project: same as above. You are a hardworking young man with 
        a brain stuck in overdrive :) Super nice C++ videos, check them out:
        https://www.youtube.com/user/TheChernoProject

        - My Music Heroes:

            Karsten Koch
            - Love your music, as I said elsewhere: Jean Michel Jarre 
            would be proud!

            Henrik Sundberg aka fajser / RAGE PC 
            - you make some cruel tunes dude, love 'em!!!!!

            Jonne Valtonen aka Purple Motion / FC
            - the most melodic melodies ever ;) I can listen to it for hours 
            and hours and...
        
            Peter Hajba aka Skaven / FC
            - some very, very nice tunes... I hope you're still at it!

            Moby / dreamdealers / ..
            - too bad that other moby from NY "stole" your nick now it's more
            difficult to find your music ;)

            GROO / CNCD
            - The music for Stars / NOOON is simply fantastic!

            Wiliam Petiot aka Ghost Fellow / Magical Wonder Band
            - Your s3m's are not always 100% balanced but I really love 'em
            still AND you're such a gifted programmer!!!
            
            Jugi / Complex Media Labs
            - dope.mod is fantastic for testing, great tune as well! Your
            other tunes are super dope as well :)

            Magnus Högdahl aka Vogue / Triton / Starbreeze
            - you have too many talents, simple as that :)
            
            And also: Gustaf Grefberg aka Lizardking, Jogeir 
            Liljedahl, Sidewinder / Megawatts, otis / inforcorner, Robin van 
            Nooy aka Cygnes / T-Matic: women.s3m is 
            fantastic :), Necros & Basehead / fm - that Unreal music is truly 
            unreal!, same for the Crusader soundtrack!       
            Linus Elman aka probe / TBL - LOOOOOVE the JIZZ & STASH 
            soundtracks!!!!!, Dune / Orange, Leinad / TYO, Kenny Chou aka 
            C.C.Catch - beautiful melodies, same goes for Thomas Pytel 
            aka Tran - and some programmer you are, too! , Chris Korte, 
            Maso / Accession, Jari Karppinen, Scorpik / abs! , Mordicus / 
            Jamm, Azazel / TBL - cruel tunes just like we like 'em :)
            mad freak / anarchy, Deathjester, Saint Shoe, Brainbug / 
            alcatraz, twins / phenomena, Laxity / Kefrens, Nuke / Anarchy,
            Zodiak / Cascada, Jesper Kyd, Volker Tripp aka Jester ...  
            and so many others!!!

        - My friends and teachers:
            - Bart Goossens / Ghent university for his expert help with SSE
            instructions, mathematics and programming in general: you are a 
            one man army!
            - Wodan, my programming buddy, bugging me to leave Pascal and to
            start with C, then to stop me on C++ ;) (failed on that one)
            http://www.caiman.us
            - Bluejazz aka Affective Disorder - for the music, the drawings,
            the friendship! May you rest in peace.
            - Sombritude - for the friendship, shared hardships & the XM's of 
            course!!!
            - Mister "Kalevra" Gustavo, for trying (and so far failing mostly) 
            to convert me into a web developer :D

        - ... and last but definitely not least: my long time girlfriend Maryna
        for letting me spend as much hours behind my computer as I care for and 
        for being generally supportive. Thank you my love <3 
*/
