/*
 * pcb.h
 *
 *  Created on: Feb 3, 2022
 *      Author: DarkTyr
 */

#ifndef INC_PCB_H_
#define INC_PCB_H_

extern I2C_HandleTypeDef hi2c2;
extern SPI_HandleTypeDef hspi2;

HAL_StatusTypeDef pcb_init(void);
HAL_StatusTypeDef pcb_get_temp(uint8_t* buf);

HAL_StatusTypeDef pcb_get_dac_up(uint8_t* buf);
HAL_StatusTypeDef pcb_set_dac_up(uint8_t* buf);

HAL_StatusTypeDef pcb_get_dac_dn(uint8_t* buf);
HAL_StatusTypeDef pcb_set_dac_dn(uint8_t* buf);

void pcb_get_gpio_state(uint8_t* buf);
void pcb_set_gpio_state(uint8_t* buf);

void pcb_get_loopback_state(uint8_t* buf);
void pcb_set_loopback_state(uint8_t* buf);

void pcb_get_null_ctrl_state(uint8_t* buf);
void pcb_set_null_ctrl_state(uint8_t* buf);

HAL_StatusTypeDef pcb_init_synth(void);
HAL_StatusTypeDef pcb_synth_status(uint8_t* buf_rcvd);
HAL_StatusTypeDef pcb_synth_get_reg(uint8_t* buf_send, uint8_t* buf_rcvd);
HAL_StatusTypeDef pcb_synth_set_reg(uint8_t* buf_send, uint8_t* buf_rcvd);
HAL_StatusTypeDef pcb_synth_shutdown(void);
HAL_StatusTypeDef pcb_synth_rst(void);

#endif /* INC_PCB_H_ */
