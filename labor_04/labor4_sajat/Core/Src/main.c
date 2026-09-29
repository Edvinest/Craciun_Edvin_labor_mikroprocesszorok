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
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// Enumeration for state machine
typedef enum
{
    OLED_STATE_TETRIS = 0x00,       // Tetris animation
    OLED_STATE_JOHNDOE = 0x01,      // JOHN DOE + Microprocessors text
} OLED_Test_state;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BLOCK_SIZE 8
#define GRID_WIDTH 16
#define GRID_HEIGHT 8
#define DROP_INTERVAL_MS 500
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
OLED_Test_state test_state = OLED_STATE_TETRIS;

GPIO_PinState previous_button_state[3] = {0u, 0u, 0u};
GPIO_PinState current_button_state[3] = {0u, 0u, 0u};

uint32_t drop_counter = 0;

// Tetromino shapes (4x4 matrix, each cell is 1 bit)
const uint8_t tetrominos[7][16] = {
    {1,1,1,1,  0,0,0,0,  0,0,0,0,  0,0,0,0},  // I
    {1,1,1,0,  0,1,0,0,  0,0,0,0,  0,0,0,0},  // J
    {1,1,1,0,  1,0,0,0,  0,0,0,0,  0,0,0,0},  // L
    {1,1,0,0,  1,1,0,0,  0,0,0,0,  0,0,0,0},  // O
    {0,1,1,0,  1,1,0,0,  0,0,0,0,  0,0,0,0},  // S
    {0,1,0,0,  1,1,1,0,  0,0,0,0,  0,0,0,0},  // T
    {0,1,1,0,  0,1,1,0,  0,0,0,0,  0,0,0,0}   // Z
};

typedef struct {
    uint8_t shape_index;
    uint8_t col;
    uint8_t row;
} FallingBlock;

FallingBlock current_block = {0};
FallingBlock locked_blocks[GRID_HEIGHT][GRID_WIDTH];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// Helper function declarations
void DrawBlock(uint8_t col, uint8_t row, uint8_t color);
void DrawTetromino(FallingBlock* block, uint8_t color);
void MoveDown(void);
uint8_t IsRowFull(uint8_t row);
void ClearBottomRow(uint8_t row);
void RenderLockedBlocks(void);
void ResetGameBoard(void);
void OLED_TestJohnDoe(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Draw a single block at grid position
void DrawBlock(uint8_t col, uint8_t row, uint8_t color)
{
    uint8_t px = col * BLOCK_SIZE;
    uint8_t py = row * BLOCK_SIZE;
    OLED_DrawRectangle(px, py, px + BLOCK_SIZE - 1, py + BLOCK_SIZE - 1, color);
}

// Draw entire tetromino at current position
void DrawTetromino(FallingBlock* block, uint8_t color)
{
    const uint8_t* shape = tetrominos[block->shape_index];
    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            if (shape[r * 4 + c])
            {
                DrawBlock(block->col + c, block->row + r, color);
            }
        }
    }
}

// Move current block down
void MoveDown(void)
{
    if (current_block.row < GRID_HEIGHT - 1)
    {
        current_block.row++;
    }
    else
    {
        // Lock the block
        const uint8_t* shape = tetrominos[current_block.shape_index];
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                if (shape[r * 4 + c])
                {
                    uint8_t gc = current_block.col + c;
                    uint8_t gr = current_block.row + r;

                    if (gc < GRID_WIDTH && gr < GRID_HEIGHT)
                    {
                        locked_blocks[gr][gc].shape_index = current_block.shape_index;
                        locked_blocks[gr][gc].col = gc;
                        locked_blocks[gr][gc].row = gr;
                    }
                }
            }
        }

        // Spawn new block
        current_block.shape_index = rand() % 7;
        current_block.col = GRID_WIDTH / 2 - 2;
        current_block.row = 0;
    }
}

// Check if row is full
uint8_t IsRowFull(uint8_t row)
{
    for (int c = 0; c < GRID_WIDTH; c++)
    {
        if (locked_blocks[row][c].shape_index == 255) return 0;
    }
    return 1;
}

// Clear full row and shift down
void ClearBottomRow(uint8_t row)
{
    for (int c = 0; c < GRID_WIDTH; c++)
    {
        locked_blocks[row][c].shape_index = 255;  // Mark empty
    }

    // Shift everything down
    for (int r = row; r > 0; r--)
    {
        for (int c = 0; c < GRID_WIDTH; c++)
        {
            locked_blocks[r][c] = locked_blocks[r-1][c];
        }
    }
}

// Render all locked blocks
void RenderLockedBlocks(void)
{
    for (int r = 0; r < GRID_HEIGHT; r++)
    {
        for (int c = 0; c < GRID_WIDTH; c++)
        {
            if (locked_blocks[r][c].shape_index != 255)
            {
                DrawBlock(c, r, WHITE);
            }
        }
    }
}

// Reset game board
void ResetGameBoard(void)
{
    for (int r = 0; r < GRID_HEIGHT; r++)
    {
        for (int c = 0; c < GRID_WIDTH; c++)
        {
            locked_blocks[r][c].shape_index = 255;  // Empty marker
        }
    }
    current_block.shape_index = rand() % 7;
    current_block.col = GRID_WIDTH / 2 - 2;
    current_block.row = 0;
    drop_counter = 0;
}

// JOHN DOE + Microprocessors text display
void OLED_TestJohnDoe(void)
{
    // Clear screen
    OLED_Fill(BLACK);

    // Draw "JOHN DOE" centered near top
    OLED_SetCursor(14, 15);
    OLED_WriteString("CRACIUN EDVIN", Font_11x18, WHITE);

    // Draw "Microprocessors" below it
    OLED_SetCursor(11, 15 + 18 + 5);
    OLED_WriteString("Microprocessors", Font_7x10, WHITE);

    return;
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
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  OLED_Init();
  ResetGameBoard();  // Initialize Tetris game board

  HAL_Delay(1000);
  OLED_Fill(BLACK);
  OLED_UpdateScreen();
  HAL_Delay(1000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Button handling - read the button states
    current_button_state[0] = HAL_GPIO_ReadPin(HMI_BTN_2_GPIO_Port, HMI_BTN_2_Pin);
    current_button_state[1] = HAL_GPIO_ReadPin(HMI_BTN_3_GPIO_Port, HMI_BTN_3_Pin);
    current_button_state[2] = HAL_GPIO_ReadPin(HMI_BTN_4_GPIO_Port, HMI_BTN_4_Pin);

    // Left button - previous state
    if ((current_button_state[0] != previous_button_state[0]) && (GPIO_PIN_RESET == current_button_state[0]))
    {
        if(test_state == OLED_STATE_TETRIS)
        {
            test_state = OLED_STATE_JOHNDOE;
        }
        else
        {
            test_state = OLED_STATE_TETRIS;
        }

        // Cleanup the screen
        OLED_Fill(BLACK);
        OLED_UpdateScreen();
    }

    // Right button - next state
    if ((current_button_state[2] != previous_button_state[2]) && (GPIO_PIN_RESET == current_button_state[2]))
    {
        if(test_state == OLED_STATE_TETRIS)
        {
            test_state = OLED_STATE_JOHNDOE;
        }
        else
        {
            test_state = OLED_STATE_TETRIS;
        }

        // Cleanup the screen
        OLED_Fill(BLACK);
        OLED_UpdateScreen();
    }

    // Save the current button states
    previous_button_state[0] = current_button_state[0];
    previous_button_state[1] = current_button_state[1];
    previous_button_state[2] = current_button_state[2];

    // Select OLED test function
    switch (test_state)
    {
    case OLED_STATE_TETRIS:     // Tetris animation
        // Clear old frame
        for (int r = 0; r < GRID_HEIGHT; r++)
        {
            for (int c = 0; c < GRID_WIDTH; c++)
            {
                DrawBlock(c, r, BLACK);
            }
        }

        // Draw locked blocks
        RenderLockedBlocks();

        // Draw current falling block
        DrawTetromino(&current_block, WHITE);

        // Drop timer
        drop_counter += 50;  // 50ms per loop iteration
        if (drop_counter >= DROP_INTERVAL_MS)
        {
            drop_counter = 0;
            MoveDown();

            // Check for full rows (simple version - check bottom)
            for (int r = GRID_HEIGHT - 1; r >= 0; r--)
            {
                if (IsRowFull(r))
                {
                    ClearBottomRow(r);
                    break;
                }
            }
        }
        break;

    case OLED_STATE_JOHNDOE:    // JOHN DOE + Microprocessors
        OLED_TestJohnDoe();
        break;

    default:
        test_state = OLED_STATE_TETRIS;
        break;
    }

    OLED_UpdateScreen();
    HAL_Delay(50);

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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
