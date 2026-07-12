<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# lib

## Purpose
PlatformIO private-library directory. Contains exactly one library: `Middlewares`, the
vendored FreeRTOS distribution. PlatformIO's Library Dependency Finder cannot auto-detect it
(the `cmsis_os2.h` include is too deep), so `platformio.ini` forces it via
`lib_deps = Middlewares` + `lib_ldf_mode = deep+` — without that, FreeRTOS silently never
compiles and linking fails on missing `os*`/`vTask*` symbols.

## Key Files
| File | Description |
|------|-------------|
| `README` | PlatformIO scaffold placeholder |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `Middlewares/` | Vendored FreeRTOS + CMSIS-RTOS2 wrapper (see `Middlewares/AGENTS.md`) |

## For AI Agents

### Working In This Directory
- Do not rename `Middlewares/` — the name is referenced by `lib_deps` in `platformio.ini`.
- New first-party libraries would go in their own `lib/<name>/` folder per PlatformIO
  convention; none exist today.

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
