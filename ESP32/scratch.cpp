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
        // Wait, if it's within tolerance, isActive is true, but matched[v] is false.
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
            // Glide from current frequency (since it was within tolerance)
            if (steps > 0) {
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
