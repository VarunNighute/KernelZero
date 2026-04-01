.syntax unified
.thumb

.global  kz_launch
.global  PendSV_Handler
.extern  kz

.equ TCB_SIZE,          28
.equ KZ_MAX_TASKS_ASM,   6
.equ TASKS_ARRAY_SZ,   168
.equ CURRENT_IDX_OFF,  168

.section .text.kz_launch
kz_launch:
    MSR     PSP, R0
    MRS     R0, CONTROL
    ORR     R0, R0, #0x02
    MSR     CONTROL, R0
    ISB
    POP     {R4-R11}
    POP     {R0-R3, R12, LR}
    POP     {PC}

.section .text.PendSV_Handler
PendSV_Handler:
    MRS     R0, PSP
    STMDB   R0!, {R4-R11}

    LDR     R1, =kz
    LDRB    R2, [R1, #CURRENT_IDX_OFF]
    MOV     R3, #TCB_SIZE
    MUL     R2, R2, R3
    ADD     R2, R1, R2
    STR     R0, [R2, #0]

    LDRB    R2, [R1, #CURRENT_IDX_OFF]
    MOV     R3, #TCB_SIZE
    MUL     R2, R2, R3
    ADD     R2, R1, R2
    LDR     R0, [R2, #0]

    LDMIA   R0!, {R4-R11}
    MSR     PSP, R0

    BX      LR
