
#ifndef INC_KZ_SCHED_H_
#define INC_KZ_SCHED_H_

#include "kz_tcb.h"

extern kz_scb_t kz;

void kz_init(void);

int kz_task_add(
		void (*fn)(void),
		uint32_t *stack,
		uint32_t stack_size,
		const char *name);

void kz_start(void);

void kz_tick(void);

void kz_block(void);

void kz_unblock(int slot);


#endif
