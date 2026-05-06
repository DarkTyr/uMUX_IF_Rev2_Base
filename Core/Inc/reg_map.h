/*
 * reg_map.h
 *
 *  Created on: Jan 27, 2022
 *      Author: DarkTyr
 */

#ifndef INC_REG_MAP_H_
#define INC_REG_MAP_H_

#include <string.h>
#define CMD_SIZE_STD 6
#define CMD_SIZE_CID 12

/* Definitions for Return Value bit places */
#define RET_VAL_WRITE_GOOD    0x01
#define RET_VAL_READ_GOOD     0x02
#define RET_VAL_INVALID_CMD   0x04
#define RET_VAL_I2C_ADDR_NACK 0x08
#define RET_VAL_MCU_RST_DONE  0x10
#define RET_VAL_BUSY          0x20
#define RET_VAL_OVERHEATING   0x80
/* Definitions for Interrupt Request and Status Registers, by bit */
#define RET_IRQ_ERROR         0x01
#define RET_IRQ_DATA_READ     0x02
#define RET_IRQ_LOCAL_DONE    0x04
#define RET_IRQ_TEMPERATURE_DONE  0x08
#define RET_IRQ_NULL_DONE     0x10
#define RET_IRQ_SYNTH_DONE    0x20
#define RET_IRQ_OVERHEATING   0x80

typedef enum _ALL_CMDS {
	CMD_NULL =  0x00,
	CMD_ISR,
	CMD_ISR_MASK,
	CMD_LOCAL_FW_IDN  = 0x10,
	CMD_LOCAL_FW_CID,		// Chip 96bit ID
	CMD_LOCAL_FW_BSN,		// Reads first 16 bytes from EEPROM
	CMD_LOCAL_FW_EEPROM,	// Reads entire EEPROM that is written (112 Bytes)
	CMD_LOCAL_LOOPBACK,
	CMD_LOCAL_PIN_STATE,
	CMD_LOCAL_SOFT_RST,
	CMD_TEMP_THLD = 0x20,
	CMD_TEMP_READ,
	CMD_NULLING_CTRL = 0x30,
	CMD_NULLING_UP,
	CMD_NULLING_DN,
	CMD_SYNTH_SR = 0x40,
	CMD_SYNTH_RST,
	CMD_SYNTH_INIT,
	CMD_SYNTH_WRITE,
	CMD_SYNTH_REG_DUMP,
	CMD_I2C_IF = 0x50,
	CMD_SPI_LOOPBACK,
	CMD_PROG_EEPROM = 0x60	// Writes entire EEPROM that is written (112 Bytes)
} ALL_CMDS_t;

const uint8_t RST_MAGIC_NUM[4] = {0x55, 0x44, 0x33, 0x22};
#define LOCAL_FW_IDN "uMux_IF_Rev2_Base 1.0.0"


#endif /* INC_REG_MAP_H_ */
