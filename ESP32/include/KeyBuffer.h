#define CIRCULAR_BUFFER_DEBUG
#include <CircularBuffer.hpp>

#ifndef KeyBuffer_h
#define KeyBuffer_h

#include "Arduino.h"

#define BUFFER_MAX 4

class KeyBuffer {
  public:
    KeyBuffer();
    bool isEmpty();
    char getFirst();
    int getSize();
    char getAt(int index);
    bool isPhysicallyPressed(int index);
    void print();
    void printBuffer();
    void populate();
  private:
    CircularBuffer<int, BUFFER_MAX> _buffer;
    bool isInBuffer(int c);
    bool removeFromBuffer(int c);
    void scanMatrix();
    bool _physicalKeyState[17];
    bool _lastPhysicalKeyState[17];
    unsigned long _lastDebounceTime[17];
};

#endif
