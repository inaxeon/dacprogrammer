/*
* File:   usart.c
* Author: Matt
*
* Created on 13 November 2017, 10:32
*/

#include "project.h"

#include <stdint.h>
#include <stdbool.h>

#include "usart.h"
#include "i2c.h"

#ifdef _USART1_

void usart1_open(uint8_t flags, uint8_t brg)
{
    if (flags & USART_SYNC)
        TXSTAbits.SYNC = 1;

    if (flags & USART_9BIT)
    {
        TXSTAbits.TX9 = 1;
        RCSTAbits.RX9 = 1;
    }

    if (flags & USART_SYNC_MASTER)
        TXSTAbits.CSRC = 1;

    if (flags & USART_CONT_RX)
        RCSTAbits.CREN = 1;
    else
        RCSTAbits.SREN = 1;

    if (flags & USART_BRGH)
        TXSTAbits.BRGH = 1;
    else
        TXSTAbits.BRGH = 0;

    if (flags & USART_IOR)
        PIE1bits.RCIE = 1;
    else
        PIE1bits.RCIE = 0;

    if (flags & USART_IOT)
        PIE1bits.TXIE = 1;
    else
        PIE1bits.TXIE = 0;

    SPBRG = brg;

    TXSTAbits.TXEN = 1;
    RCSTAbits.SPEN = 1;

#if defined(__18F2550) || defined(__18F26K22) || defined(__18F2520) || defined(__16F876A) || defined(__16F876)
    TRISCbits.TRISC6 = 0; // TX
    TRISCbits.TRISC7 = 1; // RX
    if (TXSTAbits.SYNC && !TXSTAbits.CSRC)	//Synchronous slave mode
        TRISCbits.TRISC6 = 1;
#else
#error Unknown device
#endif
}

bool usart1_busy(void)
{
    if (!TXSTAbits.TRMT)
        return true;
    return false;
}

void usart1_put(char c)
{
    TXREG = c;
}

bool usart1_data_ready(void)
{
    if (PIR1bits.RCIF)
        return true;
    return false;
}

char usart1_get(void)
{
    char data;
    data = RCREG;
    return data;
}

#endif /* _USART1_ */