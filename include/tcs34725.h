#ifndef __TCS34725_H
#define __TCS34725_H

#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCS34725_I2C_ADDR           0x29

#define TCS34725_COMMAND_BIT        0x80
#define TCS34725_ENABLE             0x00
#define TCS34725_ATIME              0x01
#define TCS34725_WTIME              0x03
#define TCS34725_AILTL              0x04
#define TCS34725_AILTH              0x05
#define TCS34725_AIHTL              0x06
#define TCS34725_AIHTH              0x07
#define TCS34725_PERS               0x0C
#define TCS34725_CONFIG             0x0D
#define TCS34725_CONTROL            0x0F
#define TCS34725_ID                 0x12
#define TCS34725_STATUS             0x13
#define TCS34725_CDATAL             0x14
#define TCS34725_CDATAH             0x15
#define TCS34725_RDATAL             0x16
#define TCS34725_RDATAH             0x17
#define TCS34725_GDATAL             0x18
#define TCS34725_GDATAH             0x19
#define TCS34725_BDATAL             0x1A
#define TCS34725_BDATAH             0x1B

#define TCS34725_ENABLE_PON         0x01
#define TCS34725_ENABLE_AEN         0x02

void tcs3272_init(I2C_TypeDef *I2Cx);
uint8_t tcs34725_read_id(I2C_TypeDef *I2Cx);
void getRGB(I2C_TypeDef *I2Cx, int *r, int *g, int *b, uint16_t *c);

#ifdef __cplusplus
}
#endif

#endif