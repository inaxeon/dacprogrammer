/*
 * File:   usart.h
 * Author: Matt
 *
 * Created on 13 November 2017, 10:32
 */

#ifndef __USART_H__
#define __USART_H__

#include "project.h"

#include <stdint.h>
#include <stdbool.h>

#define USART_SYNC         0x01
#define USART_9BIT         0x02
#define USART_SYNC_MASTER  0x04
#define USART_CONT_RX      0x08
#define USART_BRGH         0x10
#define USART_IOR          0x20
#define USART_IOT          0x40

#ifdef _USART1_

void usart1_open(uint8_t flags, uint8_t brg);
bool usart1_busy(void);
void usart1_put(char c);
bool usart1_data_ready(void);
char usart1_get(void);

#endif /* _USART1_ */

#endif /* __USART_H__ */
