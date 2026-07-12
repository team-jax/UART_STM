<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# src

## Purpose
All application source. CubeMX-style generated skeletons extended with the servo-controller
logic entirely inside `USER CODE` marker blocks.

## Key Files
| File | Description |
|------|-------------|
| `main.c` | Entry point + all application logic: USART1 init, UART RX ISR state machine, `PWM_Control_Task`, gesture handling, `Angle_to_CCR()` |
| `stm32f4xx_it.c` | Fault/IRQ handlers, modified for FreeRTOS (see below) |
| `stm32f4xx_hal_msp.c` | MSP callbacks: GPIO AF mappings per timer channel (TIM2→PA0-3, TIM3→PA6/PA7/PB0/PB1, TIM4→PB6-8) |
| `system_stm32f4xx.c` | CMSIS SystemInit/clock variables (framework template, rarely touched) |
| `UART_STM32.code-workspace` | VS Code workspace file (editor config, not firmware) |

## main.c Layout (all in USER CODE blocks)
- **Defines (USER CODE PD)**: motor-ID map (0–10), `GESTURE_CMD_ID 0xF0`,
  `FINGER_OPEN_ANGLE 180.0f` / `FINGER_GRIP_ANGLE 30.0f` (calibration targets — the values the
  `CLAUDE.md` work order STEP 6 says to tune).
- **`MX_USART1_UART_Init()`**: manual USART1 + PA9(TX)/PA10(RX) AF7 setup, 115200 8-N-1,
  NVIC priority 6. Not CubeMX-generated (no `.ioc` had USART1).
- **`HAL_UART_RxCpltCallback()`**: 1-byte-at-a-time ISR state machine; resyncs on `0xAA`,
  validates checksum (`(id + angle_H + angle_L) & 0xFF`), accepts motor_id ≤ 10 **or** `0xF0`
  (gesture), then `osMessageQueuePut()` from ISR (CMSIS-RTOS2 wrapper auto-uses FromISR API).
  For gestures, `cmd.angle` carries the raw gesture code (0=OPEN, 1=GRIP), NOT angle×10/10.
- **`PWM_Control_Task()`**: sole FreeRTOS task; blocks on queue, dispatches by motor_id to
  the right `__HAL_TIM_SET_COMPARE`. The `GESTURE_CMD_ID` case drives all 6 finger channels
  (TIM3 CH1-4 + TIM4 CH1-2) to the same angle; unknown gesture codes are ignored.
  If the hand turns out to be mirror-assembled, the B channels (TIM3_CH2/CH4, TIM4_CH2) get
  `Angle_to_CCR(180.0f - a)` instead — see `CLAUDE.md` STEP 4-1.
- **`main()` USER CODE 2**: starts 11 PWM channels, `osKernelInitialize()`, creates queue
  (16 × `ServoCmd_t`) + task (512 B stack, osPriorityNormal), arms `HAL_UART_Receive_IT`,
  `osKernelStart()`.

## stm32f4xx_it.c — FreeRTOS Integration (do not "fix" these)
- `SVC_Handler`/`PendSV_Handler` bodies are **removed entirely** — FreeRTOS port.c provides
  them via `#define`s in `include/FreeRTOSConfig.h`. Re-adding stubs = duplicate-symbol
  link error.
- `SysTick_Handler` calls `HAL_IncTick()` unconditionally, then `xPortSysTickHandler()` only
  when the scheduler has started.
- `USART1_IRQHandler()` (USER CODE 1) forwards to `HAL_UART_IRQHandler(&huart1)`.

## For AI Agents

### Working In This Directory
- Keep all additions inside `USER CODE BEGIN/END` blocks.
- Servo timing invariants: TIM Prescaler=15 / Period=19999 (1 MHz tick, 50 Hz);
  `Angle_to_CCR` maps 0–180° → 500–2500 µs with clamping. Changing these moves real servos
  out of spec.
- Boot state: all CCRs start at 0 (no pulse) — servos hold position until first command.
  Whether to auto-assume OPEN pose at boot is an open decision (CLAUDE.md STEP 7).

### Testing Requirements
- `pio run` must build with 0 errors/warnings.
- Behavior changes need on-hardware verification per the `CLAUDE.md` HEX-frame test tables
  (e.g. `AA F0 00 01 F1` GRIP, `AA 04 03 84 8B` = motor 4 → 90.0°, bad-checksum frames must
  be silently dropped).

## Dependencies

### Internal
- `include/FreeRTOSConfig.h` (kernel config + handler #defines), `include/stm32f4xx_hal_conf.h`
  (HAL module toggles), `include/main.h`
- `lib/Middlewares` (FreeRTOS + CMSIS-RTOS2 wrapper)

### External
- STM32CubeF4 HAL (`Drivers/`), CMSIS device headers

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
