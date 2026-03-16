#include "st7735.h"
#include "ch32fun.h"
#include "fontk_8x8.h"
#include <stdbool.h>

extern "C" int mini_snprintf(char* buffer, unsigned int buffer_len, const char *fmt, ...);

// Pin mapping (CH32V003 -> ST7735)
#define PIN_RESET 7  // PC7
#define PIN_DC    0  // PD0
#ifndef ST7735_NO_CS
#define PIN_CS    3  // PC3
#endif
#define SPI_SCLK  5  // PC5
#define SPI_MOSI  6  // PC6

#define DATA_MODE()    (GPIOD->BSHR |= 1 << PIN_DC)
#define COMMAND_MODE() (GPIOD->BCR |= 1 << PIN_DC)
#define RESET_HIGH()   (GPIOC->BSHR |= 1 << PIN_RESET)
#define RESET_LOW()    (GPIOC->BCR |= 1 << PIN_RESET)
#ifndef ST7735_NO_CS
#define START_WRITE()  (GPIOC->BCR |= 1 << PIN_CS)
#define END_WRITE()    (GPIOC->BSHR |= 1 << PIN_CS)
#else
#define START_WRITE()
#define END_WRITE()
#endif

// Delays (ms)
#define ST7735_RST_DELAY     120
#define ST7735_SLPOUT_DELAY  120
#define ST7735_CMD_DELAY     10

// Commands
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_VSCRDEF 0x33
#define ST7735_VSCRSADD 0x37

// MADCTL bits
#define ST7735_MADCTL_RGB 0x00
#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MV  0x20
#define ST7735_MADCTL_MX  0x40
#define ST7735_MADCTL_MY  0x80

#define ST7735_COLMOD_16_BPP 0x05

#define FONT_WIDTH  5
#define FONT_HEIGHT 7

static uint16_t cursor_x = 0;
static uint16_t cursor_y = 0;
static uint16_t fg_color = WHITE;
static uint16_t bg_color = BLACK;
static uint32_t dma_base_cfgr = 0;

static void dma_init(void)
{
    RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;
    DMA1_Channel3->PADDR = (uint32_t)&SPI1->DATAR;

    dma_base_cfgr = DMA_DIR_PeripheralDST
                    | DMA_Mode_Normal
                    | DMA_PeripheralInc_Disable
                    | DMA_PeripheralDataSize_Byte
                    | DMA_MemoryDataSize_Byte
                    | DMA_Priority_VeryHigh
                    | DMA_M2M_Disable;
}

static void spi_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_SPI1;

    GPIOC->CFGLR &= ~(0xf << (PIN_RESET << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_RESET << 2);

    GPIOD->CFGLR &= ~(0xf << (PIN_DC << 2));
    GPIOD->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_DC << 2);

#ifndef ST7735_NO_CS
    GPIOC->CFGLR &= ~(0xf << (PIN_CS << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_CS << 2);
#endif

    GPIOC->CFGLR &= ~(0xf << (SPI_SCLK << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_SCLK << 2);

    GPIOC->CFGLR &= ~(0xf << (SPI_MOSI << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_MOSI << 2);

    SPI1->CTLR1 = SPI_CPHA_1Edge
                  | SPI_CPOL_Low
                  | SPI_Mode_Master
                  | SPI_BaudRatePrescaler_2
                  | SPI_FirstBit_MSB
                  | SPI_NSS_Soft
                  | SPI_DataSize_8b
                  | SPI_Direction_1Line_Tx;
    SPI1->CTLR1 |= CTLR1_SPE_Set;
    SPI1->CTLR2 |= SPI_I2S_DMAReq_Tx;

    dma_init();
}

static inline void spi_write(uint8_t data)
{
    SPI1->DATAR = data;
    while (!(SPI1->STATR & SPI_STATR_TXE))
        ;
    while (SPI1->STATR & SPI_STATR_BSY)
        ;
}

static void spi_write_dma(const uint8_t* data, uint32_t length, bool mem_inc)
{
    while (length > 0)
    {
        uint16_t chunk = (length > 0xFFFFu) ? 0xFFFFu : (uint16_t)length;
        uint32_t cfgr = dma_base_cfgr | (mem_inc ? DMA_MemoryInc_Enable : DMA_MemoryInc_Disable);

        DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
        DMA1_Channel3->MADDR = (uint32_t)data;
        DMA1_Channel3->CNTR = chunk;
        DMA1->INTFCR = DMA1_FLAG_TC3;
        DMA1_Channel3->CFGR = cfgr | DMA_CFGR1_EN;

        while (!(DMA1->INTFR & DMA1_FLAG_TC3))
            ;

        DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
        while (SPI1->STATR & SPI_STATR_BSY)
            ;

        if (mem_inc)
            data += chunk;
        length -= chunk;
    }
}

static inline void tft_write_cmd(uint8_t cmd)
{
    COMMAND_MODE();
    spi_write(cmd);
}

static inline void tft_write_data(uint8_t data)
{
    DATA_MODE();
    spi_write(data);
}

static inline void tft_write_data16(uint16_t data)
{
    DATA_MODE();
    spi_write((uint8_t)(data >> 8));
    spi_write((uint8_t)data);
}

static inline uint16_t apply_x(uint16_t x)
{
    return (uint16_t)(x + ST7735_X_OFFSET);
}

static inline uint16_t apply_y(uint16_t y)
{
    return (uint16_t)(y + ST7735_Y_OFFSET);
}

static void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    tft_write_cmd(ST7735_CASET);
    tft_write_data16(x0);
    tft_write_data16(x1);
    tft_write_cmd(ST7735_RASET);
    tft_write_data16(y0);
    tft_write_data16(y1);
    tft_write_cmd(ST7735_RAMWR);
}

void st7735_init(void)
{
    spi_init();

    RESET_HIGH();
    Delay_Ms(1);
    RESET_LOW();
    Delay_Ms(1);
    RESET_HIGH();
    Delay_Ms(ST7735_RST_DELAY);

    START_WRITE();

    tft_write_cmd(ST7735_SWRESET);
    Delay_Ms(ST7735_CMD_DELAY);

    tft_write_cmd(ST7735_SLPOUT);
    Delay_Ms(ST7735_SLPOUT_DELAY);

    tft_write_cmd(ST7735_COLMOD);
    tft_write_data(ST7735_COLMOD_16_BPP);
    Delay_Ms(ST7735_CMD_DELAY);

    tft_write_cmd(ST7735_MADCTL);
    tft_write_data((uint8_t)(ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_BGR));
    Delay_Ms(ST7735_CMD_DELAY);

    tft_write_cmd(ST7735_INVOFF);
    Delay_Ms(ST7735_CMD_DELAY);

    tft_write_cmd(ST7735_DISPON);
    Delay_Ms(ST7735_CMD_DELAY);

    END_WRITE();
}

void st7735_set_cursor(uint16_t x, uint16_t y)
{
    cursor_x = x;
    cursor_y = y;
}

void st7735_set_color(uint16_t color)
{
    fg_color = color;
}

void st7735_set_background_color(uint16_t color)
{
    bg_color = color;
}

void st7735_print_char(char c, uint8_t scale)
{
    if (scale < 1) scale = 1;
    if (scale > 2) scale = 2;

    const uint8_t* glyph = &font[((uint8_t)c) << 3];
    const uint16_t w = (uint16_t)(FONT_WIDTH * scale);
    const uint16_t h = (uint16_t)(FONT_HEIGHT * scale);

    const uint16_t x0 = apply_x(cursor_x);
    const uint16_t y0 = apply_y(cursor_y);

    START_WRITE();
    tft_set_window(x0, y0, (uint16_t)(x0 + w - 1), (uint16_t)(y0 + h - 1));

    for (uint8_t row = 0; row < FONT_HEIGHT; row++)
    {
        for (uint8_t sy = 0; sy < scale; sy++)
        {
            for (uint8_t col = 0; col < FONT_WIDTH; col++)
            {
                const uint8_t bits = glyph[col];
                const uint16_t color = (bits & (uint8_t)(1U << row)) ? fg_color : bg_color;
                for (uint8_t sx = 0; sx < scale; sx++)
                {
                    tft_write_data16(color);
                }
            }
        }
    }

    END_WRITE();
}

void st7735_print(const char* str, uint8_t scale)
{
    if (!str)
        return;

    while (*str)
    {
        st7735_print_char(*str++, scale);
        cursor_x = (uint16_t)(cursor_x + ((FONT_WIDTH + 1) * scale));
    }
}

void st7735_print_number(int32_t num, uint16_t width)
{
    char buf[16];
    mini_snprintf(buf, sizeof(buf), "%*ld", (int)width, (long)num);
    st7735_print(buf, 1);
}

void st7735_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    const uint16_t xx = apply_x(x);
    const uint16_t yy = apply_y(y);

    START_WRITE();
    tft_set_window(xx, yy, xx, yy);
    tft_write_data16(color);
    END_WRITE();
}

void st7735_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (width == 0 || height == 0)
        return;

    const uint16_t x0 = apply_x(x);
    const uint16_t y0 = apply_y(y);
    const uint16_t x1 = (uint16_t)(x0 + width - 1);
    const uint16_t y1 = (uint16_t)(y0 + height - 1);

    START_WRITE();
    tft_set_window(x0, y0, x1, y1);
    DATA_MODE();

    static uint8_t linebuf[128];
    for (uint16_t i = 0; i < sizeof(linebuf); i += 2)
    {
        linebuf[i] = (uint8_t)(color >> 8);
        linebuf[i + 1] = (uint8_t)color;
    }

    uint32_t remaining = (uint32_t)width * (uint32_t)height * 2U;
    while (remaining > 0)
    {
        uint32_t chunk = (remaining > sizeof(linebuf)) ? sizeof(linebuf) : remaining;
        spi_write_dma(linebuf, chunk, false);
        remaining -= chunk;
    }

    END_WRITE();
}

void st7735_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (width == 0 || height == 0)
        return;

    st7735_draw_line((int16_t)x, (int16_t)y, (int16_t)(x + width - 1), (int16_t)y, color);
    st7735_draw_line((int16_t)x, (int16_t)(y + height - 1), (int16_t)(x + width - 1), (int16_t)(y + height - 1), color);
    st7735_draw_line((int16_t)x, (int16_t)y, (int16_t)x, (int16_t)(y + height - 1), color);
    st7735_draw_line((int16_t)(x + width - 1), (int16_t)y, (int16_t)(x + width - 1), (int16_t)(y + height - 1), color);
}

void st7735_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t dx = (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = (int16_t)((y1 > y0) ? (y0 - y1) : (y1 - y0));
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (int16_t)(dx + dy);

    while (1)
    {
        if (x0 >= 0 && y0 >= 0)
        {
            st7735_draw_pixel((uint16_t)x0, (uint16_t)y0, color);
        }
        if (x0 == x1 && y0 == y1)
            break;
        int16_t e2 = (int16_t)(2 * err);
        if (e2 >= dy)
        {
            err = (int16_t)(err + dy);
            x0 = (int16_t)(x0 + sx);
        }
        if (e2 <= dx)
        {
            err = (int16_t)(err + dx);
            y0 = (int16_t)(y0 + sy);
        }
    }
}

void st7735_draw_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* bitmap)
{
    if (!bitmap || width == 0 || height == 0)
        return;

    const uint16_t x0 = apply_x(x);
    const uint16_t y0 = apply_y(y);
    const uint16_t x1 = (uint16_t)(x0 + width - 1);
    const uint16_t y1 = (uint16_t)(y0 + height - 1);

    START_WRITE();
    tft_set_window(x0, y0, x1, y1);
    DATA_MODE();

    const uint32_t count = (uint32_t)width * (uint32_t)height * 2U;
    spi_write_dma(bitmap, count, true);

    END_WRITE();
}

void st7735_set_scroll_area(uint16_t top_fixed, uint16_t scroll_height, uint16_t bottom_fixed)
{
    START_WRITE();
    tft_write_cmd(ST7735_VSCRDEF);
    tft_write_data16(top_fixed);
    tft_write_data16(scroll_height);
    tft_write_data16(bottom_fixed);
    END_WRITE();
}

void st7735_set_scroll_start(uint16_t scroll_start)
{
    START_WRITE();
    tft_write_cmd(ST7735_VSCRSADD);
    tft_write_data16(scroll_start);
    END_WRITE();
}
