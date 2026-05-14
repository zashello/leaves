#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2cSoftInit(int sclPin, int sdaPin);
void i2cSoftDeinit(void);
void i2cSoftStart(void);
void i2cSoftStop(void);
void i2cSoftSendByte(uint8_t byte);
bool i2cSoftWaitAck(void);
void i2cSoftReadByte(uint8_t *byte, bool ack);

#ifdef __cplusplus
}
#endif
