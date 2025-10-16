/*
 * RotaryEncoder.h
 * A class for handling rotary encoders using state table approach
 */

#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include "Particle.h"

class RotaryEncoder {
private:
  // Pin assignments
  int pinA;
  int pinB;

  // Position limits
  int minPos;
  int maxPos;

  // State tracking (volatile for ISR access)
  volatile int position;
  volatile uint8_t state;

  // State table for rotary encoder
  // [current_state][new_AB] = direction (-1, 0, or +1)
  static const int8_t stateTable[4][4];

  // Static instance pointers for ISR callbacks (max 2 encoders)
  static RotaryEncoder* instance0;
  static RotaryEncoder* instance1;

  // Static ISR callback functions
  static void isr0();
  static void isr1();

public:
  // Constructor
  RotaryEncoder(int pinA, int pinB, int minPos, int maxPos, int initialPos = 0);

  // Initialize the encoder (setup pins, read initial state, and attach interrupts)
  void begin();

  // Get current position
  int getPosition() const;

  // Set position
  void setPosition(int newPos);

  // Handle encoder state change (call from ISR)
  void update();

  // Get the current state (for debugging)
  uint8_t getState() const;
};

#endif
