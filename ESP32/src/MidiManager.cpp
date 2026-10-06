#include "MidiManager.h"
#include <BLEMidi.h>

bool MidiManager::_midiMode = false;
int MidiManager::_playingMidiNote[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
bool MidiManager::_wasKeyActive[16] = {false};

void MidiManager::begin() {
    BLEMidiServer.begin("Mini Synth");
}

void MidiManager::setMode(bool active) {
    _midiMode = active;
    if (!_midiMode) {
        turnOffAllNotes();
    }
}

bool MidiManager::isModeActive() {
    return _midiMode;
}

void MidiManager::turnOffAllNotes() {
    for(int i = 0; i < 16; i++) {
        if (_playingMidiNote[i] != -1) {
            BLEMidiServer.noteOff(0, _playingMidiNote[i], 0);
            _playingMidiNote[i] = -1;
        }
    }
}

void MidiManager::processKeys(bool isKeyActive[16], int octave) {
    for (int i = 0; i < 16; i++) {
        if (isKeyActive[i] && !_wasKeyActive[i]) {
            int note = 60 + i + (octave - 2) * 12;
            if (_midiMode) {
                BLEMidiServer.noteOn(0, note, 127);
            }
            _playingMidiNote[i] = note;
            _wasKeyActive[i] = true;
        } else if (!isKeyActive[i] && _wasKeyActive[i]) {
            if (_midiMode && _playingMidiNote[i] != -1) {
                BLEMidiServer.noteOff(0, _playingMidiNote[i], 0);
            }
            _playingMidiNote[i] = -1;
            _wasKeyActive[i] = false;
        }
    }
}
