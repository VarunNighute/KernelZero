/* ================================================================
   main.c  —  Task Definitions and Entry Point
   KernelZero | STM32F103C8 | Round-Robin Scheduler

   THREE LED DEMO — no debugger required
   ──────────────────────────────────────
   LED 1 — PC13  onboard  (active LOW)   task_a  500ms period
   LED 2 — PB0   external (active HIGH)  task_b  1000ms period
   LED 3 — PB1   external (active HIGH)  task_c  2000ms period

   WIRING
   ──────
   LED 1: already on board — no wiring needed

   LED 2 (PB0):
     PB0 → 220Ω → LED anode (+)
     LED cathode (−) → any GND pin

   LED 3 (PB1):
     PB1 → 220Ω → LED anode (+)
     LED cathode (−) → any GND pin

   PB0 and PB1 are adjacent pins on the left header of Blue Pill.
   Both are GPIO pins with no special function conflicts.

   WHAT TO OBSERVE WITHOUT A DEBUGGER
   ────────────────────────────────────
   LED 1 blinks fastest  — 500ms ON, 500ms OFF
   LED 2 blinks medium   — 1000ms ON, 1000ms OFF
   LED 3 blinks slowest  — 2000ms ON, 2000ms OFF

   All three run independently and simultaneously. If any LED
   stops or blinks at the wrong rate, the scheduler has a bug.
   If all three blink correctly at their own rates, round-robin
   context switching is working.

   REGISTER DERIVATION
   ────────────────────
   All addresses from RM0008 Table 3 (memory map) and Section 9.5
   (GPIO register map). Process for each register:
     1. Find peripheral base in Table 3
     2. Add register offset from Section 9.5
     3. Find bit position in bit description table
     4. Write a named #define — never a raw number
   ================================================================ */

#include "kz_sched.h"

/* ----------------------------------------------------------------
   GPIOC — PC13 onboard LED (active LOW)

   Base address: RM0008 Table 3 → GPIOC → 0x40011000
   CRH offset:   RM0008 Section 9.5 → 0x04 (pins 8–15)
   BSRR offset:  RM0008 Section 9.5 → 0x10

   PC13 active LOW:
     LED ON  = PC13 LOW  = write 1 to bit 29 (BR13 = 16+13)
     LED OFF = PC13 HIGH = write 1 to bit 13 (BS13)

   CRH pin 13 field = bits [23:20]
   Target: MODE13=10 (2MHz out), CNF13=00 (push-pull) → 0x2      */
#define GPIOC_BASE   0x40011000U
#define GPIOC_CRH    (*((volatile uint32_t*)(GPIOC_BASE + 0x04U)))
#define GPIOC_BSRR   (*((volatile uint32_t*)(GPIOC_BASE + 0x10U)))

#define LED1_ON      (1U << 29)   /* BR13 — PC13 LOW  = on  */
#define LED1_OFF     (1U << 13)   /* BS13 — PC13 HIGH = off */

/* ----------------------------------------------------------------
   GPIOB — PB0 (LED 2) and PB1 (LED 3), both active HIGH

   Base address: RM0008 Table 3 → GPIOB → 0x40010C00
   CRL offset:   RM0008 Section 9.5 → 0x00 (pins 0–7)
   BSRR offset:  RM0008 Section 9.5 → 0x10

   PB0 occupies bits [3:0] of CRL
   PB1 occupies bits [7:4] of CRL
   Both: MODE=10 (2MHz out), CNF=00 (push-pull) → nibble 0x2

   PB0 active HIGH:
     LED ON  = PB0 HIGH = write 1 to bit 0  (BS0)
     LED OFF = PB0 LOW  = write 1 to bit 16 (BR0 = 16+0)

   PB1 active HIGH:
     LED ON  = PB1 HIGH = write 1 to bit 1  (BS1)
     LED OFF = PB1 LOW  = write 1 to bit 17 (BR1 = 16+1)         */
#define GPIOB_BASE   0x40010C00U
#define GPIOB_CRL    (*((volatile uint32_t*)(GPIOB_BASE + 0x00U)))
#define GPIOB_BSRR   (*((volatile uint32_t*)(GPIOB_BASE + 0x10U)))

#define LED2_ON      (1U << 0)    /* BS0  — PB0 HIGH = on  */
#define LED2_OFF     (1U << 16)   /* BR0  — PB0 LOW  = off */

#define LED3_ON      (1U << 1)    /* BS1  — PB1 HIGH = on  */
#define LED3_OFF     (1U << 17)   /* BR1  — PB1 LOW  = off */

/* ----------------------------------------------------------------
   RCC — clock enable for GPIOB and GPIOC

   Base address: RM0008 Table 3 → RCC → 0x40021000
   APB2ENR offset: RM0008 Section 7.3.7 → 0x18
   Bit 3 = IOPBEN: GPIOB clock enable
   Bit 4 = IOPCEN: GPIOC clock enable

   Both enabled together in one write — avoids intermediate state
   where one port is clocked and the other is not.                */
#define RCC_BASE     0x40021000U
#define RCC_APB2ENR  (*((volatile uint32_t*)(RCC_BASE + 0x18U)))
#define IOPBEN       (1U << 3)
#define IOPCEN       (1U << 4)

/* ----------------------------------------------------------------
   TASK STACKS
   128 words = 512 bytes each. Enough for the 16-word exception
   frame plus the delay_ticks call depth.                         */
static uint32_t stack_a[128];
static uint32_t stack_b[128];
static uint32_t stack_c[128];

/* ----------------------------------------------------------------
   delay_ticks — spin delay using global tick counter

   kz.tick_count increments every SysTick = every 10ms.
   N ticks = N × 10ms.

   500ms  = 50 ticks
   1000ms = 100 ticks
   2000ms = 200 ticks

   Unsigned subtraction handles rollover:
   when tick_count wraps from 0xFFFFFFFF to 0, the result is
   still correct because unsigned overflow is well-defined in C.

   While spinning, other tasks still get their tick slices —
   the scheduler switches away from this task on every tick.      */
static void delay_ticks(uint32_t ticks)
{
    uint32_t start = kz.tick_count;
    while ((kz.tick_count - start) < ticks);
}

/* ----------------------------------------------------------------
   task_a — LED 1 (PC13 onboard, active LOW)
   500ms ON, 500ms OFF
   50 ticks ON + 50 ticks OFF = 100 ticks total = 1000ms period
   Blinks twice per second — fastest of the three               */
void SystemInit(void) { }
void task_a(void)
{
    while (1) {
        GPIOC_BSRR = LED1_ON;
        delay_ticks(50);
        GPIOC_BSRR = LED1_OFF;
        delay_ticks(50);
    }
}

/* ----------------------------------------------------------------
   task_b — LED 2 (PB0 external, active HIGH)
   1000ms ON, 1000ms OFF
   100 ticks ON + 100 ticks OFF = 200 ticks total = 2000ms period
   Blinks once per two seconds — medium rate                     */
void task_b(void)
{
    while (1) {
        GPIOB_BSRR = LED2_ON;
        delay_ticks(100);
        GPIOB_BSRR = LED2_OFF;
        delay_ticks(100);
    }
}

/* ----------------------------------------------------------------
   task_c — LED 3 (PB1 external, active HIGH)
   2000ms ON, 2000ms OFF
   200 ticks ON + 200 ticks OFF = 400 ticks total = 4000ms period
   Blinks once per four seconds — slowest of the three           */
void task_c(void)
{
    while (1) {
        GPIOB_BSRR = LED3_ON;
        delay_ticks(200);
        GPIOB_BSRR = LED3_OFF;
        delay_ticks(200);
    }
}

/* ----------------------------------------------------------------
   main — hardware init, task registration, scheduler start

   Dependency order — must not change:
   1. Enable clocks  (RCC)  — peripherals powered off after reset
   2. Configure pins (CRL/CRH) — mode before output state
   3. Set initial LED states   — all off before tasks start
   4. kz_init()               — zero scheduler before adding tasks
   5. kz_task_add()            — register tasks after init
   6. kz_start()               — never returns                    */
int main(void)
{
    /* ── Step 1: Enable GPIOB and GPIOC clocks ──────────────────
       Source: RM0008 Section 7.3.7 — RCC_APB2ENR
       Bit 3 = IOPBEN (GPIOB), Bit 4 = IOPCEN (GPIOC)
       One write enables both — avoids partial state             */
    RCC_APB2ENR |= (IOPBEN | IOPCEN);

    /* ── Step 2a: Configure PC13 as output ──────────────────────
       Source: RM0008 Section 9.2.2 — GPIOx_CRH
       Pin 13 = bits [23:20] of CRH
       Clear 4 bits first, then write target nibble 0x2          */
    GPIOC_CRH &= ~(0xFU << 20);
    GPIOC_CRH |=  (0x2U << 20);

    /* ── Step 2b: Configure PB0 and PB1 as outputs ──────────────
       Source: RM0008 Section 9.2.1 — GPIOx_CRL
       Pin 0 = bits [3:0],  Pin 1 = bits [7:4]
       Clear each 4-bit field, then write target nibble 0x2
       Both configured as 2MHz push-pull output                  */
    GPIOB_CRL &= ~(0xFFU << 0);   /* clear bits [7:0] (pins 0+1) */
    GPIOB_CRL |=  (0x22U << 0);   /* 0x2 for pin0, 0x2 for pin1  */

    /* ── Step 3: All LEDs off at startup ─────────────────────────
       LED1 (PC13) OFF = pin HIGH (active low)
       LED2 (PB0)  OFF = pin LOW  (active high)
       LED3 (PB1)  OFF = pin LOW  (active high)
       BSRR writes are atomic — safe before scheduler starts     */
    GPIOC_BSRR = LED1_OFF;
    GPIOB_BSRR = (LED2_OFF | LED3_OFF);  /* both in one write    */

    /* ── Step 4: Scheduler init ──────────────────────────────── */
    kz_init();

    /* ── Step 5: Register tasks ──────────────────────────────────
       task_a → LED1 PC13 onboard — 500ms rate
       task_b → LED2 PB0 external — 1000ms rate
       task_c → LED3 PB1 external — 2000ms rate                  */
    kz_task_add(task_a, stack_a, 128, "led1-500ms");
    kz_task_add(task_b, stack_b, 128, "led2-1000ms");
    kz_task_add(task_c, stack_c, 128, "led3-2000ms");

    /* ── Step 6: Start scheduler — never returns ─────────────── */
    kz_start();

    while (1);
}
