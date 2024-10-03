/*
* File:   i2c.c
* Author: Matt
*
* Created on 12 August 2015, 12:02
*/

#include <stdint.h>
#include <stdbool.h>

#include "i2c.h"

#ifdef _I2C_XFER_MANY_TO_UART_
#include "util.h"
#endif /* _I2C_XFER_MANY_TO_UART_ */

#define I2C_NUMCLOCKS_TIMEOUT 100

//#define i2c_wait_for(x) while (x)

#define i2c_wait_for(x)                             \
    do {                                            \
        uint8_t waits = I2C_NUMCLOCKS_TIMEOUT;      \
        uint8_t cleared = 0;                        \
        do {                                        \
            uint8_t timeout = _g_waitPeriod;        \
            do {                                    \
                if (!(x)) {                         \
                    cleared = 1;                    \
                    break;                          \
                }                                   \
                __delay_us(1);                      \
            } while (--timeout);                    \
            if (cleared)                            \
                break;                              \
        } while (--waits);                          \
        if (!cleared)                               \
            goto fail;                              \
        } while (0);

#ifdef __PIC12__
#define i2c_wait_for_idle() i2c_wait_for((SSP1CON2 & 0x1F) | (SSP1STATbits.R_nW))
#else
#define i2c_wait_for_idle() i2c_wait_for((SSPCON2 & 0x1F) | (SSPSTATbits.R_W))
#endif

#define i2c_put_start_and_wait() { SSPCON2bits.SEN = 1; i2c_wait_for(SSPCON2bits.SEN); }
#define i2c_put_stop_and_wait() { SSPCON2bits.PEN = 1; i2c_wait_for(SSPCON2bits.PEN); }
#define i2c_ack_was_received() (!SSPCON2bits.ACKSTAT)

#if defined (_I2C_XFER_) || defined(_I2C_XFER_BYTE_) || defined(_I2C_XFER_MANY_) \
  || defined(_I2C_XFER_X16_) || defined(_I2C_DS2482_SPECIAL_)

static uint8_t _g_waitPeriod;

void i2c_init(uint16_t freq_khz)
{
    if (freq_khz < I2C_MIN_FREQ)
        return;

#if defined(__PIC16__) || defined(__PIC12__)
    SSPCON = 0x28;                   /* I2C enabled, Master mode */
    SSPCON2 = 0x00;
#else
    SSPCON1 = 0x28;                  /* I2C enabled, Master mode */
    SSPCON2 = 0x00;
#endif
    
    SSPADD = (((_XTAL_FREQ / (freq_khz * 1000)) / 4) - 1);
    SSPSTAT = 0b11000000;            /* Slew rate disabled */

    _g_waitPeriod = (uint8_t)(1000 / freq_khz);
}

#ifdef _I2C_BRUTEFORCE_RESET_

void i2c_bruteforce_reset(void)
{
    uint8_t i;
    
#if defined(__PIC16__) || defined(__PIC12__)
    SSPCON = 0x08;                   /* I2C disabled, Master mode */
#else
    SSPCON1 = 0x08;                  /* I2C disabled, Master mode */
#endif
    
#if defined(_18F2550)
    
#define BB_SCL  PORTBbits.RB1
#define BB_SDA  PORTBbits.RB0

#define BT_SCL  TRISBbits.TRISB1
#define BT_SDA  TRISBbits.TRISB0

#else
#error Unknown device
#endif
    
    // Set both pins to output
    BT_SCL = 0;
    BT_SDA = 0;
    
    // Start condition
    BB_SDA = 0;
    __delay_us(20);
    BB_SCL = 0;
    __delay_us(20);
    BB_SDA = 1;
    __delay_us(20);
    BB_SCL = 1;
    
    // 16 clocks
    for (i = 0; i < 16; i++)
    {
        __delay_us(20);
        BB_SCL = 0;
        __delay_us(20);
        BB_SCL = 1;
    }
    
    // Start of stop condition
    __delay_us(20);
    BB_SCL = 0;
    delay_10ms(10);
    BB_SDA = 0;
    __delay_us(20);
    BB_SCL = 1;
    __delay_us(20);
    // End of stop
    BB_SDA = 1;
    
    // Both pins back to input
    BT_SDA = 1;
    BT_SCL = 1;
    
    __delay_ms(1);
}

#endif /* _I2C_BRUTEFORCE_RESET_ */

static bool i2c_byte_out(uint8_t data_out)
{
    SSPBUF = data_out;
#ifdef __PIC16__
    if (SSPCONbits.WCOL)
    {
        SSPCONbits.WCOL = 0;
#else
    if (SSPCON1bits.WCOL)
    {
        SSPCON1bits.WCOL = 0;
#endif
        goto fail;
    }

    i2c_wait_for(SSPSTATbits.BF);    /* Wait until write cycle is complete */

    i2c_wait_for_idle();
    return true;
    
fail:
    return false;
}

static bool i2c_byte_in(bool ack, uint8_t *data)
{
    SSPCON2bits.RCEN = 1;            /* Enable master for 1 byte reception */

    i2c_wait_for(!SSPSTATbits.BF);
    i2c_wait_for(SSPCON2bits.RCEN);  /* Check that receive sequence is over */

    SSPCON2bits.ACKDT = ack ? 0 : 1; /* Send ACK when 0 and NACK when 1 */
    SSPCON2bits.ACKEN = 1;

    i2c_wait_for(SSPCON2bits.ACKEN); /* Wait till finished */

    *data = (SSPBUF);                 /* Return with read byte */
    return true;
    
fail:
    *data = (SSPBUF);                 /* Read SSPBUF anyway, so that BF is cleared */
    return false;
}

#ifdef _I2C_XFER_

bool i2c_write(uint8_t addr, uint8_t reg, uint8_t data)
{
    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(reg);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(data);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *ret)
{
    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(reg);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_put_start_and_wait();
    i2c_byte_out((addr << 1) | 0x01);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    if (!i2c_byte_in(false, ret))
        goto fail;
    
    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

#endif /* _I2C_XFER_ */

#ifdef _I2C_XFER_BYTE_

bool i2c_write_byte(uint8_t addr, uint8_t data)
{
    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(data);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

bool i2c_read_byte(uint8_t addr, uint8_t *ret)
{
    i2c_wait_for_idle();
    i2c_put_start_and_wait();

    i2c_byte_out((addr << 1) | 0x01);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    if (!i2c_byte_in(false, ret))
        goto fail;
    
    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

#endif /* _I2C_XFER_BYTE_ */

#ifdef _I2C_XFER_MANY_

bool i2c_write_buf(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;

    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(reg);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    for (i = 0; i < len; i++)
    {
        i2c_byte_out(*(data + i));

        if (!i2c_ack_was_received())
        {
            i2c_put_stop_and_wait(); /* Reset I2C bus */
            goto fail;               /* Error */
        }
    }

    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

bool i2c_read_buf(uint8_t addr, uint8_t offset, uint8_t *ret, uint8_t len)
{
    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(offset);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_put_start_and_wait();
    i2c_byte_out((addr << 1) | 0x01);

    if (!i2c_ack_was_received())
    {
        i2c_put_start_and_wait(); /* Reset I2C bus */
        goto fail;                /* Error */
    }

    while (len > 1)
    {
        if (!i2c_byte_in(true, ret))
            goto fail;
        
        ret++;
        len--;
    }

    if (!i2c_byte_in(false, ret))
        goto fail;
    
    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

#endif /* _I2C_XFER_MANY_ */

#ifdef _I2C_XFER_MANY_TO_UART_

bool i2c_read_to_uart(uint8_t addr, uint8_t offset, uint8_t len)
{
    uint8_t ret;
    
    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(offset);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_put_start_and_wait();
    i2c_byte_out((addr << 1) | 0x01);

    if (!i2c_ack_was_received())
    {
        i2c_put_start_and_wait(); /* Reset I2C bus */
        goto fail;                /* Error */
    }

    while (len > 1)
    {
        if (!i2c_byte_in(true, &ret))
            goto fail;
        
        uart_put(ret);
        len--;
    }

    if (!i2c_byte_in(false, &ret))
        goto fail;
    
    uart_put(ret);
    
    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

#endif /* _I2C_XFER_MANY_TO_UART_ */

#ifdef _I2C_XFER_X16_

bool i2c_write16(uint8_t addr, uint8_t reg, uint16_t data)
{
    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(reg);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(*((uint8_t *)&data + 1));

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(*((uint8_t *)&data));

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

bool i2c_read16(uint8_t addr, uint8_t offset, uint16_t *ret)
{
    i2c_wait_for_idle();
    i2c_put_start_and_wait();
    i2c_byte_out(addr << 1);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_byte_out(offset);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    i2c_put_start_and_wait();
    i2c_byte_out((addr << 1) | 0x01);

    if (!i2c_ack_was_received())
    {
        i2c_put_start_and_wait(); /* Reset I2C bus */
        goto fail;                /* Error */
    }
    if (!i2c_byte_in(true, ((uint8_t *)ret + 1)))
        goto fail;
    
    if (!i2c_byte_in(false, ((uint8_t *)ret)))
        goto fail;

    i2c_put_stop_and_wait();
    return true;
    
fail:
    return false;
}

#endif /* _I2C_XFER_X16_ */

#ifdef _I2C_DS2482_SPECIAL_

bool i2c_await_flag(uint8_t addr, uint8_t mask, uint8_t *ret, uint8_t attempts)
{
    uint8_t status;

    i2c_wait_for_idle();
    i2c_put_start_and_wait();

    i2c_byte_out((addr << 1) | 0x01);

    if (!i2c_ack_was_received())
    {
        i2c_put_stop_and_wait(); /* Reset I2C bus */
        goto fail;               /* Error */
    }

    while (attempts--)
    {
        if (!i2c_byte_in(1, &status))
            goto fail;
            
        if (!(status & mask))
            break;
    }

    if (!i2c_byte_in(0, &status))
        goto fail;
    
    i2c_put_stop_and_wait();

    *ret = status;
    return !(status & mask);
    
fail:
    return false;
}

#endif /* _I2C_DS2482_SPECIAL_ */
#endif /* Entire Feature */