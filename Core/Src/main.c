/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "usart.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "reg_map.h"
#include "pcb.h"
#include <string.h>	// Just for memcpy and memcmp
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//    Internal EEPROM Address
#define INTEEPROM_BAR	0x080080000
#define INTEEPROM_BOARD_INFO_BYTES 112
//	Defined to be 1kB only because we have so much ram on the chip
#define MAX_SPI_TRANSMISSION_SIZE 1024
#define TX_MEM_SIZE MAX_SPI_TRANSMISSION_SIZE + 1
#define RX_MEM_SIZE TX_MEM_SIZE
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t rx_mem[RX_MEM_SIZE] = {0x00};
uint8_t tx_mem[TX_MEM_SIZE] = {0x00};

uint8_t cmd_val = 0x00;
uint8_t read_nWrite_bit = 0x0; // Read_not_Write bit, following I2C convention

HAL_StatusTypeDef status = HAL_OK;

volatile uint8_t overheat_flag = 0x00;		// Flag to state that the Synth has been turned off
volatile uint8_t reg_isr = 0x00;			// Not used
volatile uint8_t reg_isr_mask = 0x00;		// Not used
volatile uint8_t time_to_read_temp = 0x00;	// Flag that states to read the temperatures and compare the temp to threshold

uint8_t reg_synth_temp_thld[2] = {0x1C, 0xD4};
uint8_t reg_side_temp_thld[2] = {0x1C, 0xD4};

extern DMA_HandleTypeDef    hdma_spi1_tx;
extern I2C_HandleTypeDef    hi2c2;
extern SPI_HandleTypeDef    hspi1;
extern SPI_HandleTypeDef  	hspi2;

// nBytes is used for the SPI loop back command
uint32_t nBytes = 0x00;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void per_read_temperatures(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_LPUART1_UART_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM16_Init();
  MX_I2C2_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */

	/* Configure on board peripherals */
	HAL_TIM_OnePulse_Start(&htim16, TIM_CHANNEL_1);
	HAL_Delay(1000);
	__HAL_TIM_ENABLE(&htim16);
	HAL_Delay(1000);
	__HAL_TIM_ENABLE(&htim16);
	pcb_init();
	HAL_Delay(100);
	__HAL_TIM_ENABLE(&htim16);
	HAL_TIM_Base_Start_IT(&htim6);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		status = HAL_SPI_TransmitReceive_DMA(&hspi1, &tx_mem[0], &rx_mem[0], CMD_SIZE_STD);
		if(status != HAL_OK)
		{
			while(hspi1.State != HAL_SPI_STATE_READY);
			status = HAL_SPI_TransmitReceive_DMA(&hspi1, &tx_mem[0], &rx_mem[0], CMD_SIZE_STD);
		}

		HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

    /* Wait for the Command to arrive */
		while(hspi1.State != HAL_SPI_STATE_READY)
		{
			if(time_to_read_temp)
			{
				per_read_temperatures();
			}
		}
		__HAL_TIM_ENABLE(&htim16);
		/* Perform pre-processing */
		cmd_val = rx_mem[0] >> 1;
		read_nWrite_bit = rx_mem[0] & 0x1;
		memset(tx_mem, 0x00, TX_MEM_SIZE);

		/* Process the command */
		switch(cmd_val)
		{
			case CMD_NULL:
				if(read_nWrite_bit)
				{
				tx_mem[0] = RET_VAL_READ_GOOD | overheat_flag;
				memcpy(&tx_mem[1], &rx_mem[0], 5);
				}
				else
				{
				tx_mem[0] = RET_VAL_WRITE_GOOD | overheat_flag;
				memcpy(&tx_mem[1], &rx_mem[0], 5);
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
				Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_ISR:
				if(read_nWrite_bit)
				{
					tx_mem[0] = RET_VAL_READ_GOOD | overheat_flag;
					tx_mem[1] = reg_isr;
				}
				else
				{
					tx_mem[0] = RET_VAL_WRITE_GOOD | overheat_flag;
					tx_mem[1] = reg_isr;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_ISR_MASK:
				if(read_nWrite_bit)
				{
					tx_mem[0] = RET_VAL_READ_GOOD | overheat_flag;
					tx_mem[1] = reg_isr_mask;
				}
				else
				{
					reg_isr_mask = rx_mem[1];
					tx_mem[0] = RET_VAL_WRITE_GOOD | overheat_flag;
					tx_mem[1] = reg_isr_mask;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_LOCAL_FW_IDN:
				if(read_nWrite_bit)
				{
					// Good command, return the number of bytes the software needs to read back for the next transaction
					tx_mem[0] = RET_VAL_READ_GOOD | overheat_flag;
					tx_mem[1] = sizeof(LOCAL_FW_IDN);
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					while(hspi1.State != HAL_SPI_STATE_READY); // Wait until all data is sent
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

					// Copy the Firmware ID to the buffer
					memcpy(&tx_mem[0], (uint8_t*)LOCAL_FW_IDN, sizeof(LOCAL_FW_IDN));

					// Transmit the FWID byte data buffer, let the loop finish the transaction
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], sizeof(LOCAL_FW_IDN));
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				else
				{
					tx_mem[0] = RET_VAL_INVALID_CMD | overheat_flag;
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], 12);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				break;	// case CMD_LOCAL_FW_CID

			case CMD_LOCAL_FW_CID:
				if(read_nWrite_bit)
				{
					// Good command, return the number of bytes the software needs to read back for the next transaction
					tx_mem[0] = RET_VAL_READ_GOOD | overheat_flag;
					tx_mem[1] = 12;	// 96 bits or 12 Bytes
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
					Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					while(hspi1.State != HAL_SPI_STATE_READY); // Wait until all data is sent
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

					// Copy the Chip ID (AKA Device Unique ID) in to the tx_mem buffer
					memcpy(&tx_mem[0], (uint8_t*)UID_BASE, 8);	// BAR + 0x00 and 0x04
					memcpy(&tx_mem[8], (uint8_t*)UID_BASE + 0x14, 4);	// BAR + 0x14

					// Transmit the 12 byte data buffer, let the loop finish the transaction
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], 12);
					if(status != HAL_OK)
					{
					Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				else
				{
					tx_mem[0] = RET_VAL_INVALID_CMD | overheat_flag;
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], 12);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}

				break;	// case CMD_LOCAL_FW_CID

			case CMD_LOCAL_FW_BSN:
				if(read_nWrite_bit)
				{
					// Good command, return the number of bytes the software needs to read back for the next transaction
					tx_mem[0] = RET_VAL_READ_GOOD | overheat_flag;
					tx_mem[1] = 16; // Bytes
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					while(hspi1.State != HAL_SPI_STATE_READY);	// Wait until all data is sent
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

					// Copy first 16 bytes from the EEPROM which contain the board serial number
					memcpy(&tx_mem[0], (uint8_t*)INTEEPROM_BAR, 16);

					// Transmit the 16 byte data buffer, let the loop finish the transaction
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], 16);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				else
				{
					tx_mem[0] = RET_VAL_INVALID_CMD  | overheat_flag;
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				break;	// case CMD_LOCAL_FW_BSN

			case CMD_LOCAL_FW_EEPROM:
				if(read_nWrite_bit)
				{
					// Good command, return the number of bytes the software needs to read back for the next transaction
					tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
					tx_mem[1] = INTEEPROM_BOARD_INFO_BYTES; // Bytes
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					while(hspi1.State != HAL_SPI_STATE_READY);	// Wait until all data is sent
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

					// Copy first 16 bytes from the EEPROM which contain the board serial number
					memcpy((uint32_t*)&tx_mem[0], (uint32_t*)INTEEPROM_BAR, INTEEPROM_BOARD_INFO_BYTES);

					// Transmit the 16 byte data buffer, let the loop finish the transaction
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], INTEEPROM_BOARD_INFO_BYTES);
					if(status != HAL_OK)
					{
					Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				else
				{
					tx_mem[0] = RET_VAL_INVALID_CMD  | overheat_flag;
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}

				break;	// case CMD_LOCAL_FW_EEPROM

			case CMD_LOCAL_LOOPBACK:
				if(read_nWrite_bit)
				{
					pcb_get_loopback_state(&tx_mem[1]);
					tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
				}
				else
				{
					pcb_set_loopback_state(&rx_mem[1]);
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
					pcb_get_loopback_state(&tx_mem[1]);
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_LOCAL_PIN_STATE:
				if(read_nWrite_bit)
				{
					tx_mem[0] = RET_VAL_READ_GOOD | overheat_flag;
					pcb_get_gpio_state(&tx_mem[1]);
				}
				else
				{
					pcb_set_gpio_state(&rx_mem[1]);
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
					pcb_get_gpio_state(&tx_mem[1]);
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_LOCAL_SOFT_RST:
				if(read_nWrite_bit)
				{
					tx_mem[0] = RET_VAL_INVALID_CMD;
				}
				else
				{
					if(memcmp(&rx_mem[1], &RST_MAGIC_NUM[0], 4) == 0)
					{
						/* Trigger a hardware MCU reset */
						HAL_NVIC_SystemReset();
					}
					else
					{
					tx_mem[0] = RET_VAL_INVALID_CMD;
					}
				}
				break;

			case CMD_TEMP_THLD:
				if(read_nWrite_bit)
				{
					tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
					tx_mem[1] = reg_synth_temp_thld[1];
					tx_mem[2] = reg_synth_temp_thld[0];
					tx_mem[3] = reg_side_temp_thld[1];
					tx_mem[4] = reg_side_temp_thld[0];
					overheat_flag = 0x00;
				}
				else
				{
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
					// Copy new values over from rx_mem
					reg_synth_temp_thld[1] = rx_mem[1];
					reg_synth_temp_thld[0] = rx_mem[2];
					reg_side_temp_thld[1] = rx_mem[3];
					reg_side_temp_thld[0] = rx_mem[4];
					// copy new values back to the tx_mem
					tx_mem[1] = reg_synth_temp_thld[1];
					tx_mem[2] = reg_synth_temp_thld[0];
					tx_mem[3] = reg_side_temp_thld[1];
					tx_mem[4] = reg_side_temp_thld[0];
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_TEMP_READ:
				if(read_nWrite_bit)
				{
					status = pcb_get_temp(&tx_mem[1]);
					if(status == HAL_OK)
					{
						tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
					}
					else
					{
						tx_mem[0] = RET_VAL_I2C_ADDR_NACK  | overheat_flag;
					}
				}
				else
				{
					tx_mem[0] = RET_VAL_INVALID_CMD  | overheat_flag;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_NULLING_CTRL:
				if(read_nWrite_bit)
				{
					pcb_get_null_ctrl_state(&tx_mem[1]);
					tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
				}
				else
				{
					pcb_set_null_ctrl_state(&rx_mem[1]);
					pcb_get_null_ctrl_state(&tx_mem[1]);
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_NULLING_UP:
				if(read_nWrite_bit)
				{
					pcb_get_dac_up(&tx_mem[1]);
					tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
				}
				else
				{
					pcb_set_dac_up(&rx_mem[1]);
					pcb_get_dac_up(&tx_mem[1]);
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_NULLING_DN:
				if(read_nWrite_bit)
				{
					pcb_get_dac_dn(&tx_mem[1]);
					tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
				}
				else
				{
					pcb_set_dac_dn(&rx_mem[1]);
					pcb_get_dac_dn(&tx_mem[1]);
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_SYNTH_SR:
				if(read_nWrite_bit)
				{
					if(pcb_synth_status(&tx_mem[0]) == HAL_OK)
					{
						tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
					}
					else
					{
						tx_mem[0] = RET_VAL_BUSY  | overheat_flag;
					}
				}
				else
				{
					tx_mem[0] = RET_VAL_INVALID_CMD  | overheat_flag;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_SYNTH_RST:
				if(read_nWrite_bit)
				{
					tx_mem[0] = RET_VAL_INVALID_CMD  | overheat_flag;
				}
				else
				{
					status = pcb_synth_rst();
					if(status != HAL_OK)
					{
					Error_Handler();
					}
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_SYNTH_INIT:
				status = pcb_init_synth();
				if(status != HAL_OK)
				{
					tx_mem[0] = RET_VAL_BUSY  | overheat_flag;
				}
				else
				{
					tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
				}

				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_SYNTH_WRITE:
				if(read_nWrite_bit)
				{
					if(pcb_synth_get_reg(&rx_mem[1], &tx_mem[0]) == HAL_OK)
					{
						tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
					}
					else
					{
						tx_mem[0] = RET_VAL_BUSY  | overheat_flag;
					}
				}
				else
				{
					if(pcb_synth_set_reg(&rx_mem[1], &tx_mem[0]) == HAL_OK)
					{
						tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
					}
					else
					{
						tx_mem[0] = RET_VAL_BUSY  | overheat_flag;
					}
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_SYNTH_REG_DUMP:
				if(read_nWrite_bit)
				{
					// Good command, return the number of bytes the software needs to read back for the next transaction
					tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
					tx_mem[1] = 65*3; // Bytes, 65 registers * 3 bytes per register [reg, data high, data low]
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					while(hspi1.State != HAL_SPI_STATE_READY);	// Wait until all data is sent
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

					// modify array
					for(int x = 0; x < 65; x += 1)
					{
						tx_mem[x*3] = x;	// What the SPI is going to receive from synth, and send back to SPI host
						rx_mem[x*3] = x;	// What the SPI is going to send to synth
						pcb_synth_get_reg(&rx_mem[x*3], &tx_mem[x*3]);
						// Set the reg number field to the reg number, ignore what the synth returns for this byte
						tx_mem[x*3] = x;
					}


					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], 65*3);
					if(status != HAL_OK)
					{
					Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				else
				{
					tx_mem[0] = RET_VAL_INVALID_CMD  | overheat_flag;
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				break;

			case CMD_I2C_IF:
				if(read_nWrite_bit)
				{
					status = HAL_I2C_Mem_Read(&hi2c2, rx_mem[1] << 1, (uint16_t)rx_mem[2], 1, &tx_mem[1], 2, 2000);
					if(status == HAL_OK)
					{
						tx_mem[0] = RET_VAL_READ_GOOD  | overheat_flag;
						// Data is already in the buffer
					}
					else
					{
						tx_mem[0] = RET_VAL_I2C_ADDR_NACK  | overheat_flag;
					}
				}
				else
				{
					status = HAL_I2C_Master_Transmit(&hi2c2, rx_mem[1] << 1, &rx_mem[2], 3, 2000);
					if(status == HAL_OK)
					{
						tx_mem[0] = RET_VAL_WRITE_GOOD  | overheat_flag;
						// Data is already in the buffer
					}
					else
					{
						tx_mem[0] = RET_VAL_I2C_ADDR_NACK  | overheat_flag;
					}
				}
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break;

			case CMD_SPI_LOOPBACK:
				// Copy The number of bytes over, this will be the expected number for each transaction
				nBytes = rx_mem[1];
				// Send a return response back to the user, once complete then start loopback
				tx_mem[0] = RET_VAL_WRITE_GOOD;
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				while(hspi1.State != HAL_SPI_STATE_READY);
				__HAL_TIM_ENABLE(&htim16);
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

				//Initial data grab of nBytes, after this is will be bi-directional
				status = HAL_SPI_Receive_DMA(&hspi1, &rx_mem[0], nBytes);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				while(hspi1.State != HAL_SPI_STATE_READY);
				__HAL_TIM_ENABLE(&htim16);
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

				// First byte needs to be the value for CMD_SPI_LOOPBACK, if it isn't, break
				while((rx_mem[0] >> 1) == CMD_SPI_LOOPBACK)
				{
					memcpy(&tx_mem[0], &rx_mem[0], nBytes);
					tx_mem[0] = RET_VAL_WRITE_GOOD;
					status = HAL_SPI_TransmitReceive_DMA(&hspi1, &tx_mem[0], &rx_mem[0], nBytes);
					if(status != HAL_OK)
					{
					Error_Handler();
					}
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					while(hspi1.State != HAL_SPI_STATE_READY); // Wait until all data is sent
					__HAL_TIM_ENABLE(&htim16);
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);
				}
				break;

			case CMD_PROG_EEPROM:
				if(read_nWrite_bit)
				{
					// Good command, return the number of bytes the software needs to read back for the next transaction
					tx_mem[0] = RET_VAL_READ_GOOD;
					tx_mem[1] = INTEEPROM_BOARD_INFO_BYTES; // Bytes
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					while(hspi1.State != HAL_SPI_STATE_READY);	// Wait until all data is sent
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

					// Copy 112 bytes from the EEPROM which contain the board serial number
					memcpy(&tx_mem[0], (uint8_t*)INTEEPROM_BAR, INTEEPROM_BOARD_INFO_BYTES);

					// Transmit the 112 byte data buffer, let the loop finish the transaction
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], INTEEPROM_BOARD_INFO_BYTES);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					// Let the commanding MCU know there is data ready
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				}
				else
				{
					if(rx_mem[1] == INTEEPROM_BOARD_INFO_BYTES)
					{
						tx_mem[0] = RET_VAL_WRITE_GOOD;
						tx_mem[1] = INTEEPROM_BOARD_INFO_BYTES;
						status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);

						// Let the commanding MCU know there is data ready
						HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
						while(hspi1.State != HAL_SPI_STATE_READY);	// Wait until all data is sent

						HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

						// Prepare to receive the incoming EEPROM data
						status = HAL_SPI_Receive_DMA(&hspi1, &rx_mem[0], INTEEPROM_BOARD_INFO_BYTES);
						if(status != HAL_OK)
						{
							Error_Handler();
						}
						HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
						while(hspi1.State != HAL_SPI_STATE_READY);
						__HAL_TIM_ENABLE(&htim16);
						HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_SET);

						// Now we have the data, write it to the internal EEPROM
						HAL_FLASH_Unlock();
						// Must write to the flash in chunks of 8 Bytes
						for(int x = 0; x < (INTEEPROM_BOARD_INFO_BYTES / 8); x++)
						{
						// status = HAL_DATA_EEPROMEx_Program(FLASH_TYPEPROGRAMDATA_BYTE, INTEEPROM_BAR + x, rx_mem[x]);
						status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, INTEEPROM_BAR + x, rx_mem[x]);
						if(status != HAL_OK)
						{
							tx_mem[0] = RET_VAL_WRITE_GOOD;
							tx_mem[2] = x;
							tx_mem[3] = 0xFA;
							tx_mem[4] = 0x11;
							break;
						}
					}
					HAL_FLASH_Lock();
					tx_mem[0] = RET_VAL_WRITE_GOOD;
					tx_mem[1] = INTEEPROM_BOARD_INFO_BYTES;

					// send response
					status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
					if(status != HAL_OK)
					{
						Error_Handler();
					}
					HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
					}	// if(rx_mem[1] == INTEEPROM_BOARD_INFO_BYTES)
				}	// if(read_nWrite_bit)
				break;	// case CMD_LOCAL_FW_EEPROM

			default:
				tx_mem[0] = RET_VAL_INVALID_CMD;
				memcpy(&tx_mem[1], &rx_mem[0], 5);
				status = HAL_SPI_Transmit_DMA(&hspi1, &tx_mem[0], CMD_SIZE_STD);
				if(status != HAL_OK)
				{
					Error_Handler();
				}
				HAL_GPIO_WritePin(SPI1_nINT_GPIO_Port, SPI1_nINT_Pin, GPIO_PIN_RESET);
				break; // case default
		}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// Timer handler for Tim7 which just tells the MCU when to read some temperatures
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
	time_to_read_temp = 1;
	__HAL_TIM_DISABLE(&htim6);
}

void per_read_temperatures(void)
{
	uint8_t temp_array[4] = {0x00};
	uint16_t synth_temp = 0x00;
	uint16_t synth_temp_thres = 0x00;
	uint16_t side_temp = 0x00;
	uint16_t side_temp_thres = 0x00;
	pcb_get_temp(&temp_array[0]);
	synth_temp = (temp_array[0] << 8) | temp_array[1];
	side_temp = (temp_array[2] << 8) | temp_array[3];
	synth_temp_thres = reg_synth_temp_thld[1] << 8 | reg_synth_temp_thld[0];
	side_temp_thres = reg_side_temp_thld[1] << 8 | reg_side_temp_thld[0];

	if((side_temp > side_temp_thres) | (synth_temp > synth_temp_thres))
	{
		//		pcb_synth_shutdown();
		overheat_flag = 0x80;
	}

	time_to_read_temp = 0;
	__HAL_TIM_ENABLE(&htim16);	// LED timer
	__HAL_TIM_ENABLE(&htim6);		// Periodic Check Timer
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == SPI1_nRST_Pin)
	{
		// If the spi chip select line is low when the reset line goes low, then reset the MCU
		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET)
		{
			HAL_NVIC_SystemReset();
		}
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
