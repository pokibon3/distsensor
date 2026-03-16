#pragma once

#include <stdint.h>
#include "ch32fun.h"

class TftSt7735 {
 public:
  static constexpr int16_t kWidth = 160;
  static constexpr int16_t kHeight = 80;

  void begin();
  void fillScreen(uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawText(int16_t x, int16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale = 1);
  void drawDigit(int16_t x, int16_t y, uint8_t digit, uint16_t onColor, uint16_t offColor, uint8_t scale = 1);
  void drawMinus(int16_t x, int16_t y, uint16_t color, uint16_t bg, uint8_t scale = 1);

  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

 private:
  static constexpr uint8_t kPinCs = PC0;
  static constexpr uint8_t kPinDc = PD3;
  static constexpr uint8_t kPinRst = PD4;
  static constexpr uint8_t kPinBl = PD5;
  static constexpr uint8_t kXOffset = 0;
  static constexpr uint8_t kYOffset = 24;

  void initSpi();
  void resetPanel();
  void writeCommand(uint8_t command);
  void writeData8(uint8_t data);
  void writeData16(uint16_t data);
  void writeRepeatedColor(uint16_t color, uint32_t count);
  void setAddressWindow(int16_t x, int16_t y, int16_t w, int16_t h);
  void spiWrite(uint8_t data);
  void drawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
  const uint8_t *glyphFor(char c) const;
};

