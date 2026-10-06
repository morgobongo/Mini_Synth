#include <Arduino.h>
#include "HardwarePins.h"
#include "Presets.h"
#include "SynthEngine.h"
#include "KeyBuffer.h"
#include "Notes.h"

// ---------------------------------------------------------
// CONFIG
// ---------------------------------------------------------

int currentPresetIndex = 0;
unsigned long lastPresetTouch = 0;
unsigned long lastOctaveTouch = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 300;

// SETTINGS
int octave = 3;
bool printToSerial = true;
const int STARTING_NOTE_DISTANCE_FROM_MIDDLE_A = -9;

// ---------------------------------------------------------
// GLOBALS
// ---------------------------------------------------------
Notes notes(STARTING_NOTE_DISTANCE_FROM_MIDDLE_A);
KeyBuffer buffer;
SynthEngine synth(notes);

void blink(int count = 2, int wait = 200) {
  while (count >= 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(wait);
    digitalWrite(LED_BUILTIN, LOW);
    delay(wait);
    count--;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(PIN_BUTTON_PRESET, INPUT_PULLUP);
  pinMode(PIN_BUTTON_OCTAVE, INPUT_PULLUP);

  // Load the initial preset!
  synth.loadPreset(soundBank[currentPresetIndex]);

  synth.init();
  blink();
}

void updateControl() {
  // ----------------------------------------------------
  // PUSH BUTTONS (PRESETS & OCTAVE)
  // ----------------------------------------------------
  unsigned long now = millis();
  
  if (digitalRead(PIN_BUTTON_PRESET) == LOW && (now - lastPresetTouch > TOUCH_DEBOUNCE_MS)) {
    lastPresetTouch = now;
    
    currentPresetIndex++;
    if (currentPresetIndex >= NUM_PRESETS) {
      currentPresetIndex = 0;
    }
    
    synth.loadPreset(soundBank[currentPresetIndex]);
    if (printToSerial) {
      Serial.print("Preset: SOUND_");
      Serial.println(currentPresetIndex + 1);
    }
  }
  
  if (digitalRead(PIN_BUTTON_OCTAVE) == LOW && (now - lastOctaveTouch > TOUCH_DEBOUNCE_MS)) {
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

  // Print buffer changes for debug
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

  // Delegate the rest of the control updates (voice allocation, LFOs) to the SynthEngine
  synth.updateControl(buffer, octave);
}

int updateAudio() {
  // Delegate audio generation to the SynthEngine
  return synth.updateAudio();
}

void loop() {
  audioHook(); 
  buffer.populate(); 
}
