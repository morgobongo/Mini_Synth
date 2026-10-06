#pragma once
#include <Arduino.h>

enum PolyGlideMode {
  GLIDE_ALL,
  GLIDE_LOWEST,
  GLIDE_HIGHEST
};

struct Preset {
  int numVoices;           // 1 for Monophonic, 4 for Polyphonic
  const int8_t* waveform;
  int glideTimeMs;
  int envAttack;
  int envDecay;
  int envSustainLevel;
  int envRelease;
  int vibratoDepth;
  float vibratoSpeed;
  int tremoloDepth;
  float tremoloSpeed;
  PolyGlideMode polyGlideMode;
  int masterVolume;             // Volume scaling (0-255)
};

// ---------------------------------------------------------
// 0-100% ABSTRACTION LAYER
// ---------------------------------------------------------
constexpr int mapPct(int pct, int minVal, int maxVal) {
    return minVal + (pct * (maxVal - minVal)) / 100;
}

constexpr float mapPctFloat(int pct, float minVal, float maxVal) {
    return minVal + (pct * (maxVal - minVal)) / 100.0f;
}

constexpr Preset makePreset(
    int numVoices, const int8_t* waveform, 
    int glidePct, int attackPct, int decayPct, int sustainPct, int releasePct,
    int vibDepthPct, int vibSpeedPct, int tremDepthPct, int tremSpeedPct,
    PolyGlideMode polyGlideMode, int masterVolumePct) 
{
    return {
        numVoices,
        waveform,
        mapPct(glidePct, 0, 2000),             // 0-2000 ms
        mapPct(attackPct, 2, 2000),            // 2-2000 ms
        mapPct(decayPct, 2, 2000),             // 2-2000 ms
        mapPct(sustainPct, 0, 255),            // 0-255 internal
        mapPct(releasePct, 2, 5000),           // 2-5000 ms
        mapPct(vibDepthPct, 0, 50),            // 0-50 internal
        mapPctFloat(vibSpeedPct, 0.1f, 20.0f), // 0.1 - 20.0 Hz
        mapPct(tremDepthPct, 0, 255),          // 0-255 internal
        mapPctFloat(tremSpeedPct, 0.1f, 20.0f),// 0.1 - 20.0 Hz
        polyGlideMode,
        mapPct(masterVolumePct, 0, 255)        // 0-255 internal
    };
}

// Preset declarations
extern Preset SOUND_1;
extern Preset SOUND_2;
extern Preset SOUND_3;
extern Preset SOUND_4;

extern Preset* soundBank[];
extern const int NUM_PRESETS;
