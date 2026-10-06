#pragma once

class MidiManager {
public:
    static void begin();
    static void setMode(bool active);
    static bool isModeActive();
    static void processKeys(bool isKeyActive[16], int octave);

private:
    static void turnOffAllNotes();
    
    static bool _midiMode;
    static int _playingMidiNote[16];
    static bool _wasKeyActive[16];
};
