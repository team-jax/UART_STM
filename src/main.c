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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cmsis_os2.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  UART_RX_WAIT_HEADER = 0,
  UART_RX_WAIT_ID,
  UART_RX_WAIT_ANGLE_H,
  UART_RX_WAIT_ANGLE_L,
  UART_RX_WAIT_CHECKSUM
} UartRxState_t;

typedef struct
{
  uint8_t motor_id;
  float   angle;
} ServoCmd_t;

/* UART 수신 상태머신 컨텍스트 */
typedef struct
{
  uint8_t       byte;    /* HAL_UART_Receive_IT 1바이트 수신 버퍼 */
  UartRxState_t state;
  uint8_t       motorId;
  uint8_t       angleH;
  uint8_t       angleL;
} UartRxCtx_t;

/* 손가락 채널 1개의 타이머/채널 + 캘리브레이션 각도 */
typedef struct
{
  TIM_HandleTypeDef *tim;
  uint32_t           channel;
  float              openAngle;
  float              gripAngle;
} FingerChannel_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_PKT_HEADER      0xAAU
#define SERVO_QUEUE_LEN      16U

/* 프레임 수신 응답: 정상 = ACK, checksum/ID 불량 = NAK (연결 확인용) */
#define UART_RESP_ACK        0x55U
#define UART_RESP_NAK        0xEEU

#define SERVO_ANGLE_MIN      0.0f
#define SERVO_ANGLE_MAX      180.0f
#define SERVO_CCR_MIN        500U
#define SERVO_CCR_MAX        2500U

/* MG996R : TIM2 */
#define MOTOR_SHOULDER_ROT   0U   /* TIM2_CH1 / PA15 (PA0은 USER 버튼용으로 양보) */
#define MOTOR_WRIST_1        1U   /* TIM2_CH2 / PA1 */
#define MOTOR_WRIST_2        2U   /* TIM2_CH3 / PA2 */
#define MOTOR_WRIST_3        3U   /* TIM2_CH4 / PA3 */

/* MG90S : TIM3 / TIM4 */
#define MOTOR_FINGER1_A      4U   /* TIM3_CH1 / PA6 */
#define MOTOR_FINGER1_B      5U   /* TIM3_CH2 / PA7 */
#define MOTOR_FINGER2_A      6U   /* TIM3_CH3 / PB0 */
#define MOTOR_FINGER2_B      7U   /* TIM3_CH4 / PB1 */
#define MOTOR_FINGER3_A      8U   /* TIM4_CH1 / PB6 */
#define MOTOR_FINGER3_B      9U   /* TIM4_CH2 / PB7 */
#define MOTOR_FINGER_ROT     10U  /* TIM4_CH3 / PB8 */
#define MOTOR_ID_MAX         MOTOR_FINGER_ROT

/* 제스처 명령: [0xAA][0xF0][0x00][code][checksum], code: 0x00=OPEN, 0x01=GRIP */
#define GESTURE_CMD_ID       0xF0U
#define GESTURE_CODE_OPEN    0x00U
#define GESTURE_CODE_GRIP    0x01U

/* 손가락 제스처 기본 각도 — 손가락별 실제 값은 아래 fingerTable에서 개별 조정 */
#define FINGER_OPEN_ANGLE    180.0f
#define FINGER_GRIP_ANGLE    30.0f

/* 홈잉: 부팅 후 손가락을 하나씩 OPEN 위치로 이동 (동시 기동 시 돌입 전류 방지) */
#define HOMING_START_DELAY_MS   500U   /* 전원 안정화 대기 */
#define HOMING_STEP_DELAY_MS    150U   /* 손가락 사이 간격 */

/* Discovery 보드: USER 버튼(PA0, 누르면 High) = GRIP/OPEN 토글, LED(PD12) = 명령 수신 표시 */
#define BUTTON_PORT          GPIOA
#define BUTTON_PIN           GPIO_PIN_0
#define LED_PORT             GPIOD
#define LED_PIN              GPIO_PIN_12
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
UART_HandleTypeDef huart1;

static osMessageQueueId_t xServoQueueHandle;
static osThreadId_t       pwmCtrlTaskHandle;
static osThreadId_t       buttonTaskHandle;

static UartRxCtx_t uartCtx1 = { .state = UART_RX_WAIT_HEADER };

/* ★ 손가락별 캘리브레이션 테이블 — 서보마다 가동 범위가 다르므로 여기 숫자만 고치면 됨.
 *   {타이머, 채널, OPEN 각도, GRIP 각도} 순. 부팅 홈잉과 제스처가 모두 이 표를 사용. */
static const FingerChannel_t fingerTable[] = {
  { &htim3, TIM_CHANNEL_1, 90.0f, 170.0f },   /* FINGER1_A / PA6 */
  { &htim3, TIM_CHANNEL_2, 100.0f, 179.9f },   /* FINGER1_B / PA7 */
  { &htim3, TIM_CHANNEL_3, 100.0f, 180.0f },   /* FINGER2_A / PB0 */
  { &htim3, TIM_CHANNEL_4, 105.0f, 180.0f },   /* FINGER2_B / PB1 */
  { &htim4, TIM_CHANNEL_1, 85.0f, 165.0f },   /* FINGER3_A / PB6 */
  { &htim4, TIM_CHANNEL_2, 22.8f, 109.8f },   /* FINGER3_B / PB7 */
};
#define FINGER_CHANNEL_COUNT  (sizeof(fingerTable) / sizeof(fingerTable[0]))
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
static void     MX_USART1_UART_Init(void);
static uint32_t Angle_to_CCR(float angle);
static void     PWM_Control_Task(void *argument);
static void     Button_Gesture_Task(void *argument);
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
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  MX_USART1_UART_Init();

  /* 부팅 표시: LED 3회 점멸 (새 펌웨어가 실행 중임을 눈으로 확인) */
  for (int i = 0; i < 3; ++i)
  {
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
  }

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);

  osKernelInitialize();

  xServoQueueHandle = osMessageQueueNew(SERVO_QUEUE_LEN, sizeof(ServoCmd_t), NULL);
  if (xServoQueueHandle == NULL)
  {
    Error_Handler();
  }

  {
    const osThreadAttr_t pwmTaskAttr = {
      .name       = "PWM_CTRL",
      .priority   = osPriorityNormal,
      .stack_size = 512
    };
    pwmCtrlTaskHandle = osThreadNew(PWM_Control_Task, NULL, &pwmTaskAttr);
    if (pwmCtrlTaskHandle == NULL)
    {
      Error_Handler();
    }
  }

  if (HAL_UART_Receive_IT(&huart1, &uartCtx1.byte, 1) != HAL_OK)
  {
    Error_Handler();
  }

  {
    const osThreadAttr_t btnTaskAttr = {
      .name       = "BTN_GESTURE",
      .priority   = osPriorityBelowNormal,
      .stack_size = 512
    };
    buttonTaskHandle = osThreadNew(Button_Gesture_Task, NULL, &btnTaskAttr);
    if (buttonTaskHandle == NULL)
    {
      Error_Handler();
    }
  }

  osKernelStart();
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 15;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 15;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 19999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 15;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 19999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  __HAL_RCC_GPIOD_CLK_ENABLE();

  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* USER 버튼: 누르면 High (Discovery 보드는 외부 풀다운 있음, 내부 풀다운은 보험) */
    GPIO_InitStruct.Pin   = BUTTON_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

    /* 상태 표시 LED */
    GPIO_InitStruct.Pin   = LED_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
  }
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void MX_USART1_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* PA9 = TX, PA10 = RX */
  GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 115200;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static uint32_t Angle_to_CCR(float angle)
{
  if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
  if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

  return (uint32_t)(SERVO_CCR_MIN +
         (angle / SERVO_ANGLE_MAX) * (float)(SERVO_CCR_MAX - SERVO_CCR_MIN));
}

/* TXE 폴링 후 1바이트 송신 — ISR에서 사용 (HAL tick에 의존하지 않음) */
static void UART_Send_Byte(USART_TypeDef *usart, uint8_t byte)
{
  for (volatile uint32_t i = 0U; i < 20000U; ++i)
  {
    if ((usart->SR & USART_SR_TXE) != 0U)
    {
      usart->DR = byte;
      return;
    }
  }
}

/* UART 1바이트 수신 ISR: [0xAA][motor_id][angle_H][angle_L][checksum] 프레임 동기화 후 Queue 전달.
 * 정상 프레임 수신 시 ACK(0x55), checksum/ID 불량 시 NAK(0xEE)를 수신한 UART로 회신. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  UartRxCtx_t *ctx;

  if (huart->Instance == USART1) { ctx = &uartCtx1; }
  else                           { return; }

  switch (ctx->state)
  {
    case UART_RX_WAIT_HEADER:
      if (ctx->byte == UART_PKT_HEADER)
      {
        ctx->state = UART_RX_WAIT_ID;
      }
      break;

    case UART_RX_WAIT_ID:
      ctx->motorId = ctx->byte;
      ctx->state   = UART_RX_WAIT_ANGLE_H;
      break;

    case UART_RX_WAIT_ANGLE_H:
      ctx->angleH = ctx->byte;
      ctx->state  = UART_RX_WAIT_ANGLE_L;
      break;

    case UART_RX_WAIT_ANGLE_L:
      ctx->angleL = ctx->byte;
      ctx->state  = UART_RX_WAIT_CHECKSUM;
      break;

    case UART_RX_WAIT_CHECKSUM:
    {
      uint8_t checksum = (uint8_t)(ctx->motorId + ctx->angleH + ctx->angleL);

      if ((checksum == ctx->byte) &&
          ((ctx->motorId <= MOTOR_ID_MAX) || (ctx->motorId == GESTURE_CMD_ID)))
      {
        ServoCmd_t cmd;
        uint16_t angle_x10 = ((uint16_t)ctx->angleH << 8) | ctx->angleL;

        cmd.motor_id = ctx->motorId;
        if (ctx->motorId == GESTURE_CMD_ID)
        {
          cmd.angle = (float)angle_x10;          /* 제스처 코드(0/1)를 그대로 전달 */
        }
        else
        {
          cmd.angle = (float)angle_x10 / 10.0f;
        }

        /* ISR 컨텍스트: CMSIS-RTOS2(FreeRTOS) 래퍼가 자동으로 FromISR API 사용 */
        osMessageQueuePut(xServoQueueHandle, &cmd, 0, 0);
        UART_Send_Byte(huart->Instance, UART_RESP_ACK);
      }
      else
      {
        UART_Send_Byte(huart->Instance, UART_RESP_NAK);
      }
      ctx->state = UART_RX_WAIT_HEADER;
      break;
    }

    default:
      ctx->state = UART_RX_WAIT_HEADER;
      break;
  }

  /* 다음 1바이트 수신 재시작 */
  HAL_UART_Receive_IT(huart, &ctx->byte, 1);
}

/* Queue에서 서보 명령을 꺼내 해당 채널 CCR을 갱신 */
static void PWM_Control_Task(void *argument)
{
  ServoCmd_t cmd;
  uint32_t   ccr;

  /* 홈잉: 손가락을 하나씩 OPEN 위치로 이동 (기준 자세 확립 + 돌입 전류 분산).
   * 손목/어깨는 펄스 없음 유지 — 부팅 시 팔이 갑자기 움직이지 않도록. */
  osDelay(HOMING_START_DELAY_MS);
  for (uint32_t i = 0U; i < FINGER_CHANNEL_COUNT; ++i)
  {
    __HAL_TIM_SET_COMPARE(fingerTable[i].tim, fingerTable[i].channel,
                          Angle_to_CCR(fingerTable[i].openAngle));
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);   /* 홈잉 진행 표시 */
    osDelay(HOMING_STEP_DELAY_MS);
  }
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);

  for (;;)
  {
    if (osMessageQueueGet(xServoQueueHandle, &cmd, NULL, osWaitForever) == osOK)
    {
      HAL_GPIO_TogglePin(LED_PORT, LED_PIN);   /* 명령 수신 표시 */
      ccr = Angle_to_CCR(cmd.angle);

      switch (cmd.motor_id)
      {
        /* MG996R */
        case MOTOR_SHOULDER_ROT: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr); break;
        case MOTOR_WRIST_1:      __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, ccr); break;
        case MOTOR_WRIST_2:      __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, ccr); break;
        case MOTOR_WRIST_3:      __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, ccr); break;

        /* MG90S */
        case MOTOR_FINGER1_A:    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr); break;
        case MOTOR_FINGER1_B:    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, ccr); break;
        case MOTOR_FINGER2_A:    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, ccr); break;
        case MOTOR_FINGER2_B:    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, ccr); break;
        case MOTOR_FINGER3_A:    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, ccr); break;
        case MOTOR_FINGER3_B:    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, ccr); break;
        case MOTOR_FINGER_ROT:   __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, ccr); break;

        /* 제스처: 손가락 서보 6개 일괄 구동 — 채널별 캘리브레이션 각도 사용 (FINGER_ROT/손목/어깨는 유지) */
        case GESTURE_CMD_ID:
        {
          uint16_t code = (uint16_t)cmd.angle;

          if ((code != GESTURE_CODE_GRIP) && (code != GESTURE_CODE_OPEN))
          {
            break;   /* 정의되지 않은 제스처 코드는 무시 */
          }

          for (uint32_t i = 0U; i < FINGER_CHANNEL_COUNT; ++i)
          {
            float a = (code == GESTURE_CODE_GRIP) ? fingerTable[i].gripAngle
                                                  : fingerTable[i].openAngle;
            __HAL_TIM_SET_COMPARE(fingerTable[i].tim, fingerTable[i].channel, Angle_to_CCR(a));
          }
          break;
        }

        default: break;
      }
    }
  }
}

/* USER 버튼(PA0) 폴링: 누를 때마다 GRIP ↔ OPEN 토글 (UART 없이 제스처 테스트용) */
static void Button_Gesture_Task(void *argument)
{
  uint8_t pressedPrev = 0U;
  uint8_t gripNext    = 1U;   /* 다음 누름에 보낼 제스처: 1=GRIP, 0=OPEN */

  (void)argument;

  for (;;)
  {
    uint8_t pressed = (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_SET) ? 1U : 0U;

    if ((pressed == 1U) && (pressedPrev == 0U))
    {
      ServoCmd_t cmd;

      cmd.motor_id = GESTURE_CMD_ID;
      cmd.angle    = gripNext ? (float)GESTURE_CODE_GRIP : (float)GESTURE_CODE_OPEN;
      osMessageQueuePut(xServoQueueHandle, &cmd, 0, 0);
      gripNext = (uint8_t)!gripNext;
    }
    pressedPrev = pressed;
    osDelay(30);   /* 30 ms 폴링 = 디바운스 겸용 */
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
