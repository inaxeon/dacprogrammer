/*
 * File:   util.c
 * Author: Matt
 *
 * Created on 09 August 2015, 1429
 */

#include "project.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <xc.h>

#include "util.h"
#include "usart.h"
#include "cmd.h"

#ifdef __PIC16__
#include "usart.h"
#endif

void reset(void)
{
    asm("reset");
    while (1);
}

void putch(char byte)
{
    while (usart1_busy());
    usart1_put(byte);
}

void clear_usart_oerr(void)
{
    if (RCSTAbits.OERR)
    {
        /* Hack to clear overrun errors */
        RCSTAbits.CREN = 0;
        RCSTAbits.CREN = 1;
    }
}

void delay_10ms(uint8_t delay)
{
    uint8_t i;
    for (i = 0; i < delay; i++)
    {
        CLRWDT();
        __delay_ms(10);
    }
}

char wdt_getch(void)
{
    clear_usart_oerr();

    while (!usart1_data_ready())
        CLRWDT();

    return usart1_get();
}

void eeprom_write_data(uint8_t addr, uint8_t *bytes, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < len; i++)
    {
        while (EECON1bits.WR);

        EECON1bits.EEPGD = 0;
#ifdef __PIC18__
        EECON1bits.CFGS = 0;
#endif
        INTCONbits.GIE = 0;

        EEADR = addr + i;
        EEDATA = *bytes;

        EECON1bits.WREN = 1;
        EECON1bits.EEPGD = 0;

        EECON2 = 0x55;
        EECON2 = 0xAA;

        EECON1bits.WR = 1;

        INTCONbits.GIE = 1;

        bytes++;
    }
}

void eeprom_read_data(uint8_t addr, uint8_t *bytes, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < len; i++)
    {
        EEADR = addr + i;
        EECON1bits.EEPGD = 0;
#ifdef __PIC18__
        EECON1bits.CFGS = 0;
#endif
        EECON1bits.RD = 1;
        *bytes = EEDATA;
        bytes++;
    }
}