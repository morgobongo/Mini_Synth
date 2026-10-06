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

// ---------------------------------------------------------
// WAVEFORM ALIASES
// ---------------------------------------------------------
const int8_t* square = SQUARE_NO_ALIAS_2048_DATA;
const int8_t* saw = SAW2048_DATA;
const int8_t* sine = SIN2048_DATA;

// ---------------------------------------------------------
// PRESET ABSTRACTION
// ---------------------------------------------------------

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

// Preset 1: Original Oskitone (Monophonic, Glide, Gate Envelope)
Preset SOUND_1 = {
  1,                            // numVoices (Monophonic)
  square,                       // waveform
  50,                           // glideTimeMs
  2,                            // envAttack (fast gate)
  2,                            // envDecay
  255,                          // envSustainLevel (full volume)
  2,                            // envRelease (fast gate cut)
  0,                            // vibratoDepth
  6.0f,                         // vibratoSpeed
  0,                            // tremoloDepth
  4.0f,                         // tremoloSpeed
  GLIDE_ALL,                    // polyGlideMode
  128                           // masterVolume (Square mono est fort, on réduit à ~50%)
};

// Preset 2: Modular Canvas (Polyphonic, ADSR, LFOs, Sawtooth)
// -------------------------------------------------------------
// Guide des paramètres (Parameters Guide) :
// - numVoices : 1 (Monophonique) ou 4 (Polyphonique)
// - waveform : square (Carrée), saw (Scie), sine (Sinus)
// - glideTimeMs : Temps de glissement (portamento) en millisecondes. 0 pour désactiver.
// - envAttack / envDecay / envRelease : Temps des phases ADSR en ms (ex: 2 pour percussif, 1000 pour lent)
// - envSustainLevel : Volume de la note tenue, de 0 (silence) à 255 (volume maximum)
// - vibratoDepth : Profondeur du LFO sur le pitch (0 = désactivé, 5 = léger, 20+ = intense)
// - vibratoSpeed : Vitesse du vibrato en Hz (ex: 6.0f)
// - tremoloDepth : Profondeur du LFO sur le volume (0 = désactivé, 100 = moyen, 255 = haché)
// - tremoloSpeed : Vitesse du tremolo en Hz (ex: 4.0f)
// - polyGlideMode : (Seulement si numVoices > 1) 
//                   GLIDE_ALL (glisse tout), GLIDE_LOWEST (note grave), GLIDE_HIGHEST (note aiguë)
// - masterVolume : Volume de sortie (0-255). Utile pour équilibrer les ondes ou la polyphonie.
// -------------------------------------------------------------
Preset SOUND_2 = {
  4,                            // numVoices (Polyphonic)
  square,                       // waveform
  0,                            // glideTimeMs
  40,                           // envAttack (fast gate)
  40,                           // envDecay
  255,                          // envSustainLevel (full volume)
  30,                           // envRelease (fast gate cut)
  25,                           // vibratoDepth
  6.0f,                         // vibratoSpeed
  0,                            // tremoloDepth
  4.0f,                         // tremoloSpeed
  GLIDE_LOWEST,                 // polyGlideMode
  80                            // masterVolume (Square 4 voix, on réduit beaucoup pour éviter saturation)
};

// Preset 3: Lead Synth (Mono, Sawtooth, Long Glide)
Preset SOUND_3 = {
  1,                            // numVoices (Monophonic)
  saw,                          // waveform
  150,                          // glideTimeMs
  100,                          // envAttack (soft start)
  50,                           // envDecay
  200,                          // envSustainLevel
  500,                          // envRelease (long tail)
  10,                           // vibratoDepth
  4.0f,                         // vibratoSpeed
  0,                            // tremoloDepth
  0.0f,                         // tremoloSpeed
  GLIDE_ALL,                    // polyGlideMode
  160                           // masterVolume (Saw mono est un peu fort, on réduit)
};

// Preset 4: Oskitone Polyphonique (Square, Polyphonic, Glide)
Preset SOUND_4 = {
  4,                            // numVoices (Polyphonic)
  square,                       // waveform
  50,                           // glideTimeMs
  2,                            // envAttack (fast gate)
  2,                            // envDecay
  255,                          // envSustainLevel (full volume)
  2,                            // envRelease (fast gate cut)
  0,                            // vibratoDepth
  6.0f,                         // vibratoSpeed
  0,                            // tremoloDepth
  4.0f,                         // tremoloSpeed
  GLIDE_ALL,                    // polyGlideMode
  80                            // masterVolume (Réduit pour 4 voix)
};

// ---------------------------------------------------------
// PUSH BUTTONS & BANK CONFIG
// ---------------------------------------------------------
#define BUTTON_PRESET_PIN 15
#define BUTTON_OCTAVE_PIN 33

Preset* soundBank[] = { &SOUND_1, &SOUND_2, &SOUND_3, &SOUND_4 };
const int NUM_PRESETS = 4;
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
  blink();
}

void updateControl() {
  // ----------------------------------------------------
  // PUSH BUTTONS (PRESETS & OCTAVE)
  // ----------------------------------------------------
  unsigned long now = millis();
  
  if (digitalRead(BUTTON_PRESET_PIN) == LOW && (now - lastPresetTouch > TOUCH_DEBOUNCE_MS)) {
    lastPresetTouch = now;
    
    currentPresetIndex++;
    if (currentPresetIndex >= NUM_PRESETS) {
      currentPresetIndex = 0;
    }
    
    loadPreset(soundBank[currentPresetIndex]);
    if (printToSerial) {
      Serial.print("Preset: SOUND_");
      Serial.println(currentPresetIndex + 1);
    }
  }
  
  if (digitalRead(BUTTON_OCTAVE_PIN) == LOW && (now - lastOctaveTouch > TOUCH_DEBOUNCE_MS)) {
    lastOctaveTouch = now;
    if (octave == 3) {
      octave = 2;
    } else if (octave == 2) {
      octave = 4;
    } else {
      octave = 3;
    }
    if (printToSerial) {
      Serial.print("Octave: ");
      Serial.println(octave);
    }
  }

  activeVoiceCount = buffer.getSize();

  // Print buffer changes
  static char lastKey = -1;
  if (!buffer.isEmpty()) {
    char currentKey = buffer.getFirst();
    if (currentKey != lastKey) {
      if (printToSerial) {
        buffer.print();
      }
      lastKey = currentKey;
    }
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    lastKey = -1;
    digitalWrite(LED_BUILTIN, LOW);
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
      char key = buffer.getAt(0); // Index 0 is the most recently pressed key
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
      char key = buffer.getAt(i);
      
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
      char k = buffer.getAt(i);
      if (k < minKey) minKey = k;
      if (k > maxKey) maxKey = k;
    }
    
    // Now allocate new keys
    for (int i = 0; i < activeVoiceCount; i++) {
      char key = buffer.getAt(i);
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
