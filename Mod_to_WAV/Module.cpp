#include <cstdio>
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm.lib") 
#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <sys/stat.h>

#include "Module.h"

using namespace std;

Module::Module(const char *fileName) {
//    cout << "\nModule constructor with parameter fileName called.";
    //Module::Module ();  // this calls the default DEstructor!!!
    memset(this, 0, sizeof(Module));
    setFileName(fileName);
    loadFile ();
}

Module::~Module() {
//    cout << "\nDefault Module destructor called.";
    for (int i = 0; i < MAX_INSTRUMENTS; i++) delete instruments_[i];
    for (int i = 0; i < MAX_PATTERNS; i++) delete patterns_[i];
    delete fileName_;
    delete trackerTag_;
    delete songTitle_;
    return;
}

/*
char *Module::getFileName (const char *fileName) {
    int         max = strlen(fileName);
    char        *s = fileName_;
    const char  *d = fileName;
    int         i = 0;

    if (!fileName_) return "";
    while (*s && (i < max)) { *d++ = *s++; i++; }
    *d = '\0';
    return fileName;
}
*/
void Module::setFileName (const char *fileName) {
    fileName_ = new char[strlen(fileName) + 1];
    char        *d = fileName_;
    const char  *s = fileName;
    while (*s) *d++ = *s++; 
    *d = '\0';
}

int Module::loadFile() {
    int result;
    result = loadModFile();
    if (!isLoaded()) result = loadXmFile();
    return result;
}
