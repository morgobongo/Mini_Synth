#include "Presets.h"
#include <tables/square_no_alias_2048_int8.h>
#include <tables/saw2048_int8.h>
#include <tables/sin2048_int8.h>

// ---------------------------------------------------------
// WAVEFORM ALIASES
// ---------------------------------------------------------
const int8_t* square_wave = SQUARE_NO_ALIAS_2048_DATA;
const int8_t* saw_wave = SAW2048_DATA;
const int8_t* sine_wave = SIN2048_DATA;

// Preset 1: Original Oskitone (Monophonic, Glide, Gate Envelope)
Preset SOUND_1 = makePreset(
  1,                            // numVoices (Monophonic)
  square_wave,                  // waveform
  3,                            // glidePct (3% = ~60ms)
  0,                            // attackPct (0% = 2ms)
  0,                            // decayPct (0% = 2ms)
  100,                          // sustainPct (100% = full volume)
  0,                            // releasePct (0% = 2ms)
  0,                            // vibratoDepthPct
  30,                           // vibratoSpeedPct
  0,                            // tremoloDepthPct
  20,                           // tremoloSpeedPct
  GLIDE_ALL,                    // polyGlideMode
  50                            // masterVolumePct (~50%)
);

// Preset 2: Modular Canvas (Polyphonic, ADSR, LFOs, Sawtooth)
// -------------------------------------------------------------
// Parameter Guide: All values are 0 to 100%
// -------------------------------------------------------------
Preset SOUND_2 = makePreset(
  4,                            // numVoices (Polyphonic)
  square_wave,                  // waveform
  0,                            // glidePct
  2,                            // attackPct (2% = ~40ms)
  2,                            // decayPct (2% = ~40ms)
  100,                          // sustainPct (100% = full volume)
  1,                            // releasePct (1% = ~50ms)
  50,                           // vibratoDepthPct
  30,                           // vibratoSpeedPct
  0,                            // tremoloDepthPct
  20,                           // tremoloSpeedPct
  GLIDE_LOWEST,                 // polyGlideMode
  31                            // masterVolumePct (~31% to prevent clipping)
);

// Preset 3: Lead Synth (Mono, Sawtooth, Long Glide)
Preset SOUND_3 = makePreset(
  1,                            // numVoices (Monophonic)
  saw_wave,                     // waveform
  8,                            // glidePct (8% = ~160ms)
  5,                            // attackPct (5% = ~100ms)
  2,                            // decayPct (2% = ~40ms)
  78,                           // sustainPct (78% = ~200)
  10,                           // releasePct (10% = ~500ms)
  20,                           // vibratoDepthPct
  20,                           // vibratoSpeedPct
  0,                            // tremoloDepthPct
  0,                            // tremoloSpeedPct
  GLIDE_ALL,                    // polyGlideMode
  63                            // masterVolumePct (~63%)
);

// Preset 4: Oskitone Polyphonic (Square, Polyphonic, Glide)
Preset SOUND_4 = makePreset(
  4,                            // numVoices (Polyphonic)
  square_wave,                  // waveform
  3,                            // glidePct (3% = ~60ms)
  0,                            // attackPct (0% = 2ms)
  0,                            // decayPct (0% = 2ms)
  100,                          // sustainPct (100% = full volume)
  0,                            // releasePct (0% = 2ms)
  0,                            // vibratoDepthPct
  30,                           // vibratoSpeedPct
  0,                            // tremoloDepthPct
  20,                           // tremoloSpeedPct
  GLIDE_ALL,                    // polyGlideMode
  31                            // masterVolumePct (~31% for 4 voices)
);

Preset* soundBank[] = { &SOUND_1, &SOUND_2, &SOUND_3, &SOUND_4 };
const int NUM_PRESETS = 4;
