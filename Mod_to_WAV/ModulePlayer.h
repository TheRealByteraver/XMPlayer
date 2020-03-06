#pragma once

#include "mixer.h"
#include "Module.h"
#include "Sample.h"
#include "Pattern.h"

class Channel {
public:
    Instrument*     pInstrument;
    Sample*         pSample;
    Note            oldNote;
    Note            newNote; // oldNote + NewNote = 12 bytes

    bool            isMuted;
    unsigned char   volume;
    unsigned char   panning;
    bool            keyIsReleased;

    unsigned short  iVolumeEnvelope;
    unsigned short  iPanningEnvelope;

    unsigned short  iPitchFltrEnvelope;
    unsigned short  period;

    unsigned short  targetPeriod;
    unsigned char   instrumentNr;
    unsigned char   lastNote;

    unsigned        sampleOffset;

    // effect memory & index counters:
    bool            patternIsLooping;
    unsigned char   patternLoopStart;
    unsigned char   patternLoopCounter;
    unsigned char   lastArpeggio;

    unsigned char   arpeggioCount;
    unsigned char   arpeggioNote1;
    unsigned char   arpeggioNote2;
    unsigned char   vibratoWaveForm;

    unsigned char   tremoloWaveForm;
    unsigned char   panbrelloWaveForm;
    signed char     vibratoCount;
    signed char     tremoloCount;

    signed char     panbrelloCount;
    unsigned char   retrigCount;
    unsigned char   delayCount;
    unsigned char   lastPortamentoUp;

    unsigned short  portaDestPeriod;
    unsigned char   lastPortamentoDown;
    unsigned char   lastTonePortamento;

    unsigned char   lastVibrato;
    unsigned char   lastTremolo;
    unsigned char   lastVolumeSlide;
    unsigned char   lastFinePortamentoUp;

    unsigned char   lastFinePortamentoDown;
    unsigned char   lastFineVolumeSlideUp;
    unsigned char   lastFineVolumeSlideDown;
    unsigned char   lastGlobalVolumeSlide;

    unsigned char   lastPanningSlide;
    unsigned char   lastBpmSLide;
    unsigned char   lastSampleOffset;
    unsigned char   lastMultiNoteRetrig;

    unsigned char   lastExtendedEffect;
    unsigned char   lastTremor;
    unsigned char   lastExtraFinePortamentoUp;
    unsigned char   lastExtraFinePortamentoDown;

    unsigned short  mixerChannelNr;
    unsigned short  alignDummy;
public:
    Channel()
    {
        init();
    }
    void            init()
    {
        memset( this,0,sizeof( Channel ) );
    }
};



class ModulePlayer {

public:
    ModulePlayer( Module* module ) :
        module_( module )
    {
        
    }

    void            updateNotes();
    void            updateImmediateEffects();
    void            updateEffects();

    unsigned        noteToPeriod( unsigned note,int finetune );
    unsigned        periodToFrequency( unsigned period );


    void            updateBpm();
    void            setBpm( int bpm )
    {
        mixer_.setTempo( bpm );
    }

private:
    /*
        
    */
    unsigned        nrChannels_;

    /*
        global replay parameters, can probably be removed as we keep track
        of these in the mixer:
    */
    unsigned        globalPanning_;
    unsigned        globalVolume_;
    int             globalBalance_;     // range: -100...0...+100

    /*
        flags for the effect engine:    
    */
    bool            st300FastVolSlides_;
    bool            st3StyleEffectMemory_;
    bool            itStyleEffects_;
    bool            ft2StyleEffects_;
    bool            pt35StyleEffects_;

    /*
        Effect handling related variables:
    */
    bool            patternLoopFlag_;
    int             patternLoopStartRow_;

    /*
        Speed / tempo related variables:    
    */
    unsigned        tickNr_;
    unsigned        ticksPerRow_;  // nr of ticks per BPM
    unsigned        patternDelay_;
    unsigned        bpm_;

    /*
        Keep track of were we are in the song:
    */
    Pattern*        pattern_;
    const Note*     iNote_;
    unsigned        patternTableIdx_;
    unsigned        patternRow_;


    Channel         channels_[PLAYER_MAX_CHANNELS];


    Module*         module_;
    Mixer           mixer_;
};


