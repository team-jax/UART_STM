<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# Middlewares (vendored FreeRTOS)

## Purpose
Vendored FreeRTOS kernel + CMSIS-RTOS2 wrapper, packaged as a PlatformIO library.
**Third-party code — treat as read-only** except for the two project-specific artifacts
below (`library.json` and the copied-in GCC port).

## Key Files
| File | Description |
|------|-------------|
| `library.json` | **Project-owned.** Names the library `Middlewares` (for `lib_deps`) and defines a `srcFilter` that excludes the IAR FreeRTOS port and 4 of the 5 heap allocators (only `heap_4.c` compiles). Removing this = duplicate-symbol link errors from conflicting ports/allocators |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `Third_Party/FreeRTOS/Source/` | FreeRTOS kernel sources (tasks.c, queue.c, …) |
| `Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/` | CMSIS-RTOS2 wrapper (`cmsis_os2.h`, `freertos_os2.h` — enforces the FreeRTOSConfig.h constraints documented in `include/AGENTS.md`) |
| `Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/` | **Copied in manually** from `~/.platformio/packages/framework-stm32cubef4/Middlewares/.../portable/GCC/ARM_CM4F/` — the vendored copy only shipped the IAR port, but this toolchain is arm-none-eabi-gcc. Its context-switch asm requires the hard-FPU build flags in `platformio.ini`/`extra_script.py` |
| `Third_Party/FreeRTOS/Source/portable/IAR/ARM_CM4F/` | Unused IAR port, excluded by `srcFilter` — keep excluded |
| `Third_Party/FreeRTOS/Source/portable/MemMang/` | Heap allocators; only `heap_4.c` is compiled |

## For AI Agents

### Working In This Directory
- Never edit kernel sources. Kernel behavior is configured exclusively through
  `include/FreeRTOSConfig.h`.
- If you touch `library.json`'s `srcFilter`, verify with `pio run` that exactly one port and
  one heap implementation still compile (duplicate or missing symbols otherwise).

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
