# STM32F407 FreeRTOS 코드 가이드
> 휴머노이드 상체 프로젝트 - UART 수신 + PWM 서보 제어

---

## 1. 전체 태스크 구조

```
[Jetson Nano]
     │
   UART
     │
[STM32F407]
     ├── Task1: UART_RX_Task   → Jetson에서 각도값 수신
     ├── Task2: PWM_MG996R_Task → MG996R 4개 제어
     └── Task3: PWM_MG90S_Task  → MG90S 7개 제어

태스크끼리 Queue로 데이터 전달
```

---

## 2. 태스크 우선순위

| 태스크 | 우선순위 | 이유 |
|---|---|---|
| UART_RX_Task | 높음 (3) | 명령 놓치면 안 됨 |
| PWM_MG996R_Task | 보통 (2) | 관절 제어 |
| PWM_MG90S_Task | 보통 (2) | 손가락 제어 |

---

## 3. 데이터 구조 정의

```c
// 서보 각도 데이터 구조체
typedef struct {
    uint8_t motor_id;   // 모터 번호 (0~10)
    float   angle;      // 각도 (0.0 ~ 180.0)
} ServoCmd_t;

// Queue 핸들
QueueHandle_t xServoQueue;
```

---

## 4. 모터 ID 정의

```c
// MG996R (0~3)
#define MOTOR_SHOULDER_ROT  0   // 상완 회전
#define MOTOR_WRIST_1       1   // 손목 축1
#define MOTOR_WRIST_2       2   // 손목 축2
#define MOTOR_WRIST_3       3   // 손목 축3

// MG90S (4~10)
#define MOTOR_FINGER1_A     4   // 손가락1 서보A
#define MOTOR_FINGER1_B     5   // 손가락1 서보B
#define MOTOR_FINGER2_A     6   // 손가락2 서보A
#define MOTOR_FINGER2_B     7   // 손가락2 서보B
#define MOTOR_FINGER3_A     8   // 손가락3 서보A
#define MOTOR_FINGER3_B     9   // 손가락3 서보B
#define MOTOR_FINGER_ROT    10  // 손가락 회전
```

---

## 5. 각도 → CCR 변환 함수

```c
// 각도를 CCR 값으로 변환 (0도=500, 90도=1500, 180도=2500)
uint32_t Angle_to_CCR(float angle) {
    if (angle < 0.0f)   angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    return (uint32_t)(500.0f + (angle / 180.0f) * 2000.0f);
}
```

---

## 6. UART 수신 태스크

```c
// UART 프로토콜: [모터ID(1byte)][각도*10 정수값(2byte)] = 3byte
// 예: 모터0번 90도 → [0x00][0x03][0x84]

uint8_t rxBuf[3];

void UART_RX_Task(void *argument) {
    ServoCmd_t cmd;

    while(1) {
        // 3바이트 수신 대기
        if (HAL_UART_Receive(&huart1, rxBuf, 3, 100) == HAL_OK) {
            cmd.motor_id = rxBuf[0];
            cmd.angle    = (float)((rxBuf[1] << 8) | rxBuf[2]) / 10.0f;

            // Queue에 명령 넣기
            xQueueSend(xServoQueue, &cmd, 0);
        }
        osDelay(5);
    }
}
```

---

## 7. PWM 제어 태스크

```c
void PWM_Control_Task(void *argument) {
    ServoCmd_t cmd;
    uint32_t ccr;

    while(1) {
        // Queue에서 명령 꺼내기
        if (xQueueReceive(xServoQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            ccr = Angle_to_CCR(cmd.angle);

            switch(cmd.motor_id) {
                // MG996R
                case MOTOR_SHOULDER_ROT:
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr); break;
                case MOTOR_WRIST_1:
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, ccr); break;
                case MOTOR_WRIST_2:
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, ccr); break;
                case MOTOR_WRIST_3:
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, ccr); break;

                // MG90S
                case MOTOR_FINGER1_A:
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr); break;
                case MOTOR_FINGER1_B:
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, ccr); break;
                case MOTOR_FINGER2_A:
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, ccr); break;
                case MOTOR_FINGER2_B:
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, ccr); break;
                case MOTOR_FINGER3_A:
                    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, ccr); break;
                case MOTOR_FINGER3_B:
                    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, ccr); break;
                case MOTOR_FINGER_ROT:
                    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, ccr); break;
            }
        }
    }
}
```

---

## 8. main.c 초기화 코드

```c
int main(void) {
    // HAL 초기화 (자동 생성)
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();

    // PWM 시작 (11채널 전부)
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

    // Queue 생성 (최대 10개 명령 저장)
    xServoQueue = xQueueCreate(10, sizeof(ServoCmd_t));

    // 태스크 생성
    xTaskCreate(UART_RX_Task,     "UART_RX",  256, NULL, 3, NULL);
    xTaskCreate(PWM_Control_Task, "PWM_CTRL", 256, NULL, 2, NULL);

    // FreeRTOS 스케줄러 시작
    vTaskStartScheduler();

    while(1) {}
}
```

---

## 9. Jetson에서 보내는 코드 (Python)

```python
import serial
import struct

ser = serial.Serial('/dev/ttyUSB0', 115200)

def send_servo_cmd(motor_id, angle):
    # 각도 * 10 해서 정수로 변환
    angle_int = int(angle * 10)
    # 3바이트로 패킹: [모터ID][각도 상위byte][각도 하위byte]
    data = struct.pack('BBB',
                       motor_id,
                       (angle_int >> 8) & 0xFF,
                       angle_int & 0xFF)
    ser.write(data)

# 예시: 모터 0번(상완 회전) 90도로 이동
send_servo_cmd(0, 90.0)

# 예시: 손가락1 서보A 45도
send_servo_cmd(4, 45.0)
```

---

## 10. 주의사항

- Queue 크기 10개 → 명령이 너무 빠르게 오면 늘려야 함
- `osDelay()` 는 FreeRTOS CMSIS-RTOS API
- CubeIDE에서 FreeRTOS 미들웨어 활성화 필수
- 모든 서보 GND 공통 연결 필수
