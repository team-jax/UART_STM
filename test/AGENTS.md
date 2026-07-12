<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# test

## Purpose
PlatformIO Unity test scaffold — currently empty (README placeholder only). No automated
tests exist; firmware behavior is verified manually on hardware per the root `CLAUDE.md`
work order.

## For AI Agents

### Working In This Directory
- New tests follow the PlatformIO Unity convention: `test/<suite_name>/test_*.c`, run with
  `pio test -f <suite_name>`.
- Good first candidates for host-side unit tests: `Angle_to_CCR()` clamping/mapping and the
  UART frame checksum/state-machine logic (would need extracting from `main.c` into a
  testable module first).

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
