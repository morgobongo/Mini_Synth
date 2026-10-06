#pragma once
#include <Arduino.h>
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_AUDIO_CHANNELS 1
#include <MozziGuts.h>
#include <Oscil.h>
#include <ADSR.h>
#include <Line.h>
#include <tables/sin2048_int8.h>
#include "Presets.h"
#include "KeyBuffer.h"
#include "Notes.h"

#define NUM_VOICES 4 // Max 4 voices for this engine

struct Voice {
  Oscil<2048, AUDIO_RATE> vco;
  ADSR<CONTROL_RATE, AUDIO_RATE> envelope;
  Line<float> glide;
  volatile float currentFreq;
  volatile float targetFreq;
  char currentKey;
  bool isActive;
  int legatoFrames;
};

class SynthEngine {
public:
    SynthEngine(Notes& notesManager);
    
    void init();
    void loadPreset(Preset* p);
    void updateControl(KeyBuffer& buffer, int octave);
    int updateAudio();

private:
    Voice voices[NUM_VOICES];
    Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> vibratoLfo;
    Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> tremoloLfo;
    int currentVibrato;
    int currentTremolo;
    Preset* activePreset;
    Notes& notes;
    volatile int activeVoiceCount;
};

extern SynthEngine synth;
