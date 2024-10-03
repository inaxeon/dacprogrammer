/* 
 * File:   config.h
 * Author: Matt
 *
 * Created on 25 November 2014, 15:54
 */

#ifndef __MCP47FEBXX_H__
#define __MCP47FEBXX_H__

#define MCP47FEBXX_CMD_READ                 0x06
#define MCP47FEBXX_CMD_WRITE                0x00

#define MCP47FEBXX_VOLATILE_DAC0            (0x00 << 3)
#define MCP47FEBXX_VOLATILE_DAC1            (0x01 << 3)
#define MCP47FEBXX_NONVOLATILE_DAC0         (0x10 << 3)
#define MCP47FEBXX_NONVOLATILE_DAC1         (0x11 << 3)
#define MCP47FEBXX_GAINCTRL_SLAVEADDR       0x1A

#define MCP47FEBXX_A0_SLAVE_ADDR            0x60

#endif /* __MCP47FEBXX_H__ */