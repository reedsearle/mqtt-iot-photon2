/*
 * RotaryEncoder.cpp
 * Implementation of RotaryEncoder class
 */

#include "RotaryEncoder.h"

// Define the state table
const int8_t RotaryEncoder::stateTable[4][4] = {
  // Current State: 00      01      10      11
  {  0,  1, -1,  0 },  // New state from 00
  { -1,  0,  0,  1 },  // New state from 01
  {  1,  0,  0, -1 },  // New state from 10
  {  0, -1,  1,  0 }   // New state from 11
};

// Initialize static instance pointers
RotaryEncoder* RotaryEncoder::instance0 = nullptr;
RotaryEncoder* RotaryEncoder::instance1 = nullptr;

// Constructor
RotaryEncoder::RotaryEncoder(int pinA, int pinB, int minPos, int maxPos, int initialPos)
  : pinA(pinA), pinB(pinB), minPos(minPos), maxPos(maxPos), position(initialPos), state(0) {
  // Assign this instance to a static pointer for ISR access
  if (instance0 == nullptr) {
    instance0 = this;
  } else if (instance1 == nullptr) {
    instance1 = this;
  }
}

// Initialize the encoder
void RotaryEncoder::begin() {
  // Set up pins as inputs with pullups
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);

  // Read initial state
  // Original code for Photon 2:
  // bool a = digitalRead(pinA);
  // bool b = digitalRead(pinB);
  // Argon-compatible code (uses int instead of bool):
  int a = digitalRead(pinA);
  int b = digitalRead(pinB);
  state = (a << 1) | b;

  // Attach interrupts based on which instance this is
  if (this == instance0) {
    attachInterrupt(pinA, isr0, CHANGE);
    attachInterrupt(pinB, isr0, CHANGE);
  } else if (this == instance1) {
    attachInterrupt(pinA, isr1, CHANGE);
    attachInterrupt(pinB, isr1, CHANGE);
  }
}

// Get current position
int RotaryEncoder::getPosition() const {
  return position;
}

// Set position
void RotaryEncoder::setPosition(int newPos) {
  position = newPos;

  // Apply boundary checking
  if (position < minPos) position = minPos;
  if (position >= maxPos) position = maxPos - 1;
}

// Handle encoder state change (call from ISR)
void RotaryEncoder::update() {
  // Read current pin states
  // Original code for Photon 2:
  // bool a = digitalRead(pinA);
  // bool b = digitalRead(pinB);
  // Argon-compatible code (uses int instead of bool):
  int a = digitalRead(pinA);
  int b = digitalRead(pinB);

  // Combine A and B into a 2-bit value (0-3)
  uint8_t newState = (a << 1) | b;

  // Look up direction from state table
  int8_t direction = stateTable[state][newState];

  // Update position with boundary checking
  if (direction != 0) {
    position += direction;
    if (position < minPos) position = minPos;
    if (position >= maxPos) position = maxPos - 1;
  }

  // Save current state for next interrupt
  state = newState;
}

// Get the current state (for debugging)
uint8_t RotaryEncoder::getState() const {
  return state;
}

// Static ISR callbacks
void RotaryEncoder::isr0() {
  if (instance0 != nullptr) {
    instance0->update();
  }
}

void RotaryEncoder::isr1() {
  if (instance1 != nullptr) {
    instance1->update();
  }
}
