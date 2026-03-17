#include "Arduino.h"
#include "Wire.h"
#include "st7735.h"
#include <cstring>
#include <new>

#if defined(SENSOR_VL53L1X)
#include "VL53L1X.h"
#elif defined(SENSOR_VL53L0X)
#include "VL53L0X.h"
#else
#error "Define SENSOR_VL53L0X or SENSOR_VL53L1X in build flags"
#endif

#ifndef LCD_BRINGUP_STAGE
#define LCD_BRINGUP_STAGE 0
#endif

namespace {

#if defined(SENSOR_VL53L1X)
VL53L1X g_sensor;

#if defined(HEATMAP_GRID_8) && defined(HEATMAP_GRID_16)
#error "Choose only one of HEATMAP_GRID_8 or HEATMAP_GRID_16"
#endif

#if defined(HEATMAP_GRID_16)
constexpr uint8_t kDefaultRoiIndex = 0;
#else
constexpr uint8_t kDefaultRoiIndex = 1;
#endif
constexpr uint8_t kRoiModes[4] = {16, 8, 4, 1};
uint8_t g_roi_mode_index = kDefaultRoiIndex;
constexpr uint8_t kSw1Pin = PA1;
bool g_sw1_prev_level = true;
uint32_t g_sw1_last_change_ms = 0;

constexpr uint8_t kHeatmapPixels = 64;
constexpr uint8_t kGridX = 2;
constexpr uint8_t kGridY = 8;
uint8_t g_heat[16 * 16] = {};
uint16_t g_scan_index = 0;
uint16_t g_last_mm = 0;
bool g_has_last_mm = false;
uint16_t g_cycle_min_mm = 0;
bool g_cycle_has_min = false;
uint16_t g_current_mm = 0;
bool g_current_has_mm = false;
int16_t g_prev_hx = -1;
int16_t g_prev_hy = -1;

uint8_t currentRoiSize() {
  return kRoiModes[g_roi_mode_index];
}

uint8_t currentGrid() {
  return currentRoiSize();
}

uint8_t currentCell() {
  return (uint8_t)(kHeatmapPixels / currentGrid());
}

uint16_t currentPoints() {
  const uint8_t g = currentGrid();
  return (uint16_t)g * (uint16_t)g;
}

uint8_t spadForXY16(uint8_t x16, uint8_t y16) {
  if (y16 < 8) {
    return (uint8_t)(128 + x16 * 8 + y16);
  }
  return (uint8_t)(127 - x16 * 8 - (y16 - 8));
}

uint8_t roiCoordForGrid(uint8_t i) {
  const uint8_t grid = currentGrid();
  const uint8_t roi_margin = currentRoiSize() / 2;
  const uint8_t minc = roi_margin;
  const uint8_t maxc = (uint8_t)(15 - roi_margin);
  if (maxc < minc) return 7;
  if (grid <= 1) return 7;
  return (uint8_t)(minc + ((uint16_t)i * (maxc - minc) + (grid - 1) / 2) / (grid - 1));
}

uint8_t spadForXY(uint8_t x, uint8_t y) {
  const uint8_t x16 = roiCoordForGrid(x);
  const uint8_t y16 = roiCoordForGrid(y);
  return spadForXY16(x16, y16);
}

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

uint8_t intensityFromDistance(uint16_t mm, bool measurable, bool valid) {
  if (!measurable) return 0;
  if (!valid) return 96;
  if (mm <= 100) return 255;
  if (mm <= 200) return 230;
  if (mm <= 300) return 205;
  if (mm <= 450) return 182;
  if (mm <= 650) return 162;
  if (mm <= 900) return 145;
  if (mm <= 1200) return 130;
  if (mm <= 1550) return 118;
  if (mm <= 1950) return 108;
  if (mm <= 2350) return 100;
  if (mm <= 2700) return 94;
  if (mm <= 3000) return 90;
  return 88;
}

void applyCurrentRoiConfig() {
  const uint8_t roi = currentRoiSize();
  g_sensor.setROISize(roi, roi);
  if (roi >= 16 || roi <= 1) {
    g_sensor.setROICenter(199);
  }
}

void drawStaticUi() {
  const uint8_t grid = currentGrid();
  const uint8_t cell = currentCell();
  st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);

  for (uint8_t y = 0; y < grid; ++y) {
    for (uint8_t x = 0; x < grid; ++x) {
      st7735_fill_rect((uint16_t)(kGridX + x * cell), (uint16_t)(kGridY + y * cell), cell - 1, cell - 1, RGB(6, 8, 12));
    }
  }
  st7735_draw_rect(kGridX - 1, kGridY - 1, grid * cell + 2, grid * cell + 2, SKYBLUE);

  st7735_set_background_color(BLACK);
  st7735_set_color(WHITE);
  st7735_set_cursor(72, 8);
  st7735_print("VL53L1X", FONT_SCALE_8X8);
  st7735_set_cursor(72, 18);
  if (currentRoiSize() == 16) st7735_print("ROI 16x16", FONT_SCALE_8X8);
  else if (currentRoiSize() == 8) st7735_print("ROI 8x8 ", FONT_SCALE_8X8);
  else if (currentRoiSize() == 4) st7735_print("ROI 4x4 ", FONT_SCALE_8X8);
  else st7735_print("ROI 1x1 ", FONT_SCALE_8X8);
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

void drawHeatCell(uint8_t x, uint8_t y, bool highlight) {
  const uint8_t grid = currentGrid();
  const uint8_t cell = currentCell();
  const uint16_t idx = (uint16_t)y * grid + x;
  const uint16_t color = heatColor(g_heat[idx]);
  const uint16_t px = (uint16_t)(kGridX + x * cell);
  const uint16_t py = (uint16_t)(kGridY + y * cell);
  st7735_fill_rect(px, py, cell - 1, cell - 1, color);
  if (highlight && cell >= 2) {
    st7735_draw_rect(px, py, cell - 1, cell - 1, WHITE);
  }
}

void drawStats(bool timeout, bool measurable) {
  char buf[16];
  st7735_fill_rect(72, 30, 84, 18, BLACK);
  st7735_fill_rect(72, 50, 84, 10, BLACK);

  st7735_set_background_color(BLACK);

  if (g_cycle_has_min || g_has_last_mm) {
    const uint16_t shown_mm = g_cycle_has_min ? g_cycle_min_mm : g_last_mm;
    st7735_set_color(WHITE);
    st7735_set_cursor(72, 30);
    printMmLarge(shown_mm);
  } else {
    st7735_set_color(timeout ? ORANGE : DARKGREY);
    st7735_set_cursor(72, 30);
    st7735_print("----MM", FONT_SCALE_16X16);
  }

  st7735_set_cursor(72, 50);
  if (g_current_has_mm) {
    mini_snprintf(buf, sizeof(buf), "CUR:%4d", (int)g_current_mm);
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

bool initSensor() {
  g_sensor.setTimeout(300);
  if (!g_sensor.init()) return false;
  g_sensor.setDistanceMode(VL53L1X::Long);
  g_sensor.setMeasurementTimingBudget(33000);
  applyCurrentRoiConfig();
  return true;
}

void initSw1() {
  funPinMode(kSw1Pin, GPIO_CFGLR_IN_PUPD);
  funDigitalWrite(kSw1Pin, 1);
  g_sw1_prev_level = (funDigitalRead(kSw1Pin) != 0);
  g_sw1_last_change_ms = millis();
}

void applyModeConfigAndResetUi() {
  applyCurrentRoiConfig();
  g_scan_index = 0;
  g_prev_hx = -1;
  g_prev_hy = -1;
  g_cycle_has_min = false;
  g_current_has_mm = false;
  std::memset(g_heat, 0, sizeof(g_heat));
  drawStaticUi();
}

void handleRoiSwitch() {
  const bool level = (funDigitalRead(kSw1Pin) != 0);
  const uint32_t now = millis();
  if (level != g_sw1_prev_level && (now - g_sw1_last_change_ms) > 30) {
    g_sw1_last_change_ms = now;
    g_sw1_prev_level = level;
    if (!level) {
      g_roi_mode_index = (uint8_t)((g_roi_mode_index + 1) % 4);
      applyModeConfigAndResetUi();
    }
  }
}

#else

VL53L0X g_sensor;

bool initSensor() {
  g_sensor.setTimeout(80);
  if (!g_sensor.init()) return false;
  g_sensor.setMeasurementTimingBudget(33000);
  return true;
}

void drawUiFrame(uint16_t accent) {
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

void drawGauge(uint16_t mm, bool valid, uint16_t accent) {
  st7735_fill_rect(126, 16, 12, 48, RGB(22, 22, 28));
  if (!valid) return;
  const uint16_t capped = (mm > 1200) ? 1200 : mm;
  const uint16_t fill = (uint16_t)((capped * 46U) / 1200U);
  st7735_fill_rect(127, (uint16_t)(62 - fill), 10, fill, accent);
}

void drawDistance(uint16_t mm, bool valid, bool timeout) {
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

uint16_t accentFor(uint16_t mm, bool valid) {
  if (!valid) return SKYBLUE;
  if (mm < 200) return ORANGE;
  if (mm < 600) return GREENYELLOW;
  return CYAN;
}

#endif

}  // namespace

int main() {
  SystemInit();
  Delay_Ms(100);

  st7735_init();

  auto stageHalt = [](const char *msg, uint16_t color) {
    st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);
    st7735_set_background_color(BLACK);
    st7735_set_color(color);
    st7735_set_cursor(8, 30);
    st7735_print(msg, FONT_SCALE_8X8);
    while (1) Delay_Ms(100);
  };

#if LCD_BRINGUP_STAGE == 1
  stageHalt("STAGE1: LCD INIT", GREEN);
#endif

#if LCD_BRINGUP_STAGE == 2
  stageHalt("STAGE2: PRE I2C", CYAN);
#endif

  Wire.begin();
  Wire.setClock(100000);
#if defined(SENSOR_VL53L1X) || defined(SENSOR_VL53L0X)
  new (&g_sensor) decltype(g_sensor)();
  g_sensor.setBus(&Wire);
#endif

#if LCD_BRINGUP_STAGE == 3
  stageHalt("STAGE3: I2C INIT", SKYBLUE);
#endif

  st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);
  st7735_set_background_color(BLACK);
  st7735_set_color(SKYBLUE);
  st7735_set_cursor(20, 30);
  st7735_print("BOOTING...", FONT_SCALE_16X16);
  Delay_Ms(80);

  if (!initSensor()) {
    st7735_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, BLACK);
    st7735_set_color(ORANGE);
    st7735_set_cursor(8, 28);
    st7735_print("SENSOR ERROR", FONT_SCALE_16X16);
    st7735_set_cursor(8, 52);
    st7735_print("CHECK WIRING", FONT_SCALE_8X8);
    while (1) Delay_Ms(100);
  }

#if LCD_BRINGUP_STAGE == 4
  stageHalt("STAGE4: SENSOR OK", GREENYELLOW);
#endif

#if defined(SENSOR_VL53L1X)
  initSw1();
  applyModeConfigAndResetUi();

  // 起動直後のみ、単発計測を短くウォームアップして初回TIMEOUTを回避する。
  for (uint8_t i = 0; i < 4; ++i) {
    const uint8_t boot_spad = (currentRoiSize() >= 16 || currentRoiSize() <= 1)
                                  ? 199
                                  : spadForXY(0, 0);
    g_sensor.setROICenter(boot_spad);
    (void)g_sensor.readSingle(true);
    if (!g_sensor.timeoutOccurred()) break;
    Delay_Ms(20);
  }

  while (1) {
    handleRoiSwitch();
    const uint8_t grid = currentGrid();
    const uint8_t x = (uint8_t)(g_scan_index % grid);
    const uint8_t y = (uint8_t)(g_scan_index / grid);
    const uint8_t roi = currentRoiSize();
    const uint16_t idx = g_scan_index;
    if (idx == 0) {
      g_cycle_has_min = false;
    }
    const uint8_t spad = (roi >= 16 || roi <= 1) ? 199 : spadForXY(x, y);

    g_sensor.setROICenter(spad);
    const uint16_t mm = g_sensor.readSingle(true);
    const bool timeout = g_sensor.timeoutOccurred();
    const bool measurable = !timeout && mm > 0 && mm < 4000;
    const bool valid = measurable;
    if (measurable) {
      g_current_mm = mm;
      g_current_has_mm = true;
      if (!g_cycle_has_min || mm < g_cycle_min_mm) {
        g_cycle_min_mm = mm;
        g_cycle_has_min = true;
      }
      g_last_mm = g_cycle_min_mm;
      g_has_last_mm = true;
    } else {
      g_current_has_mm = false;
    }

    if (measurable) {
      g_heat[idx] = intensityFromDistance(mm, measurable, valid);
    } else {
      g_heat[idx] = 0;
    }
    if (g_prev_hx >= 0 && g_prev_hy >= 0 &&
        (g_prev_hx != x || g_prev_hy != y)) {
      drawHeatCell((uint8_t)g_prev_hx, (uint8_t)g_prev_hy, false);
    }
    drawHeatCell(x, y, true);
    g_prev_hx = x;
    g_prev_hy = y;

    g_scan_index++;
    if (g_scan_index >= currentPoints()) g_scan_index = 0;
    drawStats(timeout, measurable);
  }
#else
  g_sensor.startContinuous(60);
  uint16_t smoothed = 0;
  bool has_reading = false;

  while (1) {
    const uint16_t raw = g_sensor.readRangeContinuousMillimeters();
    const bool timeout = g_sensor.timeoutOccurred();
    const bool valid = !timeout && raw > 0 && raw < 2000;

    if (valid) {
      if (!has_reading) smoothed = raw;
      else smoothed = (uint16_t)((smoothed * 3U + raw) / 4U);
      has_reading = true;
    }

    const bool display_valid = has_reading && valid;
    const uint16_t accent = accentFor(smoothed, display_valid);
    drawUiFrame(accent);
    drawGauge(smoothed, display_valid, accent);
    drawDistance(smoothed, display_valid, timeout);
    Delay_Ms(50);
  }
#endif
}
