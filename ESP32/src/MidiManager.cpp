#include "MidiManager.h"
#include <BLEMidi.h>
#include <Arduino.h>

bool MidiManager::_midiMode = false;
int MidiManager::_playingMidiNote[17] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
bool MidiManager::_wasKeyActive[17] = {false};
bool MidiManager::_sustainActive = false;

void MidiManager::begin() {
    BLEMidiServer.begin("Mini Synth");
}

void MidiManager::setMode(bool active) {
    _midiMode = active;
    if (!_midiMode) {
        turnOffAllNotes();
        if (_sustainActive) {
            BLEMidiServer.controlChange(0, 64, 0);
            _sustainActive = false;
        }
    }
}

bool MidiManager::isModeActive() {
    return _midiMode;
}

void MidiManager::setSustain(bool active) {
    if (!_midiMode) return;
    if (active != _sustainActive) {
        _sustainActive = active;
        delay(3); // Wait for any pending note messages to clear
        BLEMidiServer.controlChange(0, 64, active ? 127 : 0);
        delay(3); // Give BLE stack time to process the CC
    }
}

void MidiManager::turnOffAllNotes() {
    for(int i = 0; i < 17; i++) {
        if (_playingMidiNote[i] != -1) {
            BLEMidiServer.noteOff(0, _playingMidiNote[i], 0);
            _playingMidiNote[i] = -1;
            delay(2);
        }
    }
}

void MidiManager::processKeys(bool isKeyActive[17], int octave) {
    int messagesSent = 0;
    for (int i = 0; i < 17; i++) {
        if (isKeyActive[i] && !_wasKeyActive[i]) {
            int note = 60 + i + (octave - 2) * 12;
            if (_midiMode) {
                BLEMidiServer.noteOn(0, note, 127);
                messagesSent++;
            }
            _playingMidiNote[i] = note;
            _wasKeyActive[i] = true;
            if (messagesSent >= 1) break;
        } else if (!isKeyActive[i] && _wasKeyActive[i]) {
            if (_midiMode && _playingMidiNote[i] != -1) {
                BLEMidiServer.noteOff(0, _playingMidiNote[i], 0);
                messagesSent++;
            }
            _playingMidiNote[i] = -1;
            _wasKeyActive[i] = false;
            if (messagesSent >= 1) break;
        }
    }
}
