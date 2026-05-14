#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t width;
    uint8_t height;
    int sclPin;
    int sdaPin;
    uint8_t address;
    uint8_t columnOffset;
    uint8_t pageOffset;
} ssd1306_config_t;

#define SSD1306_BLACK 0
#define SSD1306_WHITE 1

esp_err_t ssd1306Init(const ssd1306_config_t *config);
void ssd1306Clear(void);
void ssd1306Display(void);
void ssd1306DrawPixel(int16_t x, int16_t y, uint8_t color);
void ssd1306SetCursor(int16_t x, int16_t y);
void ssd1306Write(char c);
void ssd1306Print(const char *text);
void ssd1306PrintFloat(float value, uint8_t decimals);
void ssd1306Deinit(void);

#ifdef __cplusplus
}
#endif
