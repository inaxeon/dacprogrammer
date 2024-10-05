/* 
 * File:   config.c
 * Author: Matt
 *
 * Created on 25 November 2014, 15:54
 */

#include "project.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "util.h"
#include "usart.h"
#include "i2c.h"
#include "mcp47febxx.h"

#define CMD_NONE              0x00
#define CMD_READLINE          0x01
#define CMD_COMPLETE          0x02
#define CMD_ESCAPE            0x03
#define CMD_AWAIT_NAV         0x04
#define CMD_PREVCOMMAND       0x05
#define CMD_NEXTCOMMAND       0x06
#define CMD_DEL               0x07
#define CMD_DROP_NAV          0x08
#define CMD_CANCEL            0x10

#define CTL_CANCEL            0x03
#define CTL_XOFF              0x13
#define CTL_U                 0x15

#define SEQ_ESCAPE_CHAR       0x1B
#define SEQ_CTRL_CHAR1        0x5B
#define SEQ_ARROW_UP          0x41
#define SEQ_ARROW_DOWN        0x42
#define SEQ_HOME              0x31
#define SEQ_INS               0x32
#define SEQ_DEL               0x33
#define SEQ_END               0x34
#define SEQ_PGUP              0x35
#define SEQ_PGDN              0x36
#define SEQ_NAV_END           0x7E

#define CMD_MAX_LINE          64
#define CMD_MAX_HISTORY       4

#define PARAM_U16             0
#define PARAM_U8              1
#define PARAM_U8H             2
#define PARAM_DESC            3


static bool do_dac_write16(sys_config_t *config, uint8_t reg, uint16_t value);
static bool do_dac_set_slave_addr(sys_config_t *config, uint8_t addr);
static bool do_dump(sys_config_t *config);
static bool do_interactive(sys_config_t *config, const char *arg);

static inline int8_t cmd_prompt_handler(char *message, sys_config_t *config);
static int8_t get_line(char *str, int8_t max, uint8_t *ignore_lf);
static uint8_t parse_param(void *param, uint8_t type, char *arg);
static void save_configuration(sys_config_t *config);
static void default_configuration(sys_config_t *config);

static uint8_t _g_max_history;
static uint8_t _g_show_history;
static uint8_t _g_next_history;
static char _g_cmd_history[CMD_MAX_HISTORY][CMD_MAX_LINE];

void cmd_prompt(sys_config_t *config)
{
    char cmdbuf[64];
    uint8_t ignore_lf = 0;

    printf("\r\n");
    
    for (;;)
    {
        int8_t ret;

        printf("cmd>");
        ret = get_line(cmdbuf, sizeof(cmdbuf), &ignore_lf);

        if (ret == 0 || ret == -1) {
            printf("\r\n");
            continue;
        }

        ret = cmd_prompt_handler(cmdbuf, config);

        if (ret > 0)
            printf("Error: command failed\r\n");

        if (ret == -1) {
            return;
        }
    }
}

static void do_show(sys_config_t *config)
{
    printf(
            "\r\nCurrent configuration:\r\n\r\n"
            "\ti2c_addr .........: %xh\r\n"
          , config->i2c_addr
        );

    printf("\r\n");
}

static void do_help(void)
{
    printf(
        "\r\nCommands:\r\n\r\n"
        "\tshow\r\n"
        "\t\tShow current configuration\r\n\r\n"
        "\tdefault\r\n"
        "\t\tLoad the default configuration\r\n\r\n"
        "\tsave\r\n"
        "\t\tSave current configuration\r\n\r\n"
        "\tdump\r\n"
        "\t\tDump current register values from DAC\r\n\r\n"
        "\taddr [0 to 7f]\r\n"
        "\t\tSets I2C slave addr used by this board. Factory default: 60h\r\n\r\n"
        "\tpgmaddr [0 to 7f]\r\n"
        "\t\tPrograms I2C slave addr used by the DAC\r\n\r\n"
        "\toffset [0 to 4095]\r\n"
        "\tnvoffset [0 to 4095]\r\n"
        "\t\tSets DAC0 (non)volatile register\r\n\r\n"
        "\tgain [0 to 4095]\r\n"
        "\tnvgain [0 to 4095]\r\n"
        "\t\tSets DAC1 (non)volatile register\r\n\r\n"
        "\tinteractive|int [gain|offset]\r\n"
        "\t\tPerform interactive calibration\r\n\r\n"
        );
}

static inline int8_t cmd_prompt_handler(char *text, sys_config_t *config)
{
    char *command;
    char *arg;

    command = strtok(text, " ");
    arg = strtok(NULL, "");

    if (!stricmp(command, "dump")) {
        if (!do_dump(config))
            return 1;
        return 0;
    }    
    else if (!stricmp(command, "addr")) {
        return parse_param(&config->i2c_addr, PARAM_U8H, arg);
    }
    else if (!stricmp(command, "pgmaddr")) {
        uint8_t new_addr;
        if (parse_param(&new_addr, PARAM_U8H, arg))
            return 1;
        if (do_dac_set_slave_addr(config, new_addr))
            return 0;
        return 1;
    }
    else if (!stricmp(command, "offset")) {
        uint16_t dac_value;
        if (parse_param(&dac_value, PARAM_U16, arg))
            return 1;
        if (do_dac_write16(config, MCP47FEBXX_VOLATILE_DAC0 | MCP47FEBXX_CMD_WRITE, dac_value))
            return 0;
        return 1;
    }
    else if (!stricmp(command, "gain")) {
        uint16_t dac_value;
        if (parse_param(&dac_value, PARAM_U16, arg))
            return 1;
        if (do_dac_write16(config, MCP47FEBXX_VOLATILE_DAC1 | MCP47FEBXX_CMD_WRITE, dac_value))
            return 0;
        return 1;
    }
    else if (!stricmp(command, "nvoffset")) {
        uint16_t dac_value;
        if (parse_param(&dac_value, PARAM_U16, arg))
            return 1;
        if (do_dac_write16(config, MCP47FEBXX_NONVOLATILE_DAC0 | MCP47FEBXX_CMD_WRITE, dac_value))
            return 0;
        return 1;
    }
    else if (!stricmp(command, "nvgain")) {
        uint16_t dac_value;
        if (parse_param(&dac_value, PARAM_U16, arg))
            return 1;
        if (do_dac_write16(config, MCP47FEBXX_NONVOLATILE_DAC1 | MCP47FEBXX_CMD_WRITE, dac_value))
            return 0;
        return 1;
    }
    else if (!stricmp(command, "interactive") || !stricmp(command, "int")) {
        if (do_interactive(config, arg))
            return 0;
        return 1;
    }
    else if (!stricmp(command, "save")) {
        save_configuration(config);
        printf("\r\nConfiguration saved.\r\n\r\n");
        return 0;
    }
    else if (!stricmp(command, "default")) {
        default_configuration(config);
        printf("\r\nDefault configuration loaded.\r\n\r\n");
        return 0;
    }
    else if (!stricmp(command, "exit")) {
        printf("\r\nStarting...\r\n");
        return -1;
    }
    else if (!stricmp(command, "show")) {
        do_show(config);
    }
    else if ((!stricmp(command, "help") || !stricmp(command, "?"))) {
        do_help();
        return 0;
    }
    else
    {
        printf("Error: no such command (%s)\r\n", command);
        return 1;
    }

    return 0;
}

static bool do_interactive(sys_config_t *config, const char *arg)
{
    uint8_t reg = MCP47FEBXX_VOLATILE_DAC0;
    uint16_t value;
    char c;
    
    if (!stricmp(arg, "gain"))
    {
        reg = MCP47FEBXX_VOLATILE_DAC1;
    }
    else if (strcmp(arg, "offset"))
    {
        printf("Error: invalid argument\r\n");
        return false;
    }
    
    if (!i2c_read16(config->i2c_addr, reg | MCP47FEBXX_CMD_READ, &value))
        return false;    
    
    printf("Performing interactive calibration for %s\r\n", reg == MCP47FEBXX_VOLATILE_DAC1 ? "gain" : "offset");
    printf("Current value is %d. Press +/- to increase/decrease. Esc to exit and save to NV\r\n", value);
    
    do {
        c = wdt_getch();
        
        if (c == '+')
            value++;
        if (c == '-')
            value--;
        
        i2c_write16(config->i2c_addr, reg | MCP47FEBXX_CMD_WRITE, value);
        
    } while (c != SEQ_ESCAPE_CHAR);
    
    if (reg == MCP47FEBXX_VOLATILE_DAC0)
        i2c_write16(config->i2c_addr, MCP47FEBXX_NONVOLATILE_DAC0 | MCP47FEBXX_CMD_WRITE, value);

    if (reg == MCP47FEBXX_VOLATILE_DAC1)
        i2c_write16(config->i2c_addr, MCP47FEBXX_NONVOLATILE_DAC1 | MCP47FEBXX_CMD_WRITE, value);
    
    return true;
}

static bool do_dac_write16(sys_config_t *config, uint8_t reg, uint16_t value)
{
    return i2c_write16(config->i2c_addr, reg, value);
}

static bool do_dac_set_slave_addr(sys_config_t *config, uint8_t addr)
{
    uint16_t new_reg_value = addr;
    
    PORTAbits.RA3 = 1; // HV ON
    
    __delay_ms(1);

    if (!i2c_write_byte(config->i2c_addr, MCP47FEBXX_GAINCTRL_SLAVEADDR | MCP47FEBXX_CMD_DISABLE_CFG_BIT))
        return false;
    
    __delay_ms(1);
    
    PORTAbits.RA3 = 0; // HV OFF
    
    __delay_ms(100);

    if (!i2c_write16(config->i2c_addr, MCP47FEBXX_GAINCTRL_SLAVEADDR | MCP47FEBXX_CMD_WRITE, new_reg_value))
        return false;    
    
    config->i2c_addr = addr;
    
    __delay_ms(100);
    
    PORTAbits.RA3 = 1; // HV ON
    
    __delay_ms(1);

    if (!i2c_write(config->i2c_addr, MCP47FEBXX_GAINCTRL_SLAVEADDR | MCP47FEBXX_CMD_ENABLE_CFG_BIT,
            MCP47FEBXX_GAINCTRL_SLAVEADDR | MCP47FEBXX_CMD_ENABLE_CFG_BIT))
        return false;
    
    __delay_ms(1);
    
    PORTAbits.RA3 = 0; // HV OFF
    
    return true;
}

static bool do_dump(sys_config_t *config)
{
    uint16_t dac0;
    uint16_t dac1;
    uint16_t dac0_nv;
    uint16_t dac1_nv;
    uint16_t sladdr;

    if (!i2c_read16(config->i2c_addr, MCP47FEBXX_VOLATILE_DAC0 | MCP47FEBXX_CMD_READ, &dac0))
        return false;

    if (!i2c_read16(config->i2c_addr, MCP47FEBXX_VOLATILE_DAC1 | MCP47FEBXX_CMD_READ, &dac1))
        return false;
    
    if (!i2c_read16(config->i2c_addr, MCP47FEBXX_NONVOLATILE_DAC0 | MCP47FEBXX_CMD_READ, &dac0_nv))
        return false;

    if (!i2c_read16(config->i2c_addr, MCP47FEBXX_NONVOLATILE_DAC1 | MCP47FEBXX_CMD_READ, &dac1_nv))
        return false;
    
    if (!i2c_read16(config->i2c_addr, MCP47FEBXX_GAINCTRL_SLAVEADDR | MCP47FEBXX_CMD_READ, &sladdr))
        return false;

    printf(
            "\r\nCurrent registers:\r\n\r\n"
            "\tV  DAC0 (offset) ......: %d\r\n"
            "\tNV DAC0 (offset) ......: %d\r\n"
            "\tV  DAC1 (gain) ........: %d\r\n"
            "\tNV DAC1 (gain) ........: %d\r\n"
            "\tGainctrl / Slave reg ..: %x\r\n"
          , dac0, dac0_nv, dac1, dac1_nv, sladdr
        );

    printf("\r\n");

    return true;
}

static void default_configuration(sys_config_t *config)
{
    config->magic = CONFIG_MAGIC;
    config->i2c_addr = MCP47FEBXX_A0_SLAVE_ADDR;
}

static uint8_t parse_param(void *param, uint8_t type, char *arg)
{
    uint16_t u16param;
    uint8_t u8param;
    char *sparam;

    if (!arg || !*arg)
    {
        /* Avoid stack overflow */
        printf("Error: Missing parameter\r\n");
        return 1;
    }

    switch (type)
    {
        case PARAM_U8H:
            u8param = (uint8_t)strtol(arg, NULL, 16);
            *(uint8_t *)param = u8param;
            break;
        case PARAM_U8:
            if (*arg == '-')
                return 1;
            u8param = (uint8_t)atoi(arg);
            *(uint8_t *)param = u8param;
            break;
        case PARAM_U16:
            u16param = atoi(arg);
            *(int16_t *)param = u16param;
            break;
        case PARAM_DESC:
            sparam = (char *)param;
            strncpy(sparam, arg, MAX_DESC);
            sparam[MAX_DESC - 1] = 0;
            break;
    }
    return 0;
}

static void cmd_erase_line(uint8_t count)
{
    printf("%c[%dD%c[K", SEQ_ESCAPE_CHAR, count, SEQ_ESCAPE_CHAR);
}

static void config_next_command(char *cmdbuf, int8_t *count)
{
    uint8_t previdx;

    if (!_g_max_history)
        return;

    if (*count)
        cmd_erase_line(*count);

    previdx = ++_g_show_history;

    if (_g_show_history == CMD_MAX_HISTORY)
    {
        _g_show_history = 0;
        previdx = 0;
    }

    strcpy(cmdbuf, _g_cmd_history[previdx]);
    *count = strlen(cmdbuf);
    printf("%s", cmdbuf);
}

static void config_prev_command(char *cmdbuf, int8_t *count)
{
    uint8_t previdx;

    if (!_g_max_history)
        return;

    if (*count)
        cmd_erase_line(*count);

    if (_g_show_history == 0)
        _g_show_history = CMD_MAX_HISTORY;

    previdx = --_g_show_history;

    strcpy(cmdbuf, _g_cmd_history[previdx]);
    *count = strlen(cmdbuf);
    printf("%s", cmdbuf);
}

static int get_string(char *str, int8_t max, uint8_t *ignore_lf)
{
    unsigned char c;
    uint8_t state = CMD_READLINE;
    int8_t count;

    count = 0;
    do {
        c = wdt_getch();

        if (state == CMD_ESCAPE) {
            if (c == SEQ_CTRL_CHAR1) {
                state = CMD_AWAIT_NAV;
                continue;
            }
            else {
                state = CMD_READLINE;
                continue;
            }
        }
        else if (state == CMD_AWAIT_NAV)
        {
            if (c == SEQ_ARROW_UP) {
                config_prev_command(str, &count);
                state = CMD_READLINE;
                continue;
            }
            else if (c == SEQ_ARROW_DOWN) {
                config_next_command(str, &count);
                state = CMD_READLINE;
                continue;
            }
            else if (c == SEQ_DEL) {
                state = CMD_DEL;
                continue;
            }
            else if (c == SEQ_HOME || c == SEQ_END || c == SEQ_INS || c == SEQ_PGUP || c == SEQ_PGDN) {
                state = CMD_DROP_NAV;
                continue;
            }
            else {
                state = CMD_READLINE;
                continue;
            }
        }
        else if (state == CMD_DEL) {
            if (c == SEQ_NAV_END && count) {
                putch('\b');
                putch(' ');
                putch('\b');
                count--;
            }

            state = CMD_READLINE;
            continue;
        }
        else if (state == CMD_DROP_NAV) {
            state = CMD_READLINE;
            continue;
        }
        else
        {
            if (count >= max) {
                count--;
                break;
            }

            if (c == 19) /* Swallow XOFF */
                continue;

            if (c == CTL_U) {
                if (count) {
                    cmd_erase_line(count);
                    *(str) = 0;
                    count = 0;
                }
                continue;
            }

            if (c == SEQ_ESCAPE_CHAR) {
                state = CMD_ESCAPE;
                continue;
            }

            /* Unix telnet sends:    <CR> <NUL>
            * Windows telnet sends: <CR> <LF>
            */
            if (*ignore_lf && (c == '\n' || c == 0x00)) {
                *ignore_lf = 0;
                continue;
            }

            if (c == 3) { /* Ctrl+C */
                return -1;
            }

            if (c == '\b' || c == 0x7F) {
                if (!count)
                    continue;

                putch('\b');
                putch(' ');
                putch('\b');
                count--;
                continue;
            }
            if (c != '\n' && c != '\r') {
                putch(c);
            }
            else {
                if (c == '\r') {
                    *ignore_lf = 1;
                    break;
                }

                if (c == '\n')
                    break;
            }
            str[count] = c;
            count++;
        }
    } while (1);

    str[count] = 0;
    return count;
}

static int8_t get_line(char *str, int8_t max, uint8_t *ignore_lf)
{
    uint8_t i;
    int8_t ret;
    int8_t tostore = -1;

    ret = get_string(str, max, ignore_lf);

    if (ret <= 0) {
        return ret;
    }
    
    if (_g_next_history >= CMD_MAX_HISTORY)
        _g_next_history = 0;
    else
        _g_max_history++;

    for (i = 0; i < CMD_MAX_HISTORY; i++)
    {
        if (!stricmp(_g_cmd_history[i], str))
        {
            tostore = i;
            break;
        }
    }

    if (tostore < 0)
    {
        // Don't have this command in history. Store it
        strcpy(_g_cmd_history[_g_next_history], str);
        _g_next_history++;
        _g_show_history = _g_next_history;
    }
    else
    {
        // Already have this command in history, set the 'up' arrow to retrieve it.
        tostore++;

        if (tostore == CMD_MAX_HISTORY)
            tostore = 0;

        _g_show_history = tostore;
    }

    printf("\r\n");

    return ret;
}


void load_configuration(sys_config_t *config)
{
    uint16_t config_size = sizeof(sys_config_t);
    if (config_size > 0x100)
    {
        printf("\r\nConfiguration size is too large. Currently %u bytes.", config_size);
        reset();
    }
    
    eeprom_read_data(0, (uint8_t *)config, sizeof(sys_config_t));

    if (config->magic != CONFIG_MAGIC)
    {
        printf("\r\nNo configuration found. Setting defaults\r\n");
        default_configuration(config);
        save_configuration(config);
    }
}

static void save_configuration(sys_config_t *config)
{
    eeprom_write_data(0, (uint8_t *)config, sizeof(sys_config_t));
}