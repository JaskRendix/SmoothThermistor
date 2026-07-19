#pragma once
#include <cstdint>

inline std::uint16_t analogRead(std::uint8_t) { return 512; }
inline void delay(unsigned long) {}

// Simple monotonic millis() for tests
inline unsigned long millis() {
  static unsigned long t = 0;
  return ++t;
}

inline void analogReference(int) {}

#define DEFAULT 0
#define EXTERNAL 1
