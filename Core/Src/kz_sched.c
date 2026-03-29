#include "kz_sched.h"
#include <string.h>

#define SYSTICK_CTRL  (*((volatile uint32_t*)0xE000E010U))
#define SYSTICK_LOAD  (*((volatile uint32_t*)0xE000E014U))
#define SYSTICK_VAL   (*((volatile uint32_t*)0xE000E018U))

#define SYSTICK_ENABLE     (1U << 0)
#define SYSTICK_TICKINT    (1U << 1)
#define SYSTICK_CLKSOURCE  (1U << 2)

#define ICSR       (*((volatile uint32_t*)0xE000ED04U))
#define PENDSVSET  (1U << 28)

#define SHPR3      (*((volatile uint32_t*)0xE000ED20U))

kz_scb_t kz;

static uint32_t *stack_init(uint32_t *base, uint32_t size, void (*fn)(void))
{
    uint32_t *sp = base + size;

    for (uint32_t i = 0; i < size; i++)
        base[i] = KZ_STACK_FILL;

    *(--sp) = 0x01000000U;
    *(--sp) = (uint32_t)fn;
    *(--sp) = 0xFFFFFFFDU;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;

    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;
    *(--sp) = 0x00000000U;

    return sp;
}

void kz_init(void)
{
    memset(&kz, 0, sizeof(kz));
}

int kz_task_add(void (*fn)(void), uint32_t *stack,
                uint32_t stack_size, const char *name)
{
    for (int i = 0; i < KZ_MAX_TASKS; i++) {
        if (kz.tasks[i].slot_used) continue;

        kz.tasks[i].sp        = stack_init(stack, stack_size, fn);
        kz.tasks[i].state     = KZ_READY;
        kz.tasks[i].ticks_run = 0;
        kz.tasks[i].slot_used = 1;
        strncpy(kz.tasks[i].name, name, sizeof(kz.tasks[i].name) - 1);

        kz.task_count++;
        return i;
    }
    return -1;
}

static void pick_next(void)
{
    uint8_t next = (kz.current_idx + 1) % KZ_MAX_TASKS;

    for (uint8_t i = 0; i < KZ_MAX_TASKS; i++) {

        if (kz.tasks[next].slot_used &&
            kz.tasks[next].state == KZ_READY) {

            kz.tasks[kz.current_idx].state = KZ_READY;
            kz.current_idx                 = next;
            kz.tasks[next].state           = KZ_RUNNING;
            return;
        }

        next = (next + 1) % KZ_MAX_TASKS;
    }
}

void kz_tick(void)
{
    kz.tick_count++;
    kz.tasks[kz.current_idx].ticks_run++;

    pick_next();

    ICSR |= PENDSVSET;
    __asm volatile ("DSB");
    __asm volatile ("ISB");
}

void SysTick_Handler(void)
{
    kz_tick();
}

void kz_block(void)
{
    kz.tasks[kz.current_idx].state = KZ_BLOCKED;
    pick_next();

    ICSR |= PENDSVSET;
    __asm volatile ("DSB");
    __asm volatile ("ISB");
}

void kz_unblock(int slot)
{
    if (slot >= 0 &&
        slot < KZ_MAX_TASKS &&
        kz.tasks[slot].slot_used) {
        kz.tasks[slot].state = KZ_READY;
    }
}

extern void kz_launch(uint32_t *sp);

void kz_start(void)
{
    SHPR3 |= (0xFFU << 16);

    SYSTICK_LOAD = 79999U;
    SYSTICK_VAL  = 0U;
    SYSTICK_CTRL = SYSTICK_CLKSOURCE | SYSTICK_TICKINT | SYSTICK_ENABLE;

    kz.tasks[0].state = KZ_RUNNING;
    kz.current_idx    = 0;

    kz_launch(kz.tasks[0].sp);

    while (1);
}
