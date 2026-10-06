#include <Arduino.h>
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_AUDIO_CHANNELS 1
#include <MozziGuts.h>
#include <Oscil.h>
#include <ADSR.h>
#include <Line.h>
#include <tables/square_no_alias_2048_int8.h>
#include <tables/saw2048_int8.h>
#include <tables/sin2048_int8.h>
#include "KeyBuffer.h"
#include "Notes.h"
#include "Presets.h"
#include "MidiManager.h"

// ---------------------------------------------------------

// PUSH BUTTONS & BANK CONFIG
// ---------------------------------------------------------
#define BUTTON_PRESET_PIN 15
#define BUTTON_OCTAVE_PIN 33


int currentPresetIndex = 0;
Preset* activePreset = soundBank[0];

unsigned long lastPresetTouch = 0;
unsigned long lastOctaveTouch = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 300;

// SETTINGS
int octave = 3;
bool printToSerial = true;
const int STARTING_NOTE_DISTANCE_FROM_MIDDLE_A = -9;
const int NUM_VOICES = BUFFER_MAX; // Max 4 voices

// LFOs for Modulation
Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> vibratoLfo(SIN2048_DATA);
Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> tremoloLfo(SIN2048_DATA);
int currentVibrato = 0;
int currentTremolo = 255;

// VOICE STRUCTURE
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

// Initialize 4 Voices
Voice voices[NUM_VOICES] = {
  { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 },
  { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 },
  { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 },
  { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 }
};

Notes notes(STARTING_NOTE_DISTANCE_FROM_MIDDLE_A);
KeyBuffer buffer;
volatile int activeVoiceCount = 0;

void blink(int count = 2, int wait = 200) {
  while (count >= 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(wait);
    digitalWrite(LED_BUILTIN, LOW);
    delay(wait);
    count--;
  }
}

void loadPreset(Preset* p) {
  activePreset = p;
  for (int i = 0; i < NUM_VOICES; i++) {
    voices[i].vco.setTable(p->waveform);
    voices[i].envelope.setADLevels(255, p->envSustainLevel);
    voices[i].envelope.setTimes(p->envAttack, p->envDecay, 1000000, p->envRelease);
  }
  vibratoLfo.setFreq(p->vibratoSpeed);
  tremoloLfo.setFreq(p->tremoloSpeed);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(BUTTON_PRESET_PIN, INPUT_PULLUP);
  pinMode(BUTTON_OCTAVE_PIN, INPUT_PULLUP);

  // Load the selected preset!
  loadPreset(activePreset);

  startMozzi(64); // Control rate of 64 Hz
  
  // Setup BLE MIDI
  MidiManager::begin();

  blink();
}

void updateControl() {
  // ----------------------------------------------------
  // PUSH BUTTONS (PRESETS & OCTAVE)
  // ----------------------------------------------------
  unsigned long now = millis();
  
  bool presetPressed = (digitalRead(BUTTON_PRESET_PIN) == LOW);
  bool octavePressed = (digitalRead(BUTTON_OCTAVE_PIN) == LOW);

  static unsigned long bothHeldStart = 0;
  static bool bothWereHeld = false;
  static bool ignoreNextRelease = false;
  static int playingMidiNote[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  static unsigned long previewSampleEndTime = 0;

  if (presetPressed && octavePressed) {
    if (!bothWereHeld) {
      bothWereHeld = true;
      bothHeldStart = now;
    } else if (now - bothHeldStart > 2000 && !ignoreNextRelease) {
      // Toggle MIDI Mode
      bool newMode = !MidiManager::isModeActive();
      MidiManager::setMode(newMode);
      ignoreNextRelease = true;
      if (printToSerial) {
        Serial.print("MIDI Mode: ");
        Serial.println(newMode ? "ON" : "OFF");
      }
      digitalWrite(LED_BUILTIN, newMode ? HIGH : LOW);
    }
  } else {
    bothWereHeld = false;
    if (ignoreNextRelease && !presetPressed && !octavePressed) {
      ignoreNextRelease = false;
    }
  }

  if (!bothWereHeld && !ignoreNextRelease) {
    if (presetPressed && (now - lastPresetTouch > TOUCH_DEBOUNCE_MS)) {
      lastPresetTouch = now;
      currentPresetIndex++;
      if (currentPresetIndex >= NUM_PRESETS) currentPresetIndex = 0;
      loadPreset(soundBank[currentPresetIndex]);
      octave = 3; // Reset octave to default when mode changes
      previewSampleEndTime = now + 400; // Preview sample for 400ms
      if (printToSerial) {
        Serial.print("Preset: SOUND_");
        Serial.println(currentPresetIndex + 1);
      }
    }
    
    if (octavePressed && (now - lastOctaveTouch > TOUCH_DEBOUNCE_MS)) {
      lastOctaveTouch = now;
      if (octave == 3) octave = 2;
      else if (octave == 2) octave = 4;
      else octave = 3;
      if (printToSerial) {
        Serial.print("Octave: ");
        Serial.println(octave);
      }
    }
  }

  // ----------------------------------------------------
  // MIDI & BUFFER LOGIC
  // ----------------------------------------------------
  bool isKeyActive[16] = {false};
  
  for (int i = 0; i < buffer.getSize(); i++) {
    char k = buffer.getAt(i);
    if (k >= 0 && k < 16) isKeyActive[k] = true;
  }

  MidiManager::processKeys(isKeyActive, octave);
  
  bool isPreviewing = false;
  if (!MidiManager::isModeActive() && (now < previewSampleEndTime)) {
    if (buffer.isEmpty()) {
      isPreviewing = true;
    } else {
      // Cancel preview if a real key is pressed
      previewSampleEndTime = 0; 
    }
  }

  if (MidiManager::isModeActive()) {
    activeVoiceCount = 0; // Silence local synth in MIDI mode
  } else if (isPreviewing) {
    activeVoiceCount = 1; // Force 1 voice for the preview
  } else {
    activeVoiceCount = buffer.getSize();
  }

  // Print buffer changes (and no LED flashing anymore!)
  static char lastKey = -1;
  if (!buffer.isEmpty()) {
    char currentKey = buffer.getFirst();
    if (currentKey != lastKey) {
      if (printToSerial) buffer.print();
      lastKey = currentKey;
    }
  } else {
    lastKey = -1;
  }

  // LFO Modulations
  currentVibrato = (vibratoLfo.next() * activePreset->vibratoDepth) >> 8; 
  currentTremolo = 255 - (((tremoloLfo.next() + 128) * activePreset->tremoloDepth) >> 8);

  // VOICE ALLOCATION LOGIC
  
  if (activePreset->numVoices == 1) {
    // ----------------------------------------------------
    // MONOPHONIC MODE (Last-Note Priority & Glide)
    // ----------------------------------------------------
    if (activeVoiceCount > 0) {
      char key = isPreviewing ? 0 : buffer.getAt(0); // Index 0 is the most recently pressed key
      float targetFreq = notes.get(key) / 4 * pow(2, octave);
      float steps = (float)(activePreset->glideTimeMs * AUDIO_RATE) / 1000.0f;
      
      if (!voices[0].isActive) {
        // Key just pressed, no previous note held -> Jump immediately
        voices[0].glide.set(targetFreq, targetFreq, 1);
        voices[0].currentFreq = targetFreq;
        voices[0].targetFreq = targetFreq;
        voices[0].isActive = true;
        voices[0].currentKey = key;
        voices[0].envelope.noteOn();
      } else if (voices[0].currentKey != key) {
        // A new key was pressed while playing (or within tolerance) -> Glide to it
        voices[0].targetFreq = targetFreq;
        if (steps > 0) {
          voices[0].glide.set(voices[0].currentFreq, targetFreq, steps);
        } else {
          voices[0].glide.set(targetFreq, targetFreq, 1);
        }
        voices[0].isActive = true; // Resume if we were in the tolerance window
        voices[0].currentKey = key;
        voices[0].envelope.noteOn();
      }
      voices[0].legatoFrames = 0; // Reset tolerance counter
    } else {
      // No keys pressed
      if (voices[0].isActive) {
        voices[0].envelope.noteOff();
        voices[0].legatoFrames++;
        // Give a 4-frame (~60ms) tolerance window before breaking the legato/glide
        if (voices[0].legatoFrames > 4) {
          voices[0].isActive = false;
        }
      }
    }
    
  } else {
    // ----------------------------------------------------
    // POLYPHONIC MODE 
    // ----------------------------------------------------
    bool matched[NUM_VOICES] = {false};
    
    // First, maintain currently held keys
    for (int i = 0; i < activeVoiceCount; i++) {
      char key = isPreviewing ? 0 : buffer.getAt(i);
      
      for (int v = 0; v < NUM_VOICES; v++) {
        if (voices[v].isActive && voices[v].currentKey == key && !matched[v]) {
          matched[v] = true; // Key is still held
          voices[v].legatoFrames = 0;
          break;
        }
      }
    }
    
    // Process key releases
    for (int v = 0; v < NUM_VOICES; v++) {
      if (voices[v].isActive && !matched[v]) {
        voices[v].envelope.noteOff();
        voices[v].legatoFrames++;
        if (voices[v].legatoFrames > 4) {
          voices[v].isActive = false; // Officially free after tolerance
        }
      }
    }
    
    // Find min and max keys for glide logic
    char minKey = 127;
    char maxKey = -1;
    for (int i = 0; i < activeVoiceCount; i++) {
      char k = isPreviewing ? 0 : buffer.getAt(i);
      if (k < minKey) minKey = k;
      if (k > maxKey) maxKey = k;
    }
    
    // Now allocate new keys
    for (int i = 0; i < activeVoiceCount; i++) {
      char key = isPreviewing ? 0 : buffer.getAt(i);
      bool alreadyAssigned = false;
      
      for (int v = 0; v < NUM_VOICES; v++) {
        if (voices[v].isActive && voices[v].currentKey == key) {
          alreadyAssigned = true;
          break;
        }
      }
      
      if (!alreadyAssigned) {
        // Find a free voice
        int allocatedVoice = -1;
        
        // Priority 1: Completely inactive voices
        for (int v = 0; v < NUM_VOICES; v++) {
          if (!voices[v].isActive) {
            allocatedVoice = v;
            break;
          }
        }
        
        // Priority 2: Voices in release phase (isActive but not matched, meaning they were released but within tolerance)
        if (allocatedVoice == -1) {
          for (int v = 0; v < NUM_VOICES; v++) {
            if (voices[v].isActive && !matched[v]) {
              allocatedVoice = v;
              break;
            }
          }
        }
        
        // Allocate!
        if (allocatedVoice != -1) {
          int v = allocatedVoice;
          float targetFreq = notes.get(key) / 4 * pow(2, octave);
          float steps = (float)(activePreset->glideTimeMs * AUDIO_RATE) / 1000.0f;
          voices[v].targetFreq = targetFreq;
          
          if (!voices[v].isActive) {
            // Jump
            voices[v].glide.set(targetFreq, targetFreq, 1);
            voices[v].currentFreq = targetFreq;
          } else {
            // Determine if this voice should glide based on preset
            bool shouldGlide = (activePreset->polyGlideMode == GLIDE_ALL);
            if (activePreset->polyGlideMode == GLIDE_LOWEST && key == minKey) shouldGlide = true;
            if (activePreset->polyGlideMode == GLIDE_HIGHEST && key == maxKey) shouldGlide = true;
            
            // Glide from current frequency (since it was within tolerance)
            if (steps > 0 && shouldGlide) {
              voices[v].glide.set(voices[v].currentFreq, targetFreq, steps);
            } else {
              voices[v].glide.set(targetFreq, targetFreq, 1);
            }
          }
          
          voices[v].isActive = true;
          voices[v].currentKey = key;
          matched[v] = true;
          voices[v].legatoFrames = 0;
          voices[v].envelope.noteOn();
        }
      }
    }
  }
  
  // ----------------------------------------------------
  // UPDATE ALL ACTIVE ENVELOPES
  // ----------------------------------------------------
  for (int v = 0; v < NUM_VOICES; v++) {
    voices[v].envelope.update(); 
  }
}

int updateAudio() {
  long asig = 0;
  
  // Mix active voices
  for (int v = 0; v < NUM_VOICES; v++) {
    if (voices[v].envelope.playing()) {
      float nextFreq = voices[v].glide.next();
      
      // Clamp frequency to stop Line from extrapolating to infinity
      if ((voices[v].currentFreq <= voices[v].targetFreq && nextFreq >= voices[v].targetFreq) ||
          (voices[v].currentFreq >= voices[v].targetFreq && nextFreq <= voices[v].targetFreq)) {
        voices[v].currentFreq = voices[v].targetFreq;
        voices[v].glide.set(voices[v].targetFreq, voices[v].targetFreq, 1);
      } else {
        voices[v].currentFreq = nextFreq;
      }
      
      voices[v].vco.setFreq(voices[v].currentFreq + currentVibrato);

      long osc_out = voices[v].vco.next();
      long env_out = voices[v].envelope.next();
      asig += (osc_out * env_out) >> 8; 
    }
  }
  
  // Global Tremolo
  asig = (asig * currentTremolo) >> 8;
  
  // Volume scaling depending on the preset's master volume
  asig = (asig * activePreset->masterVolume) >> 8;
  
  // Le DAC interne de l'ESP32 et la librairie Mozzi attendent une valeur 
  // qui ne dépasse pas les limites d'un entier 8 bits (env -128 à 127).
  // Si on dépasse, la valeur "déborde" (overflow) et crée une distorsion horrible.
  if (asig > 127) asig = 127;
  if (asig < -128) asig = -128;
  
  return (int)asig;
}

void loop() {
  audioHook(); 
  buffer.populate(); 
}
