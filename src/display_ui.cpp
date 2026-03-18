#include "display_ui.h"
#include "st7735.h"

namespace {

uint16_t heatColor(uint8_t v) {
  if (v < 64) {
    return RGB(0, 0, (uint8_t)(v * 4));
  }
  if (v < 128) {
    return RGB(0, (uint8_t)((v - 64) * 4), 255);
  }
  if (v < 192) {
    return RGB((uint8_t)((v - 128) * 4), 255, (uint8_t)(255 - (v - 128) * 4));
  }
  return RGB(255, (uint8_t)(255 - (v - 192) * 4), 0);
}

void printMmLarge(uint16_t mm) {
  char out[7];
  uint16_t v = (mm > 9999U) ? 9999U : mm;
  out[0] = (char)('0' + (v / 1000U) % 10U);
  out[1] = (char)('0' + (v / 100U) % 10U);
  out[2] = (char)('0' + (v / 10U) % 10U);
  out[3] = (char)('0' + (v % 10U));
  out[4] = 'M';
  out[5] = 'M';
  out[6] = '\0';
  st7735_print(out, FONT_SCALE_16X16);
}

}  // namespace

void uiDrawBooting() {
  st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);
  st7735_set_background_color(BLACK);
  st7735_set_color(SKYBLUE);
  st7735_set_cursor(20, 30);
  st7735_print("BOOTING...", FONT_SCALE_16X16);
}

void uiDrawSensorError() {
  st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);
  st7735_set_background_color(BLACK);
  st7735_set_color(ORANGE);
  st7735_set_cursor(8, 28);
  st7735_print("SENSOR ERROR", FONT_SCALE_16X16);
  st7735_set_cursor(8, 52);
  st7735_print("CHECK WIRING", FONT_SCALE_8X8);
}

void uiStageHalt(const char *msg, uint16_t color) {
  st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);
  st7735_set_background_color(BLACK);
  st7735_set_color(color);
  st7735_set_cursor(8, 30);
  st7735_print(msg, FONT_SCALE_8X8);
  while (1) Delay_Ms(100);
}

#if defined(SENSOR_VL53L1X)

void uiDrawStaticVl53l1x(uint8_t roi_size, uint8_t grid, uint8_t cell, uint8_t origin_x, uint8_t origin_y) {
  st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);

  for (uint8_t y = 0; y < grid; ++y) {
    for (uint8_t x = 0; x < grid; ++x) {
      st7735_fill_rect((uint16_t)(origin_x + x * cell), (uint16_t)(origin_y + y * cell), cell - 1, cell - 1, RGB(6, 8, 12));
    }
  }
  st7735_draw_rect(origin_x - 1, origin_y - 1, grid * cell + 2, grid * cell + 2, SKYBLUE);

  st7735_set_background_color(BLACK);
  st7735_set_color(WHITE);
  st7735_set_cursor(72, 8);
  st7735_print("VL53L1X", FONT_SCALE_8X8);
  st7735_set_cursor(72, 18);
  if (roi_size == 16) st7735_print("ROI 16x16", FONT_SCALE_8X8);
  else if (roi_size == 8) st7735_print("ROI 8x8 ", FONT_SCALE_8X8);
  else if (roi_size == 4) st7735_print("ROI 4x4 ", FONT_SCALE_8X8);
  else st7735_print("ROI 1x1 ", FONT_SCALE_8X8);
}

void uiDrawHeatCellVl53l1x(uint8_t x, uint8_t y, uint8_t grid, uint8_t cell, uint8_t origin_x, uint8_t origin_y,
                           const uint8_t *heat, bool highlight) {
  const uint16_t idx = (uint16_t)y * grid + x;
  const uint16_t color = heatColor(heat[idx]);
  const uint8_t draw_x = (uint8_t)(grid - 1 - x);
  const uint16_t px = (uint16_t)(origin_x + draw_x * cell);
  const uint16_t py = (uint16_t)(origin_y + y * cell);
  st7735_fill_rect(px, py, cell - 1, cell - 1, color);
  if (highlight && cell >= 2) {
    st7735_draw_rect(px, py, cell - 1, cell - 1, WHITE);
  }
}

void uiDrawStatsVl53l1x(uint16_t cycle_min_mm, bool cycle_has_min, uint16_t current_mm, bool current_has_mm, bool timeout,
                        bool measurable) {
  char buf[16];
  st7735_fill_rect(72, 30, 84, 18, BLACK);
  st7735_fill_rect(72, 50, 84, 10, BLACK);

  st7735_set_background_color(BLACK);

  if (cycle_has_min) {
    st7735_set_color(WHITE);
    st7735_set_cursor(72, 30);
    printMmLarge(cycle_min_mm);
  } else {
    st7735_set_color(timeout ? ORANGE : DARKGREY);
    st7735_set_cursor(72, 30);
    st7735_print("----MM", FONT_SCALE_16X16);
  }

  st7735_set_cursor(72, 50);
  if (current_has_mm) {
    mini_snprintf(buf, sizeof(buf), "CUR:%4d", (int)current_mm);
    st7735_set_color(CYAN);
  } else {
    mini_snprintf(buf, sizeof(buf), "CUR:----");
    st7735_set_color(DARKGREY);
  }
  st7735_print(buf, FONT_SCALE_8X8);

  st7735_fill_rect(72, 66, 84, 10, BLACK);
  st7735_set_cursor(72, 66);
  if (timeout) {
    st7735_set_color(ORANGE);
    st7735_print("TIMEOUT", FONT_SCALE_8X8);
  } else if (measurable) {
    st7735_set_color(GREENYELLOW);
    st7735_print("TRACKING", FONT_SCALE_8X8);
  } else {
    st7735_set_color(DARKGREY);
    st7735_print("NO ECHO", FONT_SCALE_8X8);
  }
}

#elif defined(SENSOR_VL53L0X)

void uiDrawFrameVl53l0x(uint16_t accent) {
  st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);
  st7735_fill_rect(4, 4, 152, 72, RGB(8, 14, 24));
  st7735_draw_rect(4, 4, 152, 72, accent);
  st7735_draw_rect(6, 6, 148, 68, DARKGREY);

  st7735_set_background_color(RGB(8, 14, 24));
  st7735_set_color(WHITE);
  st7735_set_cursor(10, 10);
  st7735_print("DIST SENSOR", FONT_SCALE_8X8);

  st7735_set_color(accent);
  st7735_set_cursor(10, 20);
  st7735_print("VL53L0X  TOF", FONT_SCALE_8X8);

  st7735_draw_rect(12, 30, 100, 28, SKYBLUE);
  st7735_draw_rect(124, 14, 16, 52, LIGHTGREY);
}

void uiDrawGaugeVl53l0x(uint16_t mm, bool valid, uint16_t accent) {
  st7735_fill_rect(126, 16, 12, 48, RGB(22, 22, 28));
  if (!valid) return;
  const uint16_t capped = (mm > 1200) ? 1200 : mm;
  const uint16_t fill = (uint16_t)((capped * 46U) / 1200U);
  st7735_fill_rect(127, (uint16_t)(62 - fill), 10, fill, accent);
}

void uiDrawDistanceVl53l0x(uint16_t mm, bool valid, bool timeout) {
  char buf[16];
  st7735_fill_rect(14, 32, 96, 24, RGB(8, 14, 24));
  st7735_set_background_color(RGB(8, 14, 24));
  st7735_set_color(WHITE);
  st7735_set_cursor(14, 34);
  if (valid) {
    mini_snprintf(buf, sizeof(buf), "%4ldMM", (long)mm);
    st7735_print(buf, FONT_SCALE_16X16);
  } else {
    st7735_print("----MM", FONT_SCALE_16X16);
  }

  st7735_fill_rect(10, 62, 140, 10, RGB(8, 14, 24));
  st7735_set_cursor(10, 62);
  if (valid) {
    st7735_set_color(GREENYELLOW);
    st7735_print("STATUS: LIVE", FONT_SCALE_8X8);
  } else if (timeout) {
    st7735_set_color(ORANGE);
    st7735_print("STATUS: TIMEOUT", FONT_SCALE_8X8);
  } else {
    st7735_set_color(DARKGREY);
    st7735_print("STATUS: INIT", FONT_SCALE_8X8);
  }
}

uint16_t uiAccentForVl53l0x(uint16_t mm, bool valid) {
  if (!valid) return SKYBLUE;
  if (mm < 200) return ORANGE;
  if (mm < 600) return GREENYELLOW;
  return CYAN;
}

#endif
