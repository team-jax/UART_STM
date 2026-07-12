<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# Drivers (vendored STM32CubeF4 — DO NOT EDIT)

## Purpose
Vendored STM32CubeF4 framework: CMSIS core/device headers and the STM32F4xx HAL driver
sources. Pulled in by PlatformIO's `stm32cube` framework. **Entirely third-party — never
edit anything under this directory.** No per-subdirectory AGENTS.md files are maintained
here intentionally.

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `CMSIS/` | ARM CMSIS: core headers, STM32F4xx device headers/startup, plus unused DSP/NN/RTOS trees |
| `STM32F4xx_HAL_Driver/` | HAL peripheral driver sources (`Src/`) and headers (`Inc/`) |

## For AI Agents

### Working In This Directory
- Read-only reference. To change which HAL modules compile, edit
  `include/stm32f4xx_hal_conf.h` — not these sources.
- Large unused trees (CMSIS DSP/NN, examples) exist but are not compiled; ignore them.
- The CMSIS-RTOS2 headers used by the app come from `lib/Middlewares`, not from
  `Drivers/CMSIS/RTOS2`.

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
