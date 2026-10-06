#include "SynthEngine.h"
#include <tables/square_no_alias_2048_int8.h>

SynthEngine::SynthEngine(Notes& notesManager) : 
  voices {
    { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 },
    { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 },
    { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 },
    { Oscil<2048, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA), ADSR<CONTROL_RATE, AUDIO_RATE>(), Line<float>(), 440.0f, 440.0f, 0, false, 0 }
  },
  currentVibrato(0),
  currentTremolo(255),
  activePreset(nullptr),
  notes(notesManager), 
  activeVoiceCount(0),
  vibratoLfo(SIN2048_DATA),
  tremoloLfo(SIN2048_DATA) 
{
}

void SynthEngine::init() {
  startMozzi(64); // Control rate of 64 Hz
}

void SynthEngine::loadPreset(Preset* p) {
  activePreset = p;
  for (int i = 0; i < NUM_VOICES; i++) {
    voices[i].vco.setTable(p->waveform);
    voices[i].envelope.setADLevels(255, p->envSustainLevel);
    voices[i].envelope.setTimes(p->envAttack, p->envDecay, 1000000, p->envRelease);
  }
  vibratoLfo.setFreq(p->vibratoSpeed);
  tremoloLfo.setFreq(p->tremoloSpeed);
}

void SynthEngine::updateControl(KeyBuffer& buffer, int octave) {
  activeVoiceCount = buffer.getSize();

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

int SynthEngine::updateAudio() {
  long asig = 0;
  int playingVoices = 0;
  
  // Mix active voices
  for (int v = 0; v < NUM_VOICES; v++) {
    if (voices[v].envelope.playing()) {
      playingVoices++;
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
  
  // Dynamic Polyphony Gain Compensation (Ducking)
  // Reduces overall volume when multiple notes are played to avoid clipping.
  if (playingVoices == 2) {
    asig = (asig * 204) >> 8; // 80% volume for 2 notes
  } else if (playingVoices == 3) {
    asig = (asig * 153) >> 8; // 60% volume for 3 notes
  } else if (playingVoices == 4) {
    asig = (asig * 102) >> 8; // 40% volume for 4 notes
  }
  
  // Volume scaling depending on the preset's master volume
  asig = (asig * activePreset->masterVolume) >> 8;
  
  // Clamp output to 8-bit limits (-128 to 127) to prevent DAC overflow and harsh clipping.
  if (asig > 127) asig = 127;
  if (asig < -128) asig = -128;
  
  return (int)asig;
}
