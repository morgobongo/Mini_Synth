# Oskitone Scout ESP32 Port - Documentation

This document describes the hardware wiring and software architecture of the ESP32 port for the Oskitone Scout synthesizer, featuring 4-voice polyphony using the Mozzi audio library.

## 1. Keyboard Matrix Wiring

The keyboard consists of 17 tactile switches wired in a 4x5 matrix (4 rows, 5 columns). It uses the ESP32's internal pull-up resistors for the columns.

### Pin Configuration
- **Audio Output (I2S Internal DAC)**: GPIO 25 (Connected to LM386 amplifier)
- **Push Buttons (Inputs with Pull-up)**:
  - Preset (Mode): GPIO 33
  - Octave: GPIO 15
- **Rows (Outputs)**: 
  - Row 0: GPIO 4
  - Row 1: GPIO 16
  - Row 2: GPIO 17
  - Row 3: GPIO 18
- **Columns (Inputs with Pull-up)**:
  - Column 0: GPIO 13
  - Column 1: GPIO 14
  - Column 2: GPIO 12
  - Column 3: GPIO 27
  - Column 4: GPIO 32

### Matrix Map (Key Index / Note Number)
The 17 keys are mapped to the matrix intersections as follows. The numbers represent the key index (1 to 17), where 1 is the lowest note and 17 is the highest note on the keyboard.

|        | Col 0 (Pin 13) | Col 1 (Pin 14) | Col 2 (Pin 12) | Col 3 (Pin 27) | Col 4 (Pin 32) |
|--------|----------------|----------------|----------------|----------------|----------------|
| **Row 0 (Pin 4)**  | Key 1          | Key 5          | Key 9          | Key 12         | Key 15         |
| **Row 1 (Pin 16)** | Key 2          | Key 6          | Key 10         | Key 13         | Key 16         |
| **Row 2 (Pin 17)** | Key 3          | Key 7          | Key 11         | Key 14         | Key 17         |
| **Row 3 (Pin 18)** | Key 4          | Key 8          | -              | -              | -              |

*Note: The audio output uses the internal DAC via I2S, which automatically takes control of both DAC1 (GPIO 25) and DAC2 (GPIO 26). Therefore, GPIO 26 cannot be used for the keyboard matrix.*

---

## 2. Source Code Architecture

The firmware is built using PlatformIO with the Arduino framework. It uses `Keypad.h` for debouncing and matrix scanning, `CircularBuffer.hpp` for polyphonic key state management, and `Mozzi` for audio synthesis.

### `src/main.cpp`
The core application file. It acts as the bridge between the hardware inputs and the audio engine.
- Reads hardware buttons (Preset and Octave) and handles debouncing.
- `updateControl()`: Runs at 64Hz. Polls the `KeyBuffer` for pressed keys, updates the LED, and delegates voice allocation to the `SynthEngine`.
- `updateAudio()`: Runs at the audio rate. Delegates audio generation to the `SynthEngine`.
- `loop()`: Continuously calls `audioHook()` (required by Mozzi) and `buffer.populate()` (required to poll the physical keyboard as fast as possible for accurate debouncing).

### `src/SynthEngine.h` & `src/SynthEngine.cpp`
Encapsulates all Mozzi audio generation and voice management.
- Initializes the Mozzi audio engine.
- Manages 4 independent Mozzi voices (Oscillators, ADSR envelopes, Glide) to achieve polyphony.
- Handles LFO modulations (Vibrato and Tremolo).
- Maps active keys from the `KeyBuffer` to available voices and mixes them down for the DAC.

### `src/Presets.h` & `src/Presets.cpp`
Defines the sound patches available on the synth.
- Stores preset configurations including waveforms, envelope settings (Attack, Decay, Sustain, Release), and LFO rates/depths.
- Contains the `soundBank` array which holds all built-in presets.

### `src/KeyBuffer.h` & `src/KeyBuffer.cpp`
Handles all hardware interactions related to the 17-key keyboard.
- Sets up the `Keypad` library with the defined row and column pins.
- Implements a custom 10ms debounce time to prevent hardware glitches.
- Uses a `CircularBuffer` to keep track of exactly which keys are currently held down.
- `populate()`: Scans the matrix and updates the internal buffer with newly pressed keys, or removes keys that have been released.
- `getSize()` and `getAt()`: Exposes the list of currently held keys to `main.cpp` so they can be mapped to the audio voices.

### `src/Notes.h` & `src/Notes.cpp`
A mathematical utility class for calculating musical frequencies.
- Uses the standard formula for equal temperament tuning: `Frequency = 440 * 2^(distance_from_A4 / 12)`.
- It takes a starting offset (in our case, `-9` semitones from Middle A) and dynamically calculates the exact Hertz value for any given key index.
- This allows for easy transposition or octave shifting in the future.

---

## 3. Presets Reference

The synth includes a built-in Preset system defined in `src/Presets.cpp`. Here is a quick reference for the currently configured sound patches:

### Parameter Guide (0-100%)
- **`numVoices`**: 1 (Monophonic) or 4 (Polyphonic)
- **`waveform`**: `square_wave`, `saw_wave`, or `sine_wave`
- **`glidePct`**: Portamento glide time (0% = 0ms, 100% = 2000ms).
- **`attackPct` / `decayPct`**: Envelope times (0% = 2ms, 100% = 2000ms).
- **`sustainPct`**: Sustain volume level (0-100%).
- **`releasePct`**: Release tail time (0% = 2ms, 100% = 5000ms).
- **`vibratoDepthPct`**: LFO pitch modulation depth (0-100%).
- **`vibratoSpeedPct`**: LFO speed (0% = 0.1Hz, 100% = 20Hz).
- **`tremoloDepthPct`**: LFO volume modulation depth (0-100%).
- **`tremoloSpeedPct`**: LFO speed (0% = 0.1Hz, 100% = 20Hz).
- **`polyGlideMode`**: `GLIDE_ALL`, `GLIDE_LOWEST`, or `GLIDE_HIGHEST`.
- **`masterVolumePct`**: Output volume scalar (0-100%) to prevent clipping.

### Preset 1: Original Oskitone
```cpp
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
```

### Preset 2: Modular Canvas
```cpp
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
  31                            // masterVolumePct (~31% pour éviter saturation)
);
```

### Preset 3: Lead Synth
```cpp
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
```

### Preset 4: Oskitone Polyphonique
```cpp
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
  31                            // masterVolumePct (~31% pour 4 voix)
);
```
