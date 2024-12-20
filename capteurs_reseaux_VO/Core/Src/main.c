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
#include "can.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BMP280_driver.h"
#include "MPU9250_driver.h"
#include "motor_driver.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TRUE  1
#define FALSE 0
#define VERBOSE 0 // Enables debugging

#define SERIAL_BUFF_SIZE 7

#define GYRO_CAL_POINTS 1500

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
char serial_buff[SERIAL_BUFF_SIZE];

int gyro_K = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief  Transmit a character over UART.
 * @param  ch: Character to transmit.
 * @retval int: The transmitted character.
 */
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);

	return ch;
}

/**
 * @brief  Initialize BMP280 sensor.
 * @retval int: EXIT_SUCCESS if initialization is successful, EXIT_FAILURE if failed.
 */
int BMP280_init()
{
	if (BMP280_Check_id() == EXIT_FAILURE) return EXIT_FAILURE;		// Identification du BMP280
	if (BMP280_Config() == EXIT_FAILURE) return EXIT_FAILURE;		// Configuration du BMP280
	if (BMP280_calibration() == EXIT_FAILURE) return EXIT_FAILURE;	// Mise à jour des paramètres d'étalonage

	return EXIT_SUCCESS;
}

/**
 * @brief  Initialize the MPU9250 sensor.
 * @retval None
 */
void MPU9250_init()
{
	// Vérifie si l'IMU est configuré correctement et bloque si ce n'est pas le cas
	if (MPU_begin(&hi2c1, AD0_LOW, AFSR_4G, GFSR_500DPS, 0.98, 0.004) == TRUE)
	{
		//printf("Centrale inertielle configurée correctement\r\n");
	}
	else
	{
		printf("ERREUR!\r\n");
	}

	// Calibre l'IMU
	if (VERBOSE) printf("CALIBRATION EN COURS...\r\n");
	if (gyro_K == 0) gyro_K = GYRO_CAL_POINTS;
	MPU_calibrateGyro(&hi2c1, gyro_K);
}

/**
 * @brief  Initialize the motor driver and CAN interface.
 * @retval None
 */
void MOT_Init()
{
	CAN_Init();
	MOT_Set_mode(MOT_MODE_ANTICLOCKWISE, 1, 1);
	MOT_Set_origin();
}

int angle = 0;
const float proportional_coeff = 0.0001;
/**
 * @brief  Period elapsed callback in non-blocking mode.
 * @param  htim: TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM2) // Check if the interrupt is from Timer 2
	{
		// Motor movement proportional to the temperature
		angle += (int)(BMP280_get_temperature() * proportional_coeff)%360 - 180;

		// Sign and angle change to do complete rotations
		if (angle>0) MOT_Rotate(angle, MOT_ANGLE_POSITIVE);
		else if (angle<0) MOT_Rotate(-angle, MOT_ANGLE_NEGATIVE);
	}
}

/**
 * @brief  Process the command received from the Raspberry Pi over UART.
 * @param	String to parse, of type char
 * @retval	None
 */
void parse_RaspberryPI_Request(char* cmd)
{
	if (!strncmp(cmd, "GET_T", 5)) {
		printf("T=+%.2f_C\r\n", (float) (BMP280_get_temperature()/100));
	}
	else if (!strncmp(cmd, "GET_P", 5)) {
		printf("P=%dPa\r\n", (int) (BMP280_get_pressure()/256));
	}
	else if (!strncmp(cmd, "SET_K", 5)) {
		MPU_calibrateGyro(&hi2c1, gyro_K);
		printf("SET_K=OK\r\n");
	}
	else if (!strncmp(cmd, "GET_K", 5)) {
		printf("K=%.5f\r\n", (float) gyro_K/100);
	}
	else if (!strncmp(cmd, "GET_A", 5)) {
		MPU_calcAttitude(&hi2c1);
		printf("A=%.1f;%.1f;%.1f\r\n", attitude.r, attitude.p, attitude.y);
	}
	else {
		printf("Unknown request: %s\r\n", cmd);
	}
}

/**
 * @brief  UART Receive Event callback function. It processes received data from USART1.
 * @param  huart: Pointer to the UART handle.
 * @param  Size: Size of received data.
 * @retval None
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if (huart->Instance == USART1)
	{
		// Restart reception for the next byte
		HAL_UARTEx_ReceiveToIdle_DMA(&huart1, (uint8_t*)serial_buff, sizeof(serial_buff));
		parse_RaspberryPI_Request(serial_buff);
	}
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

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART2_UART_Init();
	MX_USART1_UART_Init();
	MX_CAN1_Init();
	MX_TIM2_Init();
	MX_I2C1_Init();
	/* USER CODE BEGIN 2 */
	printf("\r\n=== TP Capteurs & Reseaux ===\r\n");
	// Initialize external peripherals
	if (BMP280_init() == EXIT_FAILURE) return EXIT_FAILURE;
	MOT_Init();
	MPU9250_init();

	// Enable Timer 2 IT
	HAL_TIM_Base_Start_IT(&htim2);

	// Start USART1 DMA reception
	HAL_UARTEx_ReceiveToIdle_DMA(&huart1, (uint8_t*)serial_buff, sizeof(serial_buff));
	// Enable UART IDLE interrupt
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
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
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 80;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
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
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
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
