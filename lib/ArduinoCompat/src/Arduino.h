#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ch32fun.h"

using boolean = bool;
using byte = uint8_t;

constexpr uint8_t HIGH = 1;
constexpr uint8_t LOW = 0;
constexpr uint8_t OUTPUT = 1;
constexpr uint8_t INPUT = 0;

uint32_t millis();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
