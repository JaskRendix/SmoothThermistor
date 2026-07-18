#pragma once
#include <cstdint>

inline int analogRead(uint8_t) { return 512; }
inline void delay(unsigned long) {}
inline void analogReference(int) {}

#define DEFAULT 0
#define EXTERNAL 1
