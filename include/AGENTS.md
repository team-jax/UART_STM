<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# include

## Purpose
Project-wide headers: FreeRTOS kernel configuration, HAL module selection, and shared
declarations. Everything here is compiled into every translation unit that includes it —
small edits have global effect.

## Key Files
| File | Description |
|------|-------------|
| `FreeRTOSConfig.h` | FreeRTOS kernel config — several values are hard constraints, see below |
| `stm32f4xx_hal_conf.h` | Toggles which HAL modules compile (`HAL_UART_MODULE_ENABLED` was deliberately uncommented; most others stay disabled) |
| `stm32f4xx_it.h` | Prototypes for the IRQ handlers in `src/stm32f4xx_it.c` |
| `main.h` | Shared exported declarations (`Error_Handler`, `HAL_TIM_MspPostInit`) |
| `README` | PlatformIO scaffold placeholder |

## FreeRTOSConfig.h — Hard Constraints (breaking these fails the build or the RTOS)
- `configMAX_PRIORITIES` must be exactly `56` and
  `configUSE_PORT_OPTIMISED_TASK_SELECTION` must be `0` — the CMSIS-RTOS2 wrapper
  (`CMSIS_RTOS_V2/freertos_os2.h`) `#error`s otherwise.
- `vPortSVCHandler`/`xPortPendSVHandler` are `#define`d to `SVC_Handler`/`PendSV_Handler`
  so FreeRTOS's port owns those vectors; the matching handler-body removal lives in
  `src/stm32f4xx_it.c`.
- `USE_CUSTOM_SYSTICK_HANDLER_IMPLEMENTATION 1` suppresses `cmsis_os2.c`'s built-in
  `SysTick_Handler` (would collide with the one in `stm32f4xx_it.c`).
- `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` governs which IRQs may call FreeRTOS
  FromISR APIs — USART1 runs at NVIC priority 6 and its ISR calls `osMessageQueuePut`,
  so USART1's priority must be numerically ≥ this value (pending verification,
  `CLAUDE.md` STEP 7).

## For AI Agents

### Working In This Directory
- Never change the two CMSIS-RTOS2-mandated values above.
- Enabling a new HAL module = uncomment its `#define` in `stm32f4xx_hal_conf.h`; the source
  is already vendored in `Drivers/`, no platformio.ini change needed.

### Testing Requirements
- Any edit here requires a full `pio run` rebuild (config headers invalidate everything).

## Dependencies

### Internal
- Consumed by `src/*` and by `lib/Middlewares` FreeRTOS sources (FreeRTOSConfig.h).

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
