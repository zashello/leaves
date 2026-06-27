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
  int rstPin;
  uint8_t address;
  uint8_t columnOffset;
  uint8_t pageOffset;
} ssd1309_config_t;

#define SSD1309_BLACK 0
#define SSD1309_WHITE 1

esp_err_t ssd1309Init(const ssd1309_config_t *config);
void ssd1309Clear(void);
void ssd1309Display(void);
void ssd1309DrawPixel(int16_t x, int16_t y, uint8_t color);
void ssd1309SetCursor(int16_t x, int16_t y);
void ssd1309Write(char c);
void ssd1309Print(const char *text);
void ssd1309PrintFloat(float value, uint8_t decimals);
void ssd1309Deinit(void);

#ifdef __cplusplus
}
#endif
