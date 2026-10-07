#include "Arduino.h"
#include "KeyBuffer.h"

#define CIRCULAR_BUFFER_DEBUG
#include <CircularBuffer.hpp>
#include "HardwarePins.h"

const byte ROWS = 4;
const byte COLS = 5;
byte key_indexes[ROWS][COLS] = {
  {1, 5, 9, 12, 15},
  {2, 6, 10, 13, 16},
  {3, 7, 11, 14, 17},
  {4, 8}
};
byte rowPins[ROWS] = {PIN_MATRIX_ROW_1, PIN_MATRIX_ROW_2, PIN_MATRIX_ROW_3, PIN_MATRIX_ROW_4};
byte colPins[COLS] = {PIN_MATRIX_COL_1, PIN_MATRIX_COL_2, PIN_MATRIX_COL_3, PIN_MATRIX_COL_4, PIN_MATRIX_COL_5};

KeyBuffer::KeyBuffer() {
  for (int i = 0; i < 17; i++) {
    _physicalKeyState[i] = false;
    _lastPhysicalKeyState[i] = false;
    _lastDebounceTime[i] = 0;
  }
}

bool KeyBuffer::isEmpty() {
  return _buffer.isEmpty();
}

bool KeyBuffer::isInBuffer(int c) {
  bool value = false;

  if (!_buffer.isEmpty()) {
    for (int i = 0; i < _buffer.size(); i++) {
      if (c == _buffer[i]) {
        value = true;
        break;
      }
    }
  }

  return value;
}

bool KeyBuffer::removeFromBuffer(int c) {
  int newStack[BUFFER_MAX - 1] = {};
  int newI = 0;

  bool hasRemoval = false;

  for (int i = 0; i < _buffer.size(); i++) {
    if (c != _buffer[i]) {
      newStack[newI++] = _buffer[i];
    } else {
      hasRemoval = true;
    }
  }

  if (hasRemoval) {
    _buffer.clear();

    for (byte i = 0; i < newI; i++) {
      _buffer.push(newStack[i]);
    }
  }

  return hasRemoval;
}

void KeyBuffer::scanMatrix() {
  static bool initialized = false;
  if (!initialized) {
    for (int r = 0; r < ROWS; r++) {
      pinMode(rowPins[r], INPUT_PULLUP);
    }
    for (int c = 0; c < COLS; c++) {
      pinMode(colPins[c], INPUT);
    }
    initialized = true;
  }

  static unsigned long lastScanTime = 0;
  if (millis() - lastScanTime < 5) return;
  lastScanTime = millis();

  const int DEBOUNCE_DELAY = 10;
  
  for (int c = 0; c < COLS; c++) {
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], LOW);
    
    delayMicroseconds(10); 

    for (int r = 0; r < ROWS; r++) {
      int keyIndex = key_indexes[r][c] - 1;
      if (keyIndex >= 0 && keyIndex < 17) {
        bool isPressed = (digitalRead(rowPins[r]) == LOW);
        
        if (isPressed != _lastPhysicalKeyState[keyIndex]) {
          _lastDebounceTime[keyIndex] = millis();
        }
        
        if ((millis() - _lastDebounceTime[keyIndex]) > DEBOUNCE_DELAY) {
          if (isPressed != _physicalKeyState[keyIndex]) {
            _physicalKeyState[keyIndex] = isPressed;
          }
        }
        _lastPhysicalKeyState[keyIndex] = isPressed;
      }
    }
    
    pinMode(colPins[c], INPUT);
  }
}

void KeyBuffer::populate() {
  scanMatrix();
  
  bool isActive = false;
  for (int i = 0; i < 17; i++) {
    if (_physicalKeyState[i]) {
      if (!isInBuffer(i)) {
        _buffer.unshift(i);
      }
      isActive = true;
    } else {
      if (isInBuffer(i)) {
        removeFromBuffer(i);
      }
    }
  }

  if (!isActive) {
    _buffer.clear();
  }
}

void KeyBuffer::print() {
  if (!_buffer.isEmpty()) {
    Serial.print("[");
    for (int i = 0; i < _buffer.size(); i++) {
      Serial.print(_buffer[i]);

      if (i < _buffer.size() - 1) {
        Serial.print(",");
      }
    }
    Serial.print(
      "] ("
      + String(_buffer.size()) + "/" + String(BUFFER_MAX)
      + ")"
    );
    Serial.println();
  }
}

char KeyBuffer::getFirst() {
  return _buffer.first();
}

int KeyBuffer::getSize() {
  return _buffer.size();
}

char KeyBuffer::getAt(int index) {
  if (index >= 0 && index < _buffer.size()) {
    return _buffer[index];
  }
  return -1;
}

bool KeyBuffer::isPhysicallyPressed(int index) {
  if (index >= 0 && index < 17) {
    return _physicalKeyState[index];
  }
  return false;
}
