/*
 * pcb.c
 *
 *  Created on: Feb 3, 2022
 *      Author: DarkTyr
 */
#include "main.h"
#include "pcb.h"


#define RET_IRQ_DATA_READ     0x02

#define DAC_UP_ADDR 	0x49
#define DAC_REG_h2		0x02
#define DAC_REG_h3		0x03
#define DAC_REG_h4		0x04
#define DAC_DN_ADDR 	0x48
#define DAC_REG_DACA 	0x08
#define DAC_REG_DACB 	0x09

//#define STS31_ADDR0 	0x4A	// Back side of Synthesizer
//#define STS31_ADDR1 	0x4B	// Near MCU/Main Connector
#define STS31_REG_h1	0x01

/* Because the USART SPI handles the bytes in LSB first, we need to reverse all sent and received bytes */
// 	Stored in ROM in the flash
//static const unsigned char BitReverseTable256[] =

// 	Stored as array in Ram and initialized from ROM
unsigned char BitReverseTable256[] =
{
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
	0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
	0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
	0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
	0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
	0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

uint8_t STS31_ADDR0 = 	0x4A;	// Back side of Synthesizer
uint8_t STS31_ADDR1 =	0x4B;	// Near MCU/Main Connector
uint16_t STS31_temp_reg = 0x2C06;	// Clock stretching Enabled, High Repeatability
uint8_t STS31_temp_reg_size = 0x03;

typedef struct _i2c_init_entry
{
	uint16_t 	i2c_addr;
	uint16_t	mem_addr;
//	uint16_t	mem_size;		// In this case, is always 1
	uint8_t		data[2];
//	uint16_t	data_size;	// In this case, always is 2

} i2c_init_entry_t;

const i2c_init_entry_t i2c_init_array[] =
{
	{DAC_UP_ADDR,	DAC_REG_h2, 	{0x00, 0x00}},	// BRDCAST Disable, Sync Disable
	{DAC_UP_ADDR, 	DAC_REG_h3, 	{0x00, 0x00}},	// Ref is powered Up, DACs are powered on
	{DAC_UP_ADDR, 	DAC_REG_h4, 	{0x00, 0x00}},	// REF Divider Off, Buff Gain 1
	{DAC_DN_ADDR, 	DAC_REG_h2, 	{0x00, 0x00}},	// BRDCAST Disable, Sync Disable
	{DAC_DN_ADDR, 	DAC_REG_h3, 	{0x00, 0x00}},	// Ref is powered up, DACs are powered on
	{DAC_DN_ADDR, 	DAC_REG_h4, 	{0x00, 0x00}},	// REF Divider Off, Buff Gain 1
//	{STS31_ADDR0, 	STS31_REG_h1, 	{0x60, 0x00}},	// Set to highest resolution (12 bits)
//	{STS31_ADDR1, 	STS31_REG_h1, 	{0x60, 0x00}},	// Set to highest resolution (12 bits)
};

#define I2C_INIT_ARRAY_ENTRIES sizeof(i2c_init_array)/sizeof(i2c_init_entry_t)

/* Structure and array that defines the GPIO */
typedef struct _GPIO_DEF {
	GPIO_TypeDef* gpio_bank;
	uint16_t bank_pin;
	uint8_t	writable;
	uint8_t init_val;
} GPIO_DEF_t;

const GPIO_DEF_t gpio_array[] =
{	//Port                   ,    Pin           , Writable, Init_val
	{SPI1_nINT_GPIO_Port, 		SPI1_nINT_Pin, 		0x01,	GPIO_PIN_SET},	//    LSB of BIT information
	{Op_Amp_En_GPIO_Port, 		Op_Amp_En_Pin, 		0x01, 	GPIO_PIN_SET},	// Rev1 of PCB, this trace is cut, rev2 changed to an Enable
	{Synth_Enable_GPIO_Port,	Synth_Enable_Pin, 	0x01, 	GPIO_PIN_SET},
	{LoopBack_En_GPIO_Port, 	LoopBack_En_Pin,	0x01,	GPIO_PIN_SET},
	{Synth_CS0_GPIO_Port, 		Synth_CS0_Pin, 		0x01, 	GPIO_PIN_SET},
	{SPI1_nRST_GPIO_Port, 		SPI1_nRST_Pin,		0x00,	GPIO_PIN_RESET}	// Input Pin
};

#define GPIO_ARRAY_SIZE sizeof(gpio_array) / sizeof(GPIO_DEF_t)

/* Private Function Prototype */
void bit_swap_array(uint8_t* buf, size_t nBytes);
HAL_StatusTypeDef pcb_init_synth(void);

void pcb_init_gpio(void)
{
	// Loop through all the GPIO pins and set the initial state for them
	uint32_t i = 0x00;
	for(i = 0; i < GPIO_ARRAY_SIZE; i++)
	{
		if(gpio_array[i].writable == 0x1)
		{
			HAL_GPIO_WritePin(gpio_array[i].gpio_bank, gpio_array[i].bank_pin, gpio_array[i].init_val);
		}
	}

}

void pcb_init_i2c(void)
{
	HAL_StatusTypeDef status = HAL_OK;
	uint32_t i = 0x00;
	for(i = 0; i < I2C_INIT_ARRAY_ENTRIES; i++)
	{
		status = HAL_I2C_Mem_Write(&hi2c2,
								   i2c_init_array[i].i2c_addr << 1,
								   i2c_init_array[i].mem_addr, 0x01,
								   (uint8_t*)&i2c_init_array[i].data[0], 0x02,
								   1000);
		if(status != HAL_OK)
		{
			while(1);	// probably shouldn't do this
		}
	}
}

HAL_StatusTypeDef pcb_init(void)
{
	HAL_StatusTypeDef status = HAL_OK;
	// Init the gpio pins
	pcb_init_gpio();
	// Init the I2C devices
	pcb_init_i2c();
	// Init the Synth device
	status = pcb_init_synth();
	if(status != HAL_OK)
	{
		while(1);	// probably shouldn't do this, but here we are
	}
	return status;
}


HAL_StatusTypeDef pcb_get_temp(uint8_t* buf)
{
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t STS31_ret[3] = {0x00, 0x00, 0x00};

	status = HAL_I2C_Mem_Read(&hi2c2, STS31_ADDR0 << 1, STS31_temp_reg, 2, &STS31_ret[0], STS31_temp_reg_size, 1000);
	if(status == HAL_OK)
	{
		buf[0] = STS31_ret[0];
		buf[1] = STS31_ret[1];
	}
	else
	{
		return status;
	}

	status = HAL_I2C_Mem_Read(&hi2c2, STS31_ADDR1 << 1, STS31_temp_reg, 2, &STS31_ret[0], STS31_temp_reg_size, 1000);
	if(status == HAL_OK)
	{
		buf[2] = STS31_ret[0];
		buf[3] = STS31_ret[1];
	}
	else
	{
		return status;
	}
	return status;
}

HAL_StatusTypeDef pcb_get_dac_up(uint8_t* buf)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_I2C_Mem_Read(&hi2c2, DAC_UP_ADDR << 1, DAC_REG_DACA, 1, &buf[0], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}

	status = HAL_I2C_Mem_Read(&hi2c2, DAC_UP_ADDR << 1, DAC_REG_DACB, 1, &buf[2], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}
	return status;
}

HAL_StatusTypeDef pcb_set_dac_up(uint8_t* buf)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_I2C_Mem_Write(&hi2c2, DAC_UP_ADDR << 1, DAC_REG_DACA, 1, &buf[0], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}

	status = HAL_I2C_Mem_Write(&hi2c2, DAC_UP_ADDR << 1, DAC_REG_DACB, 1, &buf[2], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}
	return status;
}

HAL_StatusTypeDef pcb_get_dac_dn(uint8_t* buf)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_I2C_Mem_Read(&hi2c2, DAC_DN_ADDR << 1, DAC_REG_DACA, 1, &buf[0], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}

	status = HAL_I2C_Mem_Read(&hi2c2, DAC_DN_ADDR << 1, DAC_REG_DACB, 1, &buf[2], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}
	return status;
}

HAL_StatusTypeDef pcb_set_dac_dn(uint8_t* buf)
{
	HAL_StatusTypeDef status = HAL_OK;

	status = HAL_I2C_Mem_Write(&hi2c2, DAC_DN_ADDR << 1, DAC_REG_DACA, 1, &buf[0], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}

	status = HAL_I2C_Mem_Write(&hi2c2, DAC_DN_ADDR << 1, DAC_REG_DACB, 1, &buf[2], 0x02, 2000);
	if(status != HAL_OK)
	{
		return status;
	}
	return status;
}

void pcb_get_gpio_state(uint8_t* buf)
{
	uint32_t i = 0x00;
	uint32_t pin_state;
	uint16_t bit_mask = 0x01;

	for(i = 0; i < GPIO_ARRAY_SIZE; i++)
	{
		pin_state = HAL_GPIO_ReadPin(gpio_array[i].gpio_bank, gpio_array[i].bank_pin);
		if((pin_state == GPIO_PIN_SET) && (i < 8))
		{
			buf[1] = (uint8_t)(buf[1] | (bit_mask << i));
		}
		else if((pin_state == GPIO_PIN_SET) && (i >= 8))
		{
			buf[0] = (uint8_t)(buf[0] | (bit_mask << (i - 8)));
		}
	}
}

void pcb_set_gpio_state(uint8_t* buf)
{
	uint32_t i = 0x00;
	uint16_t pin_state;
	uint16_t pin_mask = 0x00;
	uint16_t bit_mask = 0x01;

	for(i = 0; i < GPIO_ARRAY_SIZE; i++)
	{
		pin_mask = (buf[1] & (bit_mask << i));
		if((gpio_array[i].writable == 1) & (pin_mask > 0))
		{
			pin_state = (buf[3] & (bit_mask << i));
			if(pin_state)
			{
				HAL_GPIO_WritePin(gpio_array[i].gpio_bank, gpio_array[i].bank_pin, GPIO_PIN_SET);
			}
			else
			{
				HAL_GPIO_WritePin(gpio_array[i].gpio_bank, gpio_array[i].bank_pin, GPIO_PIN_RESET);
			}
		}
	}

	bit_mask = 0x01;
	for(i = 8; i < GPIO_ARRAY_SIZE; i++)
	{
		pin_mask = (buf[0] & bit_mask);
		if((gpio_array[i].writable == 1) & (pin_mask > 0))
		{
			pin_state = (buf[2] & bit_mask);
			if(pin_state)
			{
				HAL_GPIO_WritePin(gpio_array[i].gpio_bank, gpio_array[i].bank_pin, GPIO_PIN_SET);
			}
			else
			{
				HAL_GPIO_WritePin(gpio_array[i].gpio_bank, gpio_array[i].bank_pin, GPIO_PIN_RESET);
			}
		}
		bit_mask = bit_mask << 1;
	}
}

void pcb_get_loopback_state(uint8_t* buf)
{
	uint8_t pin_state;
	pin_state = HAL_GPIO_ReadPin(LoopBack_En_GPIO_Port, LoopBack_En_Pin);
	if(pin_state == GPIO_PIN_SET)
	{
		buf[0] = 0x01;
	}
	else
	{
		buf[0] = 0x00;
	}
}

void pcb_set_loopback_state(uint8_t* buf)
{
	if(buf[0] == 0x01)
	{
		HAL_GPIO_WritePin(LoopBack_En_GPIO_Port, LoopBack_En_Pin, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(LoopBack_En_GPIO_Port, LoopBack_En_Pin, GPIO_PIN_RESET);
	}
}


void pcb_get_null_ctrl_state(uint8_t* buf)
{
	uint8_t pin_state;
	pin_state = HAL_GPIO_ReadPin(Op_Amp_En_GPIO_Port, Op_Amp_En_Pin);
	if(pin_state == GPIO_PIN_SET)
	{
		buf[0] = 0x01;
	}
	else
	{
		buf[0] = 0x00;
	}
}

void pcb_set_null_ctrl_state(uint8_t* buf)
{
	if(buf[0] == 0x01)
	{
		// Enabled
		HAL_GPIO_WritePin(Op_Amp_En_GPIO_Port, Op_Amp_En_Pin, GPIO_PIN_SET);
		pcb_dac_powerup();
	}
	else
	{
		// Disabled
		HAL_GPIO_WritePin(Op_Amp_En_GPIO_Port, Op_Amp_En_Pin, GPIO_PIN_RESET);
		pcb_dac_shutdown();
	}
}

void pcb_dac_shutdown()
{
	uint8_t i2c_buf[2] = {0x03, 0x01};
	HAL_I2C_Mem_Write(&hi2c2,
				   DAC_UP_ADDR << 1,
				   DAC_REG_h3, 0x01,
				   &i2c_buf[0], 0x02,
				   1000);

	HAL_I2C_Mem_Write(&hi2c2,
				   DAC_DN_ADDR << 1,
				   DAC_REG_h3, 0x01,
				   &i2c_buf[0], 0x02,
				   1000);
}

void pcb_dac_powerup()
{
	uint8_t i2c_buf[2] = {0x00, 0x00};
	HAL_I2C_Mem_Write(&hi2c2,
				   DAC_UP_ADDR << 1,
				   DAC_REG_h3, 0x01,
				   &i2c_buf[0], 0x02,
				   1000);

	HAL_I2C_Mem_Write(&hi2c2,
				   DAC_DN_ADDR << 1,
				   DAC_REG_h3, 0x01,
				   &i2c_buf[0], 0x02,
				   1000);
}

HAL_StatusTypeDef pcb_init_synth(void)
{
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t data_in[3] = {0x00};
	uint8_t data_out[3] = {0x00};
	data_out[0] = 0x00;	// R0
	data_out[1] = 0x22;	// Default
	data_out[2] = 0x9A;	// Default + Reset bit high
	status = pcb_synth_set_reg(&data_out[0], &data_in[0]);
	return status;
}

HAL_StatusTypeDef pcb_synth_status(uint8_t* buf_rcvd)
{
	HAL_StatusTypeDef status = HAL_OK;
	// Read R0
	// change the Muxout bit
	// read the Lock status
	// Set the muxout bit back
	// Return status
	uint8_t data_in[3] = {0x00};
	uint8_t data_out[3] = {0x00};
	uint8_t data_ex[3] = {0x00};
	status = pcb_synth_get_reg(&data_out[0], &data_in[0]);
	data_out[0] = 0x00;
	data_out[1] = data_in[1];
	data_out[2] = data_in[2] | 0x1 << 2;	// Set the MUXOUT bit to 1
	data_out[2] = data_out[2] & ~(0x1 << 3);	// RESET the FCAL bit, don't want to calibrate
	status = pcb_synth_set_reg(&data_out[0], &data_ex[0]);	// Write the command
	status = pcb_synth_set_reg(&data_out[0], &buf_rcvd[0]);	// Essential write it again, but reading back the LD signal
	// The output seemed to not be outputting Lock detect for the first read back, lets read back again.
	status = pcb_synth_set_reg(&data_out[0], &buf_rcvd[0]);	// Essential write it again, but reading back the LD signal
	status = pcb_synth_set_reg(&data_in[0], &data_ex[0]);		// Set R0 back to what it was
	return status;
}

HAL_StatusTypeDef pcb_synth_get_reg(uint8_t* buf_send, uint8_t* buf_rcvd)
{
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	HAL_StatusTypeDef status = HAL_OK;
	buf_send[0] = buf_send[0] | 0x80;	// MSB of register addr is R/nW
	buf_send[1] = 0x00;
	buf_send[2] = 0x00;
	status = HAL_SPI_TransmitReceive_DMA(&hspi2, &buf_send[0], &buf_rcvd[0], 0x03);
	while(hspi2.State != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_SET);
	return status;
}

HAL_StatusTypeDef pcb_synth_set_reg(uint8_t* buf_send, uint8_t* buf_rcvd)
{
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	HAL_StatusTypeDef status = HAL_OK;
	buf_send[0] = buf_send[0] & 0x7F;	// MSB of register addr is R/nW
	status = HAL_SPI_TransmitReceive_DMA(&hspi2, &buf_send[0], &buf_rcvd[0], 0x03);
	while(hspi2.State != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_SET);
	return status;
}

HAL_StatusTypeDef pcb_synth_shutdown(void)
{
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t buf_send[3] = {0x00};
	uint8_t buf_rcvd[3] = {0x00};
	// Read back register zero
	buf_send[0] = buf_send[0] | 0x80;	// MSB of register addr is R/nW
	status = HAL_SPI_TransmitReceive_DMA(&hspi2, &buf_send[0], &buf_rcvd[0], 0x03);
	while(hspi2.State != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	// Copy the read data over
	buf_send[0] = buf_rcvd[0];
	buf_send[1] = buf_rcvd[1];
	buf_send[2] = buf_rcvd[2];

	// Now write only the shutdown bit
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	buf_send[0] = buf_send[0] & 0x7F;	// MSB of register addr is R/nW
	buf_send[2] = buf_send[2] | 0x01; // Assign the powerdown bit
	buf_send[2] = buf_send[2] & ~(0x1 << 3);
	status = HAL_SPI_TransmitReceive_DMA(&hspi2, &buf_send[0], &buf_rcvd[0], 0x03);
	while(hspi2.State != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	return status;
}

HAL_StatusTypeDef pcb_synth_rst(void)
{
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_SET);
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t buf_send[3] = {0x00};
	uint8_t buf_rcvd[3] = {0x00};
	// Read back register zero
	buf_send[0] = buf_send[0] | 0x80;	// MSB of register addr is R/nW
	status = HAL_SPI_TransmitReceive_DMA(&hspi2, &buf_send[0], &buf_rcvd[0], 0x03);
	while(hspi2.State != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	// Copy the read data over
	buf_send[0] = buf_rcvd[0];
	buf_send[1] = buf_rcvd[1];
	buf_send[2] = buf_rcvd[2];

	// Now write only the shutdown bit
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_RESET);
	buf_send[0] = buf_send[0] & 0x7F;	// MSB of register addr is R/nW
	buf_send[2] = buf_send[2] | 0x02; // Assign the reset bit
	buf_send[2] = buf_send[2] & ~(0x1 << 3);	// Set FCAL to zero
	status = HAL_SPI_TransmitReceive_DMA(&hspi2, &buf_send[0], &buf_rcvd[0], 0x03);
	while(hspi2.State != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(Synth_CS0_GPIO_Port, Synth_CS0_Pin, GPIO_PIN_SET);
	// Reset bit is self resting to zero once done
	return status;
}


/* Bit swap each byte in the buffer up to nBytes
 * https://stackoverflow.com/questions/746171/efficient-algorithm-for-bit-reversal-from-msb-lsb-to-lsb-msb-in-c
 * http://graphics.stanford.edu/~seander/bithacks.html
 *
 * */
void bit_swap_array(uint8_t* buf, size_t nBytes)
{
	uint32_t i = 0x00;
	for(i = 0; i < nBytes; i++)
	{
		buf[i] = BitReverseTable256[buf[i]];
	}
}



