/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "cmsis_os.h"
#include "portmacro.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS_CLI.h"
#include <stdint.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

typedef struct led_params_t_ {
  GPIO_TypeDef *led_port;
  uint16_t led_pin;
  TickType_t led_toggle_time;
} led_params_t;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef hlpuart1;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */
SemaphoreHandle_t sem = NULL;
SemaphoreHandle_t mutex = NULL;
TimerHandle_t debounce;
QueueHandle_t queue_keyb;
QueueHandle_t queue_tx;
QueueHandle_t queue_rx;

volatile uint32_t counter = 0;

#define MAX_INPUT_LENGTH 50
#define MAX_OUTPUT_LENGTH 100

static const int8_t *const pcWelcomeMessage =
    "FreeRTOS command server.rnType Help to view a list of registered "
    "commands.rn\n";

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(sem, &pxHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
  xQueueSendToBackFromISR(queue_rx, huart->pRxBuffPtr,
                          &pxHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void transmite_uart(uint8_t *string, uint32_t len) {
  if (xSemaphoreTake(mutex, 100) == pdTRUE) {
    if (HAL_UART_Transmit_IT(&hlpuart1, string, len) == HAL_OK) {
      xSemaphoreTake(sem, portMAX_DELAY);
    }
    xSemaphoreGive(mutex);
  }
}

void uart_transmit(char *str, uint32_t len) {
  if (xSemaphoreTake(mutex, 100) == pdTRUE) {
    if (HAL_UART_Transmit_IT(&hlpuart1, (uint8_t *)str, len) == HAL_OK) {
      xSemaphoreTake(sem, portMAX_DELAY);
    }
    xSemaphoreGive(mutex);
  }
}

void receive_uart(uint8_t *data) {
  xQueueReceive(queue_rx, data, portMAX_DELAY);
}

void echo(void *param) {
  uint8_t data;
  uint8_t data_rx;
  HAL_UART_Receive_IT(&hlpuart1, &data, 1);
  while (1) {
    receive_uart(&data_rx);
    if (data_rx == '\r') {
      transmite_uart(&data_rx, 1);
      data_rx = '\n';
      transmite_uart(&data_rx, 1);
    } else {
      transmite_uart(&data_rx, 1);
    }
  }
}

volatile uint32_t cnt1 = 0;
void envia_serial_1(void *param) {
  char *texto = "Olá mundo da tarefa 1!\n\r";
  uint32_t len = strlen(texto);
  while (1) {
    if (xSemaphoreTake(mutex, 100) == pdTRUE) {
      if (HAL_UART_Transmit_IT(&hlpuart1, (uint8_t *)texto, len) == HAL_OK) {
        xSemaphoreTake(sem, portMAX_DELAY);
        cnt1++;
      }
      xSemaphoreGive(mutex);
    }
    taskYIELD();
  }
}

volatile uint16_t ctn = 0;
void send_serial(void *param) {
  char *texto = (char *)param;
  uint32_t len = strlen(texto);
  while (1) {
    if (xSemaphoreTake(mutex, 100) == pdTRUE) {
      if (HAL_UART_Transmit_IT(&hlpuart1, (uint8_t *)texto, len) == HAL_OK) {
        xSemaphoreTake(sem, portMAX_DELAY);
        ctn++;
      }
      xSemaphoreGive(mutex);
    }
    taskYIELD();
  }
}

void print_serial(void *param) {
  char msg[64];

  while (1) {
    // Wait for message from queue
    if (xQueueReceive(queue_tx, &msg, portMAX_DELAY) == pdTRUE) {
      uart_transmit(msg, strlen(msg));
    }
  }
}

void vCommandConsoleTask(void *pvParameters) {
  int8_t cRxedChar, cInputIndex = 0;
  BaseType_t xMoreDataToFollow;
  /* The input and output buffers are declared static to keep them off the
   * stack. */
  static int8_t pcOutputString[MAX_OUTPUT_LENGTH],
      pcInputString[MAX_INPUT_LENGTH];

  /* This code assumes the peripheral being used as the console has already
     been opened and configured, and is passed into the task as the task
     parameter. Cast the task parameter to the correct type. */

  /* Send a welcome message to the user knows they are connected. */
  transmite_uart(pcWelcomeMessage, strlen(pcWelcomeMessage));

  for (;;) {
    /* This implementation reads a single character at a time. Wait in the
       Blocked state until a character is received. */
    HAL_UART_Receive_IT(&hlpuart1, &cRxedChar, 1);

    receive_uart(&cRxedChar);

    if (cRxedChar == '\r') {
      transmite_uart(&cRxedChar, 1);
    } else {
      transmite_uart(&cRxedChar, 1);
    }
    
    if (cRxedChar == '\r') {
      /* A newline character was received, so the input command string is
         complete and can be processed. Transmit a line separator, just to
         make the output easier to read. */
      transmite_uart("\r\n", strlen("\r\n"));

      /* The command interpreter is called repeatedly until it returns
         pdFALSE. See the "Implementing a command" documentation for an
         exaplanation of why this is. */
      do {
        /* Send the command string to the command interpreter. Any
           output generated by the command interpreter will be placed in the
           pcOutputString buffer. */
        xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
            pcInputString,    /* The command string.*/
            pcOutputString,   /* The output buffer. */
            MAX_OUTPUT_LENGTH /* The size of the output buffer. */
        );

        /* Write the output generated by the command interpreter to the
           console. */
        transmite_uart(pcOutputString, strlen(pcOutputString));

      } while (xMoreDataToFollow != pdFALSE);

      /* All the strings generated by the input command have been sent.
         Processing of the command is complete. Clear the input string ready
         to receive the next command. */
      cInputIndex = 0;
      memset(pcInputString, 0x00, MAX_INPUT_LENGTH);
    } else {
      /* The if() clause performs the processing after a newline character
         is received. This else clause performs the processing if any other
         character is received. */

      if (cRxedChar == '\r') {
        /* Ignore carriage returns. */
      } else if (cRxedChar == '\b') {
        /* Backspace was pressed. Erase the last character in the input
           buffer - if there are any. */
        if (cInputIndex > 0) {
          cInputIndex--;
          pcInputString[cInputIndex] = ' ';
        }
      } else {
        /* A character was entered. It was not a new line, backspace
           or carriage return, so it is accepted as part of the input and
           placed into the input buffer. When a n is entered the complete
           string will be passed to the command interpreter. */
        if (cInputIndex < MAX_INPUT_LENGTH) {
          pcInputString[cInputIndex] = cRxedChar;
          cInputIndex++;
        }
      }
    }
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
  HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
  xTimerStartFromISR(debounce, &pxHigherPriorityTaskWoken);

  HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

  const char *string = "Hello from IRQ\r\n";

  xQueueSendFromISR(queue_tx, string, &pxHigherPriorityTaskWoken);

  portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void debounce_callback(TimerHandle_t xTimer) {
  uint8_t buffer = 1;
  xQueueSendToBack(queue_keyb, &buffer, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_LPUART1_UART_Init(void);
void StartDefaultTask(void const *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_LPUART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  mutex = xSemaphoreCreateMutex();
  sem = xSemaphoreCreateBinary();
  debounce =
      xTimerCreate("debounce do teclado", 50, pdFALSE, NULL, debounce_callback);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  queue_keyb = xQueueCreate(16, sizeof(char));
  queue_tx = xQueueCreate(8, sizeof(char[64]));
  queue_rx = xQueueCreate(16, sizeof(uint8_t));

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  // static led_params_t led1 = {
  //   LED_GPIO_Port,
  //    LED_Pin,
  //    500
  // };

  // char * string = "Hello from task1\n\r";
  // char * string2 = "Hello from task2\n\r";
  // xTaskCreate(blink_led, "Blink LED", 256, &led1, 4, NULL);
  // xTaskCreate(send_serial, "Serial1",256, string, 3, NULL);
  // xTaskCreate(send_serial, "Serial1",256, string2, 3, NULL);
  // xTaskCreate(envia_serial_1, "serial 1", 256, NULL, 6, NULL);
  xTaskCreate(print_serial, "Print Serial", 256, NULL, 3, NULL);
  // xTaskCreate(echo, "echo serial", 256, NULL, 5, NULL);
  xTaskCreate(vCommandConsoleTask, "Shell", 1024, NULL, 4, NULL);

  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV6;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief LPUART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_LPUART1_UART_Init(void) {

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const *argument) {
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for (;;) {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM7 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
