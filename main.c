/* 
 * File:   main.c
 * Author: Matt
 *
 * Created on 26 August 2014, 20:27
 */

#include "project.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "util.h"
#include "usart.h"
#include "i2c.h"

#ifdef __18F26K22
#ifdef _4X_PLL_
#pragma config PLLCFG = ON
#else
#pragma config PLLCFG = OFF
#endif
#pragma config PRICLKEN = ON
#pragma config FCMEN = OFF
#pragma config IESO = OFF
#if _XTAL_FREQ > 16000000
#pragma config FOSC = HSHP
#else
#pragma config FOSC = HSMP
#endif
#pragma config BOREN = OFF
#pragma config PWRTEN = OFF
#pragma config WDTPS = 16384
#pragma config WDTEN = ON
#pragma config CCP2MX = PORTC1
#pragma config CCP3MX = PORTB5
#pragma config T3CMX = PORTC0
#pragma config HFOFST = ON
#pragma config PBADEN = OFF
#pragma config MCLRE = EXTMCLR
#pragma config P2BMX = PORTB5
#pragma config STVREN = ON
#pragma config DEBUG = OFF
#pragma config LVP = OFF
#pragma config XINST = OFF // COMPLETELY FUCKS EXECUTION. DO NOT ENABLE!
#pragma config CP0 = OFF
#pragma config CP1 = OFF
#pragma config CP2 = OFF
#pragma config CP3 = OFF
#pragma config CPB = OFF
#pragma config CPD = OFF
#pragma config WRT0 = OFF
#pragma config WRT1 = OFF
#pragma config WRT2 = OFF
#pragma config WRT3 = OFF
#pragma config WRTB = OFF
#pragma config WRTC = OFF
#pragma config WRTD = OFF
#pragma config EBTR0 = OFF
#pragma config EBTR1 = OFF
#pragma config EBTR2 = OFF
#pragma config EBTR3 = OFF
#pragma config EBTRB = OFF
#endif

sys_config_t _g_cfg;

int main(void)
{
    sys_config_t *config = &_g_cfg;

    /* Disable Interrupts */
    INTCONbits.GIE_GIEH = 0;
    INTCONbits.PEIE_GIEL = 0;

    INTCON2bits.RBIP = 0;
    RCONbits.IPEN = 1;

    /* A/D pins all digital */
#ifdef __PIC18_K__
    ANSELA = 0x00;
    ANSELB = 0x00;
    ANSELC = 0x00;
#else
    ADCON1bits.PCFG0 = 1;
    ADCON1bits.PCFG1 = 1;
    ADCON1bits.PCFG2 = 1;
    ADCON1bits.PCFG3 = 1;
#endif /* __PIC18_K__ */

#ifdef __PIC18_K__
    IOCBbits.IOCB4 = 1;
    IOCBbits.IOCB5 = 1;
#endif
    
    // P-CH FETs
    TRISAbits.TRISA3 = 0;
    TRISAbits.TRISA5 = 0;
    PORTAbits.RA3 = 0;
    PORTAbits.RA5 = 0;

#ifdef _4X_PLL_
    usart1_open(USART_CONT_RX, (((_XTAL_FREQ / UART_BAUD) / 64) - 1));
#else
    usart1_open(USART_CONT_RX | USART_BRGH, (((_XTAL_FREQ / UART_BAUD) / 16) - 1));
#endif
    
    i2c_init(100);
    load_configuration(config);

    for (;;)
    {
        cmd_prompt(config);
    }
}
