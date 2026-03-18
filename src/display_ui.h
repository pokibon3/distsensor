#pragma once

#include "Arduino.h"

void uiDrawBooting();
void uiDrawSensorError();
void uiStageHalt(const char *msg, uint16_t color);

#if defined(SENSOR_VL53L1X)
void uiDrawStaticVl53l1x(uint8_t roi_size, uint8_t grid, uint8_t cell, uint8_t origin_x, uint8_t origin_y);
void uiDrawHeatCellVl53l1x(uint8_t x, uint8_t y, uint8_t grid, uint8_t cell, uint8_t origin_x, uint8_t origin_y,
                           const uint8_t *heat, bool highlight);
void uiDrawStatsVl53l1x(uint16_t cycle_min_mm, bool cycle_has_min, uint16_t current_mm, bool current_has_mm, bool timeout,
                        bool measurable);
#elif defined(SENSOR_VL53L0X)
void uiDrawFrameVl53l0x(uint16_t accent);
void uiDrawGaugeVl53l0x(uint16_t mm, bool valid, uint16_t accent);
void uiDrawDistanceVl53l0x(uint16_t mm, bool valid, bool timeout);
uint16_t uiAccentForVl53l0x(uint16_t mm, bool valid);
#endif
