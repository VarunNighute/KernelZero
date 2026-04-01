# KernelZero

A preemptive round-robin task scheduler built from scratch for the STM32F103C8 (Blue Pill), written in bare-metal C and ARM Thumb assembly — no FreeRTOS, no HAL, no CMSIS scheduler abstractions.

---

## What it does

- Runs multiple tasks concurrently using time-sliced round-robin scheduling
- SysTick fires every 10ms and pends PendSV for deferred context switching
- PendSV handler saves and restores callee-saved registers (R4–R11) manually in ARM Thumb assembly
- Hardware saves R0–R3, R12, LR, PC, xPSR automatically on exception entry and exit
- Tasks are registered with a stack array and element count — no pointer arithmetic exposed to the caller
- Stack memory is pre-filled with `0xDEADBEEF` pattern for overflow detection in the debugger
- Scheduler state is observable in real time via CubeIDE live watch on the `kz` control block

---

## Tools

| Tool | Version |
|---|---|
| STM32CubeIDE | 1.19 |
| arm-none-eabi-gcc | bundled with CubeIDE |
| ST-Link | V2 |

---


## Key concepts implemented

### Two-phase context switch

SysTick and PendSV have separate responsibilities. SysTick decides which task runs next by calling `pick_next()` and updating `kz.current_idx`. It then sets the PendSV pending bit in the ICSR register. PendSV fires only after SysTick exits completely, then performs the actual register save and restore in assembly.

This separation ensures PendSV never preempts another ISR mid-execution. PendSV is configured at the lowest possible interrupt priority (255) via the SHPR3 register.

### PSP and MSP separation

Cortex-M3 has two stack pointers. The Main Stack Pointer (MSP) is used by exception handlers. The Process Stack Pointer (PSP) is used by tasks in Thread mode. At startup `kz_launch` writes the first task's stack address into PSP, sets CONTROL bit 1 to switch Thread mode to PSP, and executes ISB to flush the pipeline.

Each task operates entirely on its own PSP stack. Interrupts use MSP. A stack overflow in one task cannot corrupt the interrupt stack.

### Manual stack frame construction

`stack_init()` builds a fake exception frame on each task's stack before the scheduler starts. The frame matches exactly what the Cortex-M3 hardware pushes on exception entry (xPSR, PC, LR, R12, R3–R0) followed by the callee-saved registers (R11–R4) that PendSV saves manually.

When `kz_launch` performs an exception return into the first task, the hardware pops this fake frame as if a real exception had fired. The task begins executing from its entry function.

### Round-robin scheduling

`pick_next()` scans forward from `current_idx` using modulo wrap-around. The first occupied, READY slot found becomes the next task. If only one READY task exists, it keeps running. The algorithm is O(N) where N is `KZ_MAX_TASKS`.

