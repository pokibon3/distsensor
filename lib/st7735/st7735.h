#pragma once
#include <stdint.h>

// Display geometry (ST7735, landscape 160x80)
#define ST7735_WIDTH    160
#define ST7735_HEIGHT   80
#define ST7735_X_OFFSET 0
#define ST7735_Y_OFFSET 24

// Optional: pull CS low and define ST7735_NO_CS
// #define ST7735_NO_CS

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define BGR565(r, g, b) ((((b) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((r) >> 3))
#define RGB             RGB565

#define BLACK       RGB(0, 0, 0)
#define NAVY        RGB(0, 0, 123)
#define DARKGREEN   RGB(0, 125, 0)
#define DARKCYAN    RGB(0, 125, 123)
#define MAROON      RGB(123, 0, 0)
#define PURPLE      RGB(123, 0, 123)
#define OLIVE       RGB(123, 125, 0)
#define LIGHTGREY   RGB(198, 195, 198)
#define DARKGREY    RGB(123, 125, 123)
#define BLUE        RGB(0, 0, 255)
#define SKYBLUE     RGB(0, 204, 255)
#define DARKBLUE    RGB(0, 0, 192)
#define GREEN       RGB(0, 255, 0)
#define CYAN        RGB(0, 255, 255)
#define RED         RGB(255, 0, 0)
#define MAGENTA     RGB(255, 0, 255)
#define YELLOW      RGB(255, 255, 0)
#define WHITE       RGB(255, 255, 255)
#define ORANGE      RGB(255, 165, 0)
#define GREENYELLOW RGB(173, 255, 41)
#define PINK        RGB(255, 130, 198)

#define FONT_SCALE_8X8   1
#define FONT_SCALE_16X16 2

void st7735_init(void);
void st7735_set_cursor(uint16_t x, uint16_t y);
void st7735_set_color(uint16_t color);
void st7735_set_background_color(uint16_t color);
void st7735_print_char(char c, uint8_t scale);
void st7735_print(const char* str, uint8_t scale);
void st7735_print_number(int32_t num, uint16_t width);
void st7735_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void st7735_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void st7735_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void st7735_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void st7735_draw_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* bitmap);
void st7735_set_scroll_area(uint16_t top_fixed, uint16_t scroll_height, uint16_t bottom_fixed);
void st7735_set_scroll_start(uint16_t scroll_start);
