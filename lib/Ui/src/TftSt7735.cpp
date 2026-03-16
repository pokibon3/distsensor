#include "TftSt7735.h"

namespace {
constexpr uint8_t kMadctl = 0xA0;

constexpr uint8_t kGlyphSpace[5] = {0, 0, 0, 0, 0};
constexpr uint8_t kGlyphDash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
constexpr uint8_t kGlyphDot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
constexpr uint8_t kGlyphColon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};

constexpr uint8_t kDigits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x62, 0x51, 0x49, 0x49, 0x46}, {0x22, 0x49, 0x49, 0x49, 0x36},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x2F, 0x49, 0x49, 0x49, 0x31},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
};

constexpr uint8_t kUpper[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x30, 0x40, 0x40, 0x40, 0x3F},
    {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, {0x7F, 0x02, 0x04, 0x08, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x09, 0x19, 0x66},
    {0x26, 0x49, 0x49, 0x49, 0x32}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x3C, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x71, 0x49, 0x45, 0x43, 0x00},
};

constexpr bool kSegments[10][7] = {
    {true, true, true, true, true, true, false},
    {false, true, true, false, false, false, false},
    {true, true, false, true, true, false, true},
    {true, true, true, true, false, false, true},
    {false, true, true, false, false, true, true},
    {true, false, true, true, false, true, true},
    {true, false, true, true, true, true, true},
    {true, true, true, false, false, false, false},
    {true, true, true, true, true, true, true},
    {true, true, true, true, false, true, true},
};
}  // namespace

uint16_t TftSt7735::color565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void TftSt7735::begin() {
  funGpioInitAll();
  funPinMode(kPinCs, GPIO_CFGLR_OUT_10Mhz_PP);
  funPinMode(kPinDc, GPIO_CFGLR_OUT_10Mhz_PP);
  funPinMode(kPinRst, GPIO_CFGLR_OUT_10Mhz_PP);
  funPinMode(kPinBl, GPIO_CFGLR_OUT_10Mhz_PP);
  funDigitalWrite(kPinCs, FUN_HIGH);
  funDigitalWrite(kPinBl, FUN_HIGH);

  initSpi();
  resetPanel();

  writeCommand(0x11);
  Delay_Ms(120);

  writeCommand(0xB1);
  writeData8(0x01);
  writeData8(0x2C);
  writeData8(0x2D);

  writeCommand(0xB2);
  writeData8(0x01);
  writeData8(0x2C);
  writeData8(0x2D);

  writeCommand(0xB3);
  writeData8(0x01);
  writeData8(0x2C);
  writeData8(0x2D);
  writeData8(0x01);
  writeData8(0x2C);
  writeData8(0x2D);

  writeCommand(0xB4);
  writeData8(0x07);

  writeCommand(0xC0);
  writeData8(0xA2);
  writeData8(0x02);
  writeData8(0x84);

  writeCommand(0xC1);
  writeData8(0xC5);

  writeCommand(0xC2);
  writeData8(0x0A);
  writeData8(0x00);

  writeCommand(0xC3);
  writeData8(0x8A);
  writeData8(0x2A);

  writeCommand(0xC4);
  writeData8(0x8A);
  writeData8(0xEE);

  writeCommand(0xC5);
  writeData8(0x0E);

  writeCommand(0x20);

  writeCommand(0x36);
  writeData8(kMadctl);

  writeCommand(0x3A);
  writeData8(0x05);

  writeCommand(0x2A);
  writeData8(0x00);
  writeData8(0x00);
  writeData8(0x00);
  writeData8(0x4F);

  writeCommand(0x2B);
  writeData8(0x00);
  writeData8(0x00);
  writeData8(0x00);
  writeData8(0x9F);

  writeCommand(0xE0);
  const uint8_t gammaPos[] = {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                              0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10};
  for (uint8_t v : gammaPos) writeData8(v);

  writeCommand(0xE1);
  const uint8_t gammaNeg[] = {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                              0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10};
  for (uint8_t v : gammaNeg) writeData8(v);

  writeCommand(0x13);
  Delay_Ms(10);
  writeCommand(0x29);
  Delay_Ms(20);
}

void TftSt7735::initSpi() {
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;
  funPinMode(PC5, GPIO_CFGLR_OUT_10Mhz_AF_PP);
  funPinMode(PC6, GPIO_CFGLR_OUT_10Mhz_AF_PP);

  SPI1->CTLR1 = SPI_CPHA_1Edge | SPI_CPOL_Low | SPI_Mode_Master |
                SPI_Direction_1Line_Tx | SPI_NSS_Soft | SPI_BaudRatePrescaler_4 |
                SPI_DataSize_8b | SPI_FirstBit_MSB;
  SPI1->CTLR1 |= CTLR1_SPE_Set;
}

void TftSt7735::resetPanel() {
  funDigitalWrite(kPinRst, FUN_HIGH);
  Delay_Ms(20);
  funDigitalWrite(kPinRst, FUN_LOW);
  Delay_Ms(20);
  funDigitalWrite(kPinRst, FUN_HIGH);
  Delay_Ms(120);
}

void TftSt7735::spiWrite(uint8_t data) {
  while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {
  }
  SPI1->DATAR = data;
  while (SPI1->STATR & SPI_I2S_FLAG_BSY) {
  }
}

void TftSt7735::writeCommand(uint8_t command) {
  funDigitalWrite(kPinCs, FUN_LOW);
  funDigitalWrite(kPinDc, FUN_LOW);
  spiWrite(command);
  funDigitalWrite(kPinCs, FUN_HIGH);
}

void TftSt7735::writeData8(uint8_t data) {
  funDigitalWrite(kPinCs, FUN_LOW);
  funDigitalWrite(kPinDc, FUN_HIGH);
  spiWrite(data);
  funDigitalWrite(kPinCs, FUN_HIGH);
}

void TftSt7735::writeData16(uint16_t data) {
  funDigitalWrite(kPinCs, FUN_LOW);
  funDigitalWrite(kPinDc, FUN_HIGH);
  spiWrite(data >> 8);
  spiWrite(data & 0xFF);
  funDigitalWrite(kPinCs, FUN_HIGH);
}

void TftSt7735::writeRepeatedColor(uint16_t color, uint32_t count) {
  funDigitalWrite(kPinCs, FUN_LOW);
  funDigitalWrite(kPinDc, FUN_HIGH);
  while (count--) {
    spiWrite(color >> 8);
    spiWrite(color & 0xFF);
  }
  funDigitalWrite(kPinCs, FUN_HIGH);
}

void TftSt7735::setAddressWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
  const uint16_t x0 = x + kXOffset;
  const uint16_t x1 = x0 + w - 1;
  const uint16_t y0 = y + kYOffset;
  const uint16_t y1 = y0 + h - 1;

  writeCommand(0x2A);
  writeData16(x0);
  writeData16(x1);

  writeCommand(0x2B);
  writeData16(y0);
  writeData16(y1);

  writeCommand(0x2C);
}

void TftSt7735::fillScreen(uint16_t color) {
  fillRect(0, 0, kWidth, kHeight, color);
}

void TftSt7735::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x >= kWidth || y >= kHeight || w <= 0 || h <= 0) return;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > kWidth) w = kWidth - x;
  if (y + h > kHeight) h = kHeight - y;
  setAddressWindow(x, y, w, h);
  writeRepeatedColor(color, static_cast<uint32_t>(w) * h);
}

void TftSt7735::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y + h - 1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x + w - 1, y, h, color);
}

void TftSt7735::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRect(x, y, w, 1, color);
}

void TftSt7735::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRect(x, y, 1, h, color);
}

const uint8_t *TftSt7735::glyphFor(char c) const {
  if (c >= '0' && c <= '9') return kDigits[c - '0'];
  if (c >= 'A' && c <= 'Z') return kUpper[c - 'A'];
  if (c == ' ') return kGlyphSpace;
  if (c == '-') return kGlyphDash;
  if (c == '.') return kGlyphDot;
  if (c == ':') return kGlyphColon;
  return kGlyphSpace;
}

void TftSt7735::drawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
  const uint8_t *glyph = glyphFor(c);
  for (uint8_t col = 0; col < 5; ++col) {
    uint8_t bits = glyph[col];
    for (uint8_t row = 0; row < 7; ++row) {
      const uint16_t color = (bits & (1 << row)) ? fg : bg;
      fillRect(x + col * scale, y + row * scale, scale, scale, color);
    }
  }
  fillRect(x + 5 * scale, y, scale, 7 * scale, bg);
}

void TftSt7735::drawText(int16_t x, int16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale) {
  while (*text) {
    char c = *text++;
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    drawChar(x, y, c, fg, bg, scale);
    x += 6 * scale;
  }
}

void TftSt7735::drawDigit(int16_t x, int16_t y, uint8_t digit, uint16_t onColor, uint16_t offColor, uint8_t scale) {
  const int16_t t = 3 * scale;
  const int16_t segW = 16 * scale;
  const int16_t segH = 24 * scale;

  auto segColor = [&](int idx) { return kSegments[digit][idx] ? onColor : offColor; };

  fillRect(x + t, y, segW, t, segColor(0));
  fillRect(x + t + segW, y + t, t, segH, segColor(1));
  fillRect(x + t + segW, y + segH + 2 * t, t, segH, segColor(2));
  fillRect(x + t, y + 2 * segH + 2 * t, segW, t, segColor(3));
  fillRect(x, y + segH + 2 * t, t, segH, segColor(4));
  fillRect(x, y + t, t, segH, segColor(5));
  fillRect(x + t, y + segH + t, segW, t, segColor(6));
}

void TftSt7735::drawMinus(int16_t x, int16_t y, uint16_t color, uint16_t bg, uint8_t scale) {
  fillRect(x, y, 22 * scale, 33 * scale, bg);
  fillRect(x + 3 * scale, y + 15 * scale, 16 * scale, 3 * scale, color);
}
