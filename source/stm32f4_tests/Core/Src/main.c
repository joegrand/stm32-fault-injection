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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "payload.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
	return ch;
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void print_hex_memory(void *mem, uint32_t len) {
	uint32_t i;
	unsigned char *p = (unsigned char *)mem;

	for (i=0;i<len;i++) {
		printf("0x%02x ", p[i]);
	}
	printf("\n");
}

// Lennert: Taken from a ChipWhisperer example
void glitch(void)
{
	unsigned int k = 0;
	//Declared volatile to avoid optimizing away loop.
	//This also adds lots of SRAM access
	volatile uint16_t i, j;
	volatile unsigned int cnt;
	cnt = 0;

	HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_SET);
	for(i=0; i<50; i++){
		for(j=0; j<50; j++){
			cnt++;
		}
	}
	HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_RESET);

	printf("%u %u %u %u\n", cnt, i, j, k++);
}
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
  //__disable_irq();
  SCB->VTOR = 0x08000000;
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_SET);
  MX_USART1_UART_Init();
  //__disable_irq();
  printf("Hello!\n");
  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_RESET);
    uint8_t *mempointer;
    uint32_t address;
    char *str;
    uint8_t buf[10];
    char cmd;

    static const char secret[] = "SECRET_STRING.\n";
  /* USER CODE END 2 */

    /* USER CODE BEGIN 3 */
    while (1)
    {
  	  HAL_UART_Receive (&huart1, buf, 1, 100);
  	  cmd = (char)buf[0];

  	  switch(cmd) {
  	  case 'A':
  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_SET);
  		  str = (char *) malloc(255);
  		  strcpy(str, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
  		  printf("Address = %p\n", str);
  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_RESET);
  		  break;

  	  case 'B':
  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_SET);
  		  HAL_UART_Receive (&huart1, buf, 4, 10000);
  		  address = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  		  mempointer = (uint8_t *)address;
  		  HAL_UART_Transmit(&huart1, mempointer, 128, HAL_MAX_DELAY);
  		  fflush(stdout);
  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_RESET);
  		  break;

  	  case 'C':
  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_SET);
  		  printf(secret);
  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_RESET);
  		  break;

  	  case 'D':
  		  glitch();
  		  break;

  	  case 'E':
  		  // https://stackoverflow.com/questions/8158007/how-to-jump-the-program-execution-to-a-specific-address-in-c
  		  // void *ptr = (void *)0x1234567;  // a random memory address
  		  //goto *ptr;                      // jump there -- probably crash

  		  HAL_UART_Receive (&huart1, buf, 4, 10000);
  		  address = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  		  void (*foo)(void) = (void (*)())address;

  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_SET);
  		  foo();
  		  //printf(secret);
  		  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_RESET);
  		  break;

  	  case 'P':
  		  uint32_t option_bytes = *(volatile uint32_t*)0x1FFFC008;
  		  uint32_t option_bytes2 = *(volatile uint32_t*)0x1FFEC008;
  		  uint32_t flash_optcr = *(volatile uint32_t*)0x40023C14;
  		  uint32_t flash_optcr1 = *(volatile uint32_t*)0x40023C18;

  		  uint8_t txBuffer[16];
  		  txBuffer[3] = (option_bytes >> 0)  & 0xFF;
  		  txBuffer[2] = (option_bytes >> 8)  & 0xFF;
  		  txBuffer[1] = (option_bytes >> 16) & 0xFF;
  		  txBuffer[0] = (option_bytes >> 24) & 0xFF;

  		  txBuffer[7] = (option_bytes2 >> 0)  & 0xFF;
  		  txBuffer[6] = (option_bytes2 >> 8)  & 0xFF;
  		  txBuffer[5] = (option_bytes2 >> 16) & 0xFF;
  		  txBuffer[4] = (option_bytes2 >> 24) & 0xFF;

  		  txBuffer[11] = (flash_optcr >> 0)  & 0xFF;
  		  txBuffer[10] = (flash_optcr >> 8)  & 0xFF;
  		  txBuffer[9] = (flash_optcr >> 16) & 0xFF;
  		  txBuffer[8] = (flash_optcr >> 24) & 0xFF;

  		  txBuffer[15] = (flash_optcr1 >> 0)  & 0xFF;
  		  txBuffer[14] = (flash_optcr1 >> 8)  & 0xFF;
  		  txBuffer[13] = (flash_optcr1 >> 16) & 0xFF;
  		  txBuffer[12] = (flash_optcr1 >> 24) & 0xFF;

  		  HAL_UART_Transmit(&huart1, txBuffer, sizeof(txBuffer), HAL_MAX_DELAY);
  		  fflush(stdout);
  		  break;

  	  case 'Q':
  		  uint8_t *sram_ptr = (uint8_t *)0x20000000;
  		  for (uint32_t i = 0; i < payload_bin_len; i++) {
  			  sram_ptr[i] = payload_bin[i];
  		  }
  		  printf("Payload loaded into SRAM!\n");
  		  break;

  	  case 'R':
  		  uint32_t bootloader_address = 0x20000000;
  		  uint32_t sp = *((volatile uint32_t *)bootloader_address);
  		  uint32_t pc = *((volatile uint32_t *)(bootloader_address + 4));

  		  typedef void (*func_ptr_t)(void);
  		  func_ptr_t bootloader_main = (func_ptr_t)pc;

  		  __disable_irq();
  		  __set_MSP(sp);
  		  bootloader_main();
  		  break;

  	  default:
  		  break;
  	  }

  	  buf[0] = 0;
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_9B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_EVEN;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(triggerpin_GPIO_Port, triggerpin_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : triggerpin_Pin */
  GPIO_InitStruct.Pin = triggerpin_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(triggerpin_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
