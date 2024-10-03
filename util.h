/* 
 * File:   util.h
 * Author: Matt
 *
 * Created on 09 August 2015, 14:30
 */

#ifndef __UTIL_H__
#define	__UTIL_H__

void delay_10ms(uint8_t delay);
void clear_usart_oerr(void);
void reset(void);
void format_fixedpoint(char *buf, int16_t value, uint8_t type);
void eeprom_read_data(uint8_t addr, uint8_t *bytes, uint8_t len);
void eeprom_write_data(uint8_t addr, uint8_t *bytes, uint8_t len);
char wdt_getch(void);

#define I_1DP               0
#define U_1DP               1

#define fixedpoint_sign(value, tag) \
    char tag##_sign[2]; \
    tag##_sign[1] = 0; \
    if (value < 0 ) \
        tag##_sign[0] = '-'; \
    else \
        tag##_sign[0] = 0; \

#define fixedpoint_arg(value, tag) tag##_sign, (abs(value) / _1DP_BASE), (abs(value) % _1DP_BASE)
#define fixedpoint_arg_u(value) (value / _1DP_BASE), (value % _1DP_BASE)
#define max_(x, y) (x > y ? x : y)
#define min_(x, y) (x < y ? x : y)

#define MAX_FDP 8 /* "-30.000" + NUL */

#endif	/* __UTIL_H__ */

