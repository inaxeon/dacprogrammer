#ifndef __PROJECT_H__
#define __PROJECT_H__

#define _USART1_
#define _12_288_CLK_

#if defined(__18F26K22) || defined(__18F26K42)

#define __PIC18__
#define __PIC18_K__
#define _4X_PLL_
#define _HELP_

#endif

#ifdef _12_288_CLK_
#ifdef _4X_PLL_
#define _XTAL_FREQ      49152000
#else
#define _XTAL_FREQ      12288000
#endif
#endif

#include <xc.h>
#include <stdint.h>

#define CONFIG_MAGIC        0x4646

#define _I2C_XFER_
#define _I2C_XFER_BYTE_
#define _I2C_XFER_X16_

#define UART_BAUD            9600

#define MAX_DESC                16

#endif /* __PROJECT_H__ */