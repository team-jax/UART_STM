<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# UART_STM32 — Humanoid-Arm Servo Controller Firmware

## Purpose
PlatformIO firmware for the "Black F407VE" board (STM32F407VET6, Cortex-M4F) using the
`stm32cube` framework (STM32CubeF4 HAL) plus FreeRTOS (CMSIS-RTOS2). A Jetson Nano (or a
USB-UART terminal during bring-up) sends servo commands over USART1 (PA9/PA10, 115200 8-N-1)
using the framed protocol `[0xAA][motor_id][angle_H][angle_L][checksum]`; an interrupt-driven
UART state machine validates each frame and pushes it onto a FreeRTOS queue; a single
`PWM_Control_Task` consumes the queue and updates TIM2/TIM3/TIM4 PWM channels (11 servo
channels: 4× MG996R on TIM2, 7× MG90S on TIM3/TIM4) producing 50 Hz 500–2500 µs pulses.

Besides per-motor commands (motor_id 0–10), a gesture command (motor_id `0xF0`) drives all
6 finger servos at once: `AA F0 00 01 F1` = GRIP (30°), `AA F0 00 00 F0` = OPEN (180°).

## Key Files
| File | Description |
|------|-------------|
| `platformio.ini` | Single env `black_f407ve`; FPU flags, `lib_deps = Middlewares`, `lib_ldf_mode = deep+` — all required, see below |
| `extra_script.py` | Re-applies `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` to LINKFLAGS (PlatformIO `build_flags` only feeds CCFLAGS); without it linking fails with "uses VFP register arguments" |
| `CLAUDE.md` | **Current hardware bring-up work order (Korean)** — step-by-step test plan for the robot hand (GRIP/OPEN gesture test, wiring pinmap, calibration). Follow it in order when assisting with hardware testing |
| `.vscode/launch.json` | PlatformIO Debug config (firmware.elf + STM32F40x SVD). `c_cpp_properties.json` is auto-generated — never hand-edit |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `src/` | Application source: main.c (UART ISR, PWM task, gesture logic), IT handlers, MSP init (see `src/AGENTS.md`) |
| `include/` | FreeRTOSConfig.h, HAL module config, shared headers (see `include/AGENTS.md`) |
| `lib/` | Project libraries; only `Middlewares/` = vendored FreeRTOS (see `lib/AGENTS.md`) |
| `Drivers/` | Vendored STM32CubeF4 (CMSIS + HAL) — **third-party, do not edit** (see `Drivers/AGENTS.md`) |
| `docs/` | Design notes; partially superseded (see `docs/AGENTS.md`) |
| `test/` | Empty PlatformIO Unity test scaffold (see `test/AGENTS.md`) |

## For AI Agents

### Build Commands
No Makefile/CMake — PlatformIO only. `pio` lives at `~/.platformio/penv/bin/pio` (add to PATH if missing).

```bash
pio run                # build (env: black_f407ve)
pio run -t upload      # flash via configured debugger (ST-Link)
pio run -t clean
pio device monitor     # 115200 baud, matches USART1
pio test -f <suite>    # run one Unity test suite from test/<suite>/test_*.c
```

### Working In This Repository
- The FreeRTOS/FPU/LDF settings in `platformio.ini`, `extra_script.py`, and
  `lib/Middlewares/library.json` are load-bearing and interdependent — do not "simplify" them.
  Each guards against a specific link failure (duplicate symbols, missing FreeRTOS objects,
  VFP mismatch).
- Application code lives inside `USER CODE` marker blocks in `src/main.c` so CubeMX regeneration
  (if ever used) won't clobber it. Keep new code inside those blocks.
- TIM2/3/4 use `Prescaler=15` / `Period=19999` (16 MHz HSI / 16 = 1 MHz tick, 20 ms = 50 Hz).
  These are servo-PWM requirements, not CubeMX defaults — never revert them.
- Firmware builds clean but hardware verification is in progress (see `CLAUDE.md` work order);
  do not claim runtime behavior is verified unless the user reports board test results.

### Testing Requirements
- `pio run` must exit 0 with no warnings before any change is considered done.
- No automated tests exist yet; hardware behavior is verified manually per the `CLAUDE.md` steps.

## Dependencies

### External
- PlatformIO `ststm32` platform, `stm32cube` framework (framework-stm32cubef4 @ 1.28.x)
- toolchain-gccarmnoneeabi (arm-none-eabi-gcc 7.2.x)
- FreeRTOS (vendored under `lib/Middlewares`, CMSIS-RTOS2 wrapper, heap_4, GCC ARM_CM4F port)

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
