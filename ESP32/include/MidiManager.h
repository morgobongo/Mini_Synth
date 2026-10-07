#pragma once

class MidiManager {
public:
    static void begin();
    static void setMode(bool active);
    static bool isModeActive();
    static void processKeys(bool isKeyActive[17], int octave);
    static void setSustain(bool active);

private:
    static void turnOffAllNotes();
    
    static bool _midiMode;
    static int _playingMidiNote[17];
    static bool _wasKeyActive[17];
    static bool _sustainActive;
};
