#pragma once

#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"

class HardI2C {
 public:
  void begin();
  void setSpeed(uint32_t hz);
  bool write(uint8_t address, const uint8_t *data, uint8_t length);
  bool read(uint8_t address, uint8_t *data, uint8_t length);

 private:
  static constexpr uint8_t kSdaPin = PC1;
  static constexpr uint8_t kSclPin = PC2;
  uint32_t clock_hz_ = 100000;

  void configureTiming();
  bool generateStart();
  bool sendAddress(uint8_t addressWithRw);
  void generateStop();
  bool waitStar1Set(uint16_t mask, uint32_t timeout_ms = 10);
  bool waitTxeOrBtf(uint32_t timeout_ms = 10);
  bool waitRxne(uint32_t timeout_ms = 10);
  void clearAddrFlag();
  void clearErrorFlags();
};

class TwoWire {
 public:
  void begin();
  void setClock(uint32_t hz);
  void beginTransmission(uint8_t address);
  size_t write(uint8_t value);
  size_t write(const uint8_t *buffer, size_t size);
  uint8_t endTransmission();
  uint8_t requestFrom(uint8_t address, uint8_t quantity);
  int read();

 private:
  static constexpr uint8_t kBufferSize = 32;

  HardI2C bus_;
  uint8_t tx_address_ = 0;
  uint8_t tx_buffer_[kBufferSize] = {};
  uint8_t tx_length_ = 0;
  uint8_t rx_buffer_[kBufferSize] = {};
  uint8_t rx_length_ = 0;
  uint8_t rx_index_ = 0;
};

extern TwoWire Wire;
