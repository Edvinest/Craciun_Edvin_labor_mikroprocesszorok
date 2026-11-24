/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "dac.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motor.h"
#include "OLED.h"
#include <stdio.h>
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// [panel] I2C-busz helyreallitasa. Ha a feltoltes vagy a reset egy atvitel kozepen erte a buszt,
// egy szenzor lefogva tarthatja az SDA-t (neki nincs reset laba, csak a tap elvetele allitja vissza).
// Ilyenkor minden I2C-hivas hibat ad, a kijelzo sem indul. A gyogymod: legfeljebb 9 orajel az SCL-en,
// amig az SDA el nem engedodik, aztan egy STOP. Az MX_I2C1_Init() elott kell lefutnia.
static void I2C_Bus_Recovery(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;          // PB6 = SCL, PB7 = SDA, most sima open-drain kimenetkent
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);
  for (int i = 0; i < 9 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
  }
  // START, majd STOP (SDA le, aztan fel, mikozben az SCL magas): minden eszkoz alapallapotba ker
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(1);
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
  I2C_Bus_Recovery();   // [panel] lasd fent
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_DAC_Init();
  MX_TIM2_Init();
  MX_TIM10_Init();
  /* USER CODE BEGIN 2 */

  OLED_Init();
  Motor_init();

  char display_char[20];
  set_speed_percent = 0;
  uint8_t fel_elozo = 0;      // [panel] a fel es a le gomb elozo allapota (csak a lenyomaskor lep)
  uint8_t le_elozo = 0;
  uint32_t kijelzo_ido = 0;   // [panel] mikor frissult utoljara a kijelzo
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /* [panel] Uzemmod a kapcsolokbol: SW1 = b (max), SW3 = e (PWM), SW4 = i (VREF), kulonben a (ki) */
	  Motor_setOpModeFromSwitches();

	  /* [panel] A gombok: egy lenyomas = egy lepes (a Nucleos valtozat addig lepett, amig nyomtad) */
	  uint8_t fel = HAL_GPIO_ReadPin(HMI_BTN_1_GPIO_Port, HMI_BTN_1_Pin);
	  uint8_t le = HAL_GPIO_ReadPin(HMI_BTN_5_GPIO_Port, HMI_BTN_5_Pin);

	  /* Increase speed by pushing Button 1 (fel) */
	  if (fel && !fel_elozo)
	  {
		  set_speed_percent += 10;
	  }

	  /* Decrease speed by pushing Button 5 (le) */
	  if (le && !le_elozo)
	  {
		  set_speed_percent -= 10;
	  }

	  /* Stop motor by pushing Button 3 (kozep) */
	  if (HAL_GPIO_ReadPin(HMI_BTN_3_GPIO_Port, HMI_BTN_3_Pin))
	  {
		  set_speed_percent = 0;
	  }
	  fel_elozo = fel;
	  le_elozo = le;

	  if (set_speed_percent > 100)
	  {
		  set_speed_percent = 100;
	  }
	  else if (set_speed_percent < 0)
	  {
		  set_speed_percent = 0;   // [panel] csak elore (a RIN-t nem vezereljuk)
	  }

	  /* Set the motor speed */
	  Motor_setSpeedPercent(set_speed_percent);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  /* [panel] A kijelzo frissitese az I2C-n kb. 0,1 s, ezert csak 0,2 s-onkent rajzolunk,
	   * a gombokat viszont 10 ms-onkent nezzuk. (A Nucleos grafikon itt nincs.) */
	  if (HAL_GetTick() - kijelzo_ido >= 200)
	  {
		  kijelzo_ido = HAL_GetTick();
		  OLED_Fill(Black);

		  OLED_SetCursor(2, 0);
		  OLED_WriteString("MOTOR", Font_11x18, White);

		  sprintf(display_char, "SW %d%d%d%d", (switch_state & 1) ? 1 : 0, (switch_state & 2) ? 1 : 0,
				  (switch_state & 4) ? 1 : 0, (switch_state & 8) ? 1 : 0);
		  OLED_SetCursor(72, 4);
		  OLED_WriteString(display_char, Font_7x10, White);

		  switch (motor_mode)
		  {
			  case MOTOR_MODE_B: sprintf(display_char, "b max"); break;
			  case MOTOR_MODE_E: sprintf(display_char, "e PWM %ld%%", set_speed_percent); break;
			  case MOTOR_MODE_I: sprintf(display_char, "i VREF %ld%%", set_speed_percent); break;
			  default:           sprintf(display_char, "a ki (%ld%%)", set_speed_percent); break;
		  }
		  OLED_SetCursor(2, 20);
		  OLED_WriteString(display_char, Font_7x10, White);

		  sprintf(display_char, "EncPos %ld", encoder_position_act);
		  OLED_SetCursor(2, 30);
		  OLED_WriteString(display_char, Font_7x10, White);

		  sprintf(display_char, "EncRPM %ld", (int32_t)encoder_speed_rpm);
		  OLED_SetCursor(2, 40);
		  OLED_WriteString(display_char, Font_7x10, White);

		  sprintf(display_char, "OutRPM %ld", (int32_t)output_speed_rpm);
		  OLED_SetCursor(2, 50);
		  OLED_WriteString(display_char, Font_7x10, White);

		  OLED_UpdateScreen();
	  }
	  HAL_Delay(10);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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
  // [panel] Ha valami nem indul el: mind a 4 LED egyutt villog
  __disable_irq();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = HMI_LED_1_Pin | HMI_LED_2_Pin | HMI_LED_3_Pin | HMI_LED_4_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOD, &gpio);
  while (1)
  {
    HAL_GPIO_TogglePin(GPIOD, gpio.Pin);
    for (volatile uint32_t i = 0; i < 3000000; i++);   // kb. 0,2 s (itt a HAL_Delay nem megy: tiltva a megszakitas)
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
