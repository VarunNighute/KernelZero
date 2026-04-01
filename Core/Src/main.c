#include "kz_sched.h"

#define GPIOC_BASE  0x40011000U
#define GPIOC_CRH   (*((volatile uint32_t*)(GPIOC_BASE + 0x04U)))
#define GPIOC_BSRR  (*((volatile uint32_t*)(GPIOC_BASE + 0x10U)))

#define LED_ON   (1U << 29)
#define LED_OFF  (1U << 13)

#define RCC_BASE     0x40021000U
#define RCC_APB2ENR  (*((volatile uint32_t*)(RCC_BASE + 0x18U)))
#define IOPCEN       (1U << 4)

static uint32_t stack_a[128];
static uint32_t stack_b[128];
static uint32_t stack_c[128];

static void delay_ticks(uint32_t ticks)
{
    uint32_t start = kz.tick_count;
    while ((kz.tick_count - start) < ticks);
}

void task_a(void)
{
    while (1) {
        GPIOC_BSRR = LED_ON;
        delay_ticks(3);
        GPIOC_BSRR = LED_OFF;
        delay_ticks(3);
    }
}

void task_b(void)
{
    while (1) {
        GPIOC_BSRR = LED_ON;
        delay_ticks(20);
        GPIOC_BSRR = LED_OFF;
        delay_ticks(20);
    }
}

void task_c(void)
{
    while (1) {
        delay_ticks(1);
    }
}

int main(void)
{
    RCC_APB2ENR |= IOPCEN;

    GPIOC_CRH &= ~(0xFU << 20);
    GPIOC_CRH |=  (0x2U << 20);

    GPIOC_BSRR = LED_OFF;

    kz_init();

    kz_task_add(task_a, stack_a, 128, "blink-fast");
    kz_task_add(task_b, stack_b, 128, "blink-slow");
    kz_task_add(task_c, stack_c, 128, "idle");

    kz_start();

    while (1);
}
