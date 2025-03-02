/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/

#ifndef __WRAPPER_AT_RTOS_H
#define __WRAPPER_AT_RTOS_H

/*============================ INCLUDES ======================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <at_rtos.h>

/*============================ MACROS ========================================*/
#if ATOS_VERSION_MAJOR_NUMBER < 2
#warning "Target at_rtos version 2.0.0 or above, other versions are not fully tested"
#endif

/* Enable them if use OS's provided system ISR implementations */
#define SYS_HARDFAULT_HANDLER
#define SYS_SVC_HANDLER
#define SYS_PENDSV_HANDLER
#define SYS_SYSTICK_HANDLER

/* Priority: 0 ~ 32 (RT_THREAD_PRIORITY_MAX).
 * The priority of idle task is RT_THREAD_PRIORITY_MAX - 1. */
#define TASK_PRIO_APP_BASE            16
#define TASK_PRIO_WIFI_BASE           16
#define TASK_PRIO_IDLE                (OS_PRIOTITY_LOWEST_LEVEL)

#define OS_TICK_RATE_HZ               (1000)
#define OS_MS_PER_TICK                (1000 / 1000)

/*============================ MACRO FUNCTIONS ===============================*/
/* Declare a variable to hold current interrupt status to restore it later */
#define SYS_SR_ALLOC()
/* Disable interrupts (nested) */
#define SYS_CRITICAL_ENTER()          OS_ENTER_CRITICAL_SECTION()

/* Enable interrupts (nested) */
#define SYS_CRITICAL_EXIT()           OS_EXIT_CRITICAL_SECTION()

/* OS IRQ service hook called just after the ISR starts */
#define sys_int_enter()
/* OS IRQ service hook called before the ISR exits */
#define sys_int_exit()

#define TASK_PRIO_HIGHER(n)           (-n)
#define TASK_PRIO_LOWER(n)            (n)

/*============================ TYPES =========================================*/
/*
 * All wrapped new types are pointers to hold dynamically allocated
 * objects except for wrapped task type which we will use for static objects,
 * so keep it as a struct type.
 * In principle, we should nullify all pointers after they are
 * freed or deleted to make them available for sys_xxx_valid macros.
 */
typedef os_sem_id_t os_sema_t;
typedef os_mutex_id_t os_mutex_t;
typedef os_thread_id_t os_task_t;
typedef os_msgq_id_t os_queue_t;
typedef os_timer_id_t os_timer_t;

typedef void (*task_func_t)(void *);
typedef void (*timer_func_t)(void *p_tmr, void *p_arg);
typedef struct timer_wrapper {
    os_timer_id_t timer_id;
    void *p_arg;
    timer_func_t timer_func;
    uint32_t delay;
    uint8_t periodic;
} timer_wrapper_t;

typedef struct task_wrapper {
    os_thread_id_t thread_id;
    os_msgq_id_t msgq_id;
} task_wrapper_t;

/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
/*============================ PROTOTYPES ====================================*/
/*============================ IMPLEMENTATION ================================*/
#endif //#ifndef __WRAPPER_AT_RTOS_H
