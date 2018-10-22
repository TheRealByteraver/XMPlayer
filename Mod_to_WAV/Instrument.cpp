#include <iostream>
#include <cstring>
#include "Module.h"

Instrument::~Instrument () {
//    std::cout << "\nInst destructor called";
    delete name_;
    for (int i = 0; i < MAX_SAMPLES; i++) delete samples_[i];
}

char *Instrument::getName (char *name) {
    int         max = (unsigned)strlen(name);
    char        *s = name_;
    char        *d = name;
    int         i = 0;

    while (*d && (i < max)) { *d++ = *s++; i++; }
    *d = '\0';
    return name;
}

void Instrument::load(const InstrumentHeader &instrumentHeader) {
    int     i = 0;
    char    *s, *d;

    name_ = new char[strlen(instrumentHeader.name) + 1];
    d = name_;
    s = instrumentHeader.name;
    while (*s && (i < MAX_INSTRUMENTNAME_LENGTH)) {
        *d++ = *s++; i++;
    }
    *d = '\0';
    for (int i = 0; i < MAXIMUM_NOTES; i++) {
        sampleForNote_[i] = instrumentHeader.sampleForNote[i];
    }
    for (int i = 0; i < 12; i++) {
        volumeEnvelope_ [i].x = instrumentHeader.volumeEnvelope [i].x;
        panningEnvelope_[i].x = instrumentHeader.panningEnvelope[i].x;
        volumeEnvelope_ [i].y = instrumentHeader.volumeEnvelope [i].y;
        panningEnvelope_[i].y = instrumentHeader.panningEnvelope[i].y;
    }
    for (int i = 0; i < MAX_SAMPLES; i++) {       // NEED COPY CONSTRUCTOR?
        samples_[i] = instrumentHeader.samples[i];// DANGER PTR 2 OBJECT
    }
    nSamples_           = instrumentHeader.nSamples;
    nVolumePoints_      = instrumentHeader.nVolumePoints;
    volumeSustain_      = instrumentHeader.volumeSustain;
    volumeLoopStart_    = instrumentHeader.volumeLoopStart;
    volumeLoopEnd_      = instrumentHeader.volumeLoopEnd;
    volumeType_         = instrumentHeader.volumeType;
    volumeFadeOut_      = instrumentHeader.volumeFadeOut;
    nPanningPoints_     = instrumentHeader.nPanningPoints;
    panningSustain_     = instrumentHeader.panningSustain;
    panningLoopStart_   = instrumentHeader.panningLoopStart;
    panningLoopEnd_     = instrumentHeader.panningLoopEnd;
    panningType_        = instrumentHeader.panningType;
    vibratoType_        = instrumentHeader.vibratoType;
    vibratoSweep_       = instrumentHeader.vibratoSweep;
    vibratoDepth_       = instrumentHeader.vibratoDepth;
    vibratoRate_        = instrumentHeader.vibratoRate;                      
}
