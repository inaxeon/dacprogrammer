/* 
 * File:   config.h
 * Author: Matt
 *
 * Created on 25 November 2014, 15:54
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t magic;
    uint8_t i2c_addr;
} sys_config_t;

void cmd_prompt(sys_config_t *config);
void load_configuration(sys_config_t *config);

#endif /* __CONFIG_H__ */