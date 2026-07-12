<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-07-13 | Updated: 2026-07-13 -->

# docs

## Purpose
Design notes and guides. Currently one document, and it is partially out of date.

## Key Files
| File | Description |
|------|-------------|
| `FREERTOS_GUIDE.md` | Korean design notes for the task/queue/motor-ID layout. **Written before the interrupt-driven UART redesign**: its `UART_RX_Task` description and 3-byte no-header/no-checksum packet format are superseded by the ISR state machine and 5-byte framed protocol actually in `src/main.c`. The motor-ID table and RTOS rationale are still useful background |

## For AI Agents

### Working In This Directory
- Do not treat `FREERTOS_GUIDE.md`'s protocol description as current — `src/main.c` and the
  root `AGENTS.md` are authoritative for the wire protocol (including the 0xF0 gesture frame).
- If the protocol or task layout changes again, either update this guide or extend its
  superseded-notes warning.

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
