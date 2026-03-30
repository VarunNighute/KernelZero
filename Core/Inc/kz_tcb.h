/*
 * kz_tcb.h
 *
 *  Created on: Mar 29, 2026
 *      Author: windows 10
 */

#ifndef INC_KZ_TCB_H_
#define INC_KZ_TCB_H_

#include <stdin.h>

#define KZ_MAX_TASKS 6
#define KZ_STACK_FILL 0xDEADBEEFU

typedef enum{
	KZ_READY = 0;
	KZ_RUNNING = 1;
	Kz_BLOCKED = 2;
}kz_state_t;


typedef struct {
	uint32_t *sp;
	kz_state_t state;
	uint32_t task_run;
	uint8_t slot;
	char name[12];
}kz_tcb_t;

typedef struct{
	kz_tcb_t tasks[KZ_MAX_TASKS];
	uint8_t current_idx;
	uint8_t task_count;
	uint32_t tick_count;

}kz_scb_t;

#endif /* INC_KZ_TCB_H_ */
