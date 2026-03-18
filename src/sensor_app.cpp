#include "sensor_app.h"
#include "Arduino.h"
#include "Wire.h"
#include "display_ui.h"
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
constexpr uint8_t kSw1Pin = PA1;
constexpr uint8_t kHeatmapPixels = 64;
constexpr uint8_t kGridX = 2;
constexpr uint8_t kGridY = 8;

uint8_t g_roi_mode_index = kDefaultRoiIndex;
bool g_sw1_prev_level = true;
uint32_t g_sw1_last_change_ms = 0;
uint8_t g_heat[16 * 16] = {};
uint16_t g_scan_index = 0;
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

bool initSensor() {
  g_sensor.setTimeout(500);
  if (!g_sensor.init()) return false;
  g_sensor.setDistanceMode(VL53L1X::Short);
  g_sensor.setMeasurementTimingBudget(20000);
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
  uiDrawStaticVl53l1x(currentRoiSize(), currentGrid(), currentCell(), kGridX, kGridY);
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

void warmup() {
  for (uint8_t i = 0; i < 4; ++i) {
    const uint8_t boot_spad = (currentRoiSize() >= 16 || currentRoiSize() <= 1) ? 199 : spadForXY(0, 0);
    g_sensor.setROICenter(boot_spad);
    (void)g_sensor.readSingle(true);
    if (!g_sensor.timeoutOccurred()) break;
    Delay_Ms(20);
  }
}

void loopStepVl53l1x() {
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
  } else {
    g_current_has_mm = false;
  }

  g_heat[idx] = measurable ? intensityFromDistance(mm, measurable, valid) : 0;
  if (g_prev_hx >= 0 && g_prev_hy >= 0 && (g_prev_hx != x || g_prev_hy != y)) {
    uiDrawHeatCellVl53l1x((uint8_t)g_prev_hx, (uint8_t)g_prev_hy, grid, currentCell(), kGridX, kGridY, g_heat, false);
  }
  uiDrawHeatCellVl53l1x(x, y, grid, currentCell(), kGridX, kGridY, g_heat, true);
  g_prev_hx = x;
  g_prev_hy = y;

  g_scan_index++;
  if (g_scan_index >= currentPoints()) g_scan_index = 0;
  uiDrawStatsVl53l1x(g_cycle_min_mm, g_cycle_has_min, g_current_mm, g_current_has_mm, timeout, measurable);
}

#else

VL53L0X g_sensor;
uint16_t g_smoothed = 0;
bool g_has_reading = false;

bool initSensor() {
  g_sensor.setTimeout(80);
  if (!g_sensor.init()) return false;
  g_sensor.setMeasurementTimingBudget(33000);
  return true;
}

void loopStepVl53l0x() {
  const uint16_t raw = g_sensor.readRangeContinuousMillimeters();
  const bool timeout = g_sensor.timeoutOccurred();
  const bool valid = !timeout && raw > 0 && raw < 2000;

  if (valid) {
    if (!g_has_reading) g_smoothed = raw;
    else g_smoothed = (uint16_t)((g_smoothed * 3U + raw) / 4U);
    g_has_reading = true;
  }

  const bool display_valid = g_has_reading && valid;
  const uint16_t accent = uiAccentForVl53l0x(g_smoothed, display_valid);
  uiDrawFrameVl53l0x(accent);
  uiDrawGaugeVl53l0x(g_smoothed, display_valid, accent);
  uiDrawDistanceVl53l0x(g_smoothed, display_valid, timeout);
  Delay_Ms(50);
}

#endif

void initBusAndSensorObject() {
  Wire.begin();
  Wire.setClock(400000);
  new (&g_sensor) decltype(g_sensor)();
  g_sensor.setBus(&Wire);
}

}  // namespace

void appInit() {
  st7735_init();

#if LCD_BRINGUP_STAGE == 1
  uiStageHalt("STAGE1: LCD INIT", GREEN);
#endif

#if LCD_BRINGUP_STAGE == 2
  uiStageHalt("STAGE2: PRE I2C", CYAN);
#endif

  initBusAndSensorObject();

#if LCD_BRINGUP_STAGE == 3
  uiStageHalt("STAGE3: I2C INIT", SKYBLUE);
#endif

  uiDrawBooting();
  Delay_Ms(80);

  bool sensor_ok = initSensor();
  if (!sensor_ok) {
    Delay_Ms(20);
    sensor_ok = initSensor();
  }

  if (!sensor_ok) {
    uiDrawSensorError();
    while (1) Delay_Ms(100);
  }

#if LCD_BRINGUP_STAGE == 4
  uiStageHalt("STAGE4: SENSOR OK", GREENYELLOW);
#endif

#if defined(SENSOR_VL53L1X)
  initSw1();
  applyModeConfigAndResetUi();
  warmup();
#else
  g_sensor.startContinuous(60);
#endif
}

void appLoopStep() {
#if defined(SENSOR_VL53L1X)
  loopStepVl53l1x();
#else
  loopStepVl53l0x();
#endif
}
