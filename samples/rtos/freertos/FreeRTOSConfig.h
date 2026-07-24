#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION                    1
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      25000000U   /* FVP REFCLK */
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                256
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEEUE_REGISTRY_SIZE              8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5
#define configSTACK_DEPTH_TYPE                  uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t
#define configTOTAL_HEAP_SIZE                   (512 * 1024)

/* Hook functions */
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          0

/* Optional task functions */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* Runtime stats */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* Co-routines */
#define configUSE_CO_ROUTINES                   0

/* Software timers */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* Interrupt priorities: CM55 has 8 NVIC prio bits */
#define configPRIO_BITS                         3
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     0x7
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY     ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Definitions that map FreeRTOS port interrupt handlers to vector table names */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#include <stdint.h>
extern uint32_t SystemCoreClock;

/* Assert: use semihosting so we can see failures */
static inline void __configAssertFail(void) {
    register const char *r1 __asm("r1") = "[ASSERT] configASSERT failed!\n";
    register int         r0 __asm("r0") = 4;
    __asm volatile ("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
    for (;;);
}
#define configASSERT(x) do { if (!(x)) __configAssertFail(); } while(0)

#endif /* FREERTOS_CONFIG_H */
