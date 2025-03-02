/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/

#include <debug_print.h>
#include <malloc.h>
#include <delay.h>

static os_task_t os_idle_task_tcb;
static os_task_t os_timer_task_tcb;

/***************** heap management implementation *****************/
/*!
    \brief      allocate a block of memory with a minimum of 'size' bytes.
    \param[in]  size: the minimum size of the requested block in bytes
    \param[out] none
    \retval     address to allocated memory, NULL pointer if there is an error
*/
void *sys_malloc(size_t size)
{
    return pvSysMalloc(size);
}

/*!
    \brief      allocate a certian chunks of memory with specified size
                Note: The allocated memory is filled with bytes of value zero.
                All chunks in the allocated memory are contiguous.
    \param[in]  count: multiple number of size want to malloc
    \param[in]  size:  number of size want to malloc
    \param[out] none
    \retval     address to allocated memory, NULL pointer if there is an error
*/
void *sys_calloc(size_t count, size_t size)
{
    void *mem_ptr;

    mem_ptr = pvSysMalloc(count*size);
    sys_memset(mem_ptr, 0, (count*size));

    return mem_ptr;
}

/*!
    \brief      change the size of a previously allocated memory block.
    \param[in]  mem: address to the old buffer
    \param[in]  size: number of the new buffer size
    \param[out] none
    \retval     address to allocated memory, NULL pointer if there is an error
*/
void *sys_realloc(void *mem, size_t size)
{
    return pvSysReAlloc(mem, size);
}

/*!
    \brief      free a memory to the heap
    \param[in]  ptr: pointer to the address want to free
    \param[out] none
    \retval     none
*/
void sys_mfree(void *ptr)
{
    vSysFree(ptr);
}

/*!
    \brief      get system free heap size
    \param[in]  none
    \param[out] none
    \retval     system free heap size value(0x00000000-0xffffffff)
*/
int32_t sys_free_heap_size(void)
{
    return xSysGetFreeHeapSize();
}

/*!
    \brief      get minimum free heap size that has been reached
    \param[in]  none
    \param[out] none
    \retval     system minimum free heap size value(0x00000000-0xffffffff)
*/
int32_t sys_min_free_heap_size(void)
{
    return xSysGetMinimumEverFreeHeapSize();
}

/*!
    \brief      get system least heap block size
                Note: it's used to count all memory used by heap and heap management structures.
                    A least block is usually consist of a heap block header and a minimum block.
                    However heap allocation may not align the allocated heap area with this minimum block,
                    the counting results may be inaccurate.
    \param[in]  none
    \param[out] none
    \retval     system heap block size value in bytes(0x0000-0xffff)
*/
uint16_t sys_heap_block_size(void)
{
    return xSysGetHeapMinimumBlockSize();
}

/*!
    \brief      copy buffer content from source address to destination address.
    \param[in]  src: the address of source buffer
    \param[in]  n: the length to copy
    \param[out] des: the address of destination buffer
    \retval     none
*/
void sys_memcpy(void *des, const void *src, uint32_t n)
{
    uint8_t *xdes = (uint8_t *)des;
    uint8_t *xsrc = (uint8_t *)src;

    while (n--) {
        *xdes++ = *xsrc++;
    }
}

/*!
    \brief      move buffer content from source address to destination address
                Note: It could work between two overlapped buffers.
    \param[in]  src: the address of source buffer
    \param[in]  n: the length to move
    \param[out] des: the address of destination buffer
    \retval     none
*/
void sys_memmove(void *des, const void *src, uint32_t n)
{
    char *tmp = (char *)des;
    char *s = (char *)src;

    if (s < tmp && tmp < s + n) {
        tmp += n;
        s += n;

        while (n--)
            *(--tmp) = *(--s);
    } else {
        while (n--)
            *tmp++ = *s++;
    }
}

/*!
    \brief      set the content of the buffer to specified value
    \param[in]  s: The address of a buffer
    \param[in]  c: the value want to memset
    \param[in]  count: count value want to memset
    \param[out] none
    \retval     none
*/
void sys_memset(void *s, uint8_t c, uint32_t count) SECTION_RAM_CODE
{
    uint32_t dword_cnt;
    uint32_t dword_value;
    uint8_t *p_dst = (uint8_t *)s;

    while (((uint32_t)p_dst & 0x03) != 0) {
        if (count == 0) {
            return;
        }
        *p_dst++ = c;
        count--;
    }

    dword_cnt = (count >> 2);
    count &= 0x03;
    dword_value = (((uint32_t)(c) << 0) & 0x000000FF) | (((uint32_t)(c) << 8) & 0x0000FF00) |
                    (((uint32_t)(c) << 16) & 0x00FF0000) | (((uint32_t)(c) << 24) & 0xFF000000);

    while (dword_cnt > 0u) {
        *(uint32_t *)p_dst = dword_value;
        p_dst += 4;
        dword_cnt--;
    }

    while (count > 0u) {
        *p_dst++ = c;
        count--;
    }
}

/*!
    \brief      compare two buffers
    \param[in]  buf1: address to the source buffer 1
    \param[in]  buf2: address to the source buffer 2
    \param[in]  count: the compared buffer size in bytes
    \param[out] none
    \retval      0 if buf1 equals buf2, non-zero otherwise.
*/
int32_t sys_memcmp(const void *buf1, const void *buf2, uint32_t count)
{
    if(!count)
        return 0;

    while (--count && *((char *)buf1) == *((char *)buf2)) {
        buf1 = (char *)buf1 + 1;
        buf2 = (char *)buf2 + 1;
    }

    return (*((char *)buf1) - *((char *)buf2));
}

/***************** OS API wrappers *****************/
void *sys_task_create(void *task, const uint8_t *name, uint32_t *stack_base, uint32_t stack_size,
                    uint32_t queue_size, uint32_t priority, task_func_t func, void *ctx)
{
    task_wrapper_t *task_wrapper = NULL;

    task_wrapper = (task_wrapper_t *)sys_malloc(sizeof(task_wrapper_t));
    if (task_wrapper == NULL) {
        DEBUGPRINT("sys_task_create: malloc wrapper failed\r\n");
        return NULL;
    }

    if (queue_size) {
        task_wrapper->msgq_id = os_msgq_init(NULL, sizeof(void *), queue_size, (const char_t *)name);
        if (os_id_is_invalid(task_wrapper->msgq_id)) {
            DEBUGPRINT("sys_task_create: malloc queue memory failed\r\n");
            sys_mfree(task_wrapper);
            return NULL;
        }
    }

    stack_size *= sizeof(uint32_t);

    /* protect task creation and task wrapper pointer storing against inconsistency of them if preempted in the middle */
    OS_ENTER_CRITICAL_SECTION();
    if (task != NULL && stack_base != NULL) {
        task_wrapper->thread_id = os_thread_init(stack_base, stack_size, priority, func, ctx, (const char_t *)name);
        if (os_id_is_invalid(task_wrapper->thread_id)) {
            DEBUGPRINT("sys_task_create init task failed\r\n");
            sys_mfree(task_wrapper);
            OS_EXIT_CRITICAL_SECTION();
            return NULL;
        }

        *(os_task_t*)task = task_wrapper->thread_id;
    } else {
        task_wrapper->thread_id = os_thread_init(NULL, stack_size, priority, func, ctx, (const char_t *)name);
        if (os_id_is_invalid(task_wrapper->thread_id)) {
            DEBUGPRINT("sys_task_create init task failed\r\n");
            sys_mfree(task_wrapper);
            OS_EXIT_CRITICAL_SECTION();
            return NULL;
        }
    }
    os_thread_user_data_set(task_wrapper->thread_id, (void*)task_wrapper);

    OS_EXIT_CRITICAL_SECTION();
    return &task_wrapper->thread_id;
}

void sys_task_delete(void *task)
{
    os_task_t task_id = *(os_task_t*)task;
    task_wrapper_t *task_wrapper;
    if (task == NULL) {
        task_id = os_thread_id_self();
    }

    task_wrapper = (task_wrapper_t *)os_thread_user_data_get(task_id);
    if (task_wrapper != NULL) {
        sys_mfree(task_wrapper);
    }
    os_thread_delete(task_id);
}

int32_t sys_task_wait(uint32_t timeout_ms, void *msg_ptr)
{
    os_task_t task_id;
    task_wrapper_t *task_wrapper;
    int32_t result;

    task_id = os_thread_id_self();
    task_wrapper = (task_wrapper_t *)os_thread_user_data_get(task_id);
    if (task_wrapper == NULL) {
        DEBUGPRINT("sys_task_wait: can't find task wrapper\r\n");
        return OS_ERROR;
    }

    if (!timeout_ms) {
        timeout_ms = OS_TIME_WAIT_FOREVER;
    }
    result = os_msgq_get(task_wrapper->msgq_id, msg_ptr, sizeof(void *), false, timeout_ms);
    PC_IF(result, PC_PASS_INFO) {
        return OS_OK;
    }
    return OS_ERROR;
}

int32_t sys_task_post(void *receiver_task, void *msg_ptr, uint8_t from_isr)
{
    os_task_t task_id = *(os_task_t*)receiver_task;
    task_wrapper_t *task_wrapper;
    int32_t ret;

    task_wrapper = (task_wrapper_t *)os_thread_user_data_get(task_id);
    if (task_wrapper == NULL) {
        DEBUGPRINT("sys_task_post: can't find task wrapper\r\n");
        return OS_ERROR;
    }

    ret = os_msgq_put(task_wrapper->msgq_id, msg_ptr, sizeof(void *), false, OS_TIME_NOWAIT_VAL);
    PC_IF(ret, PC_PASS_INFO) {
        return OS_OK;
    }
    return OS_ERROR;
}

void sys_task_msg_flush(void *task)
{
    os_task_t task_id = *(os_task_t*)task;
    task_wrapper_t *task_wrapper;
    DEBUGPRINT("%s %d\n", __FUNCTION__, __LINE__);

    if (task == NULL) {
        task_id = os_thread_id_self();
    }

    task_wrapper = (task_wrapper_t *)os_thread_user_data_get(task_id);
    if (task_wrapper != NULL) {
        int32_t result = 0;
        do {
            uint32_t data = 0;
            result = os_msgq_get(task_wrapper->msgq_id, (void*)&data, sizeof(void *), false, OS_TIME_NOWAIT_VAL);
            PC_IF(result, PC_PASS_INFO) {
                if (result == OS_PC_NODATA) {
                    break;
                }
            } else {
                DEBUGPRINT("sys_task_msg_flush: can't flush task msg\r\n");
                break;
            }
        } while(result >= 0);
    }
}

int32_t sys_task_msg_num(void *task, uint8_t from_isr)
{
    os_task_t task_id = *(os_task_t*)task;
    task_wrapper_t *task_wrapper;

    if (task == NULL) {
        task_id = os_thread_id_self();
    }

    task_wrapper = (task_wrapper_t *)os_thread_user_data_get(task_id);
    if (task_wrapper == NULL) {
        DEBUGPRINT("sys_task_msg_num: can't find task wrapper\r\n");
        return OS_ERROR;
    }

    return os_msgq_num_probe(task_wrapper->msgq_id);
}

os_task_t *sys_idle_task_handle_get(void)
{
    return os_thread_idle_id_probe();
}

os_task_t *sys_timer_task_handle_get(void)
{
    return NULL;
}

uint32_t sys_stack_free_get(void *task)
{
    os_task_t task_id = *(os_task_t*)task;

    if (task == NULL)
        return 0;

    return os_thread_stack_free_size_probe(task_id);
}

static void _list_thread(const thread_context_t *task)
{
    DEBUGPRINT("sys_thread, name = %s\r\n", task->head.pName);
}

void sys_task_list(char *pwrite_buf)
{
    os_trace_foreach_thread(_list_thread);
}

int32_t sys_sema_init(os_sema_t *sema, int32_t init_val)
{
    *sema = os_sem_init(init_val, OS_SEM_LIMIT_BINARY, "sema");
    if (os_id_is_invalid(*sema)) {
        DEBUGPRINT("sys_sema_init, sema = NULL\r\n");
        return OS_ERROR;
    }
    return OS_OK;
}

void sys_sema_free(os_sema_t *sema)
{
    if (sema == NULL) {
        DEBUGPRINT("sys_sema_free, sema = NULL\r\n");
        return;
    }
    os_sem_delete(*sema);
    *sema = NULL;
}

void sys_sema_up(os_sema_t *sema)
{
    if (!sema) {
        DEBUGPRINT("sys_sema_up, sema = NULL\r\n");
    }
    os_sem_give(*sema);
}

void sys_sema_up_from_isr(os_sema_t *sema)
{
    if (!sema) {
        DEBUGPRINT("sys_sema_up, sema = NULL\r\n");
    }
    os_sem_give(*sema);
}

int32_t sys_sema_down(os_sema_t *sema, uint32_t timeout_ms)
{
    if (!sema) {
        DEBUGPRINT("sys_sema_down, sema = NULL\r\n");
    }

    if (!timeout_ms) {
        timeout_ms = OS_TIME_WAIT_FOREVER;
    }

    i32_t result = os_sem_take(*sema, timeout_ms);
    PC_IF(result, PC_PASS_INFO) {
        if (result == OS_PC_TIMEOUT) {
            DEBUGPRINT("sys_queue_post failed\r\n");
            return OS_ERROR;
        }
    } else {
        DEBUGPRINT("sys_queue_post failed\r\n");
        return OS_ERROR;
    }
    return OS_OK;
}

void sys_mutex_init(os_mutex_t *mutex)
{
    *mutex = os_mutex_init("mutex");
    if (os_id_is_invalid(*mutex)) {
        DEBUGPRINT("sys_mutex_init, mutex = NULL\r\n");
    }
}

void sys_mutex_free(os_mutex_t *mutex)
{
    if (mutex == NULL) {
        DEBUGPRINT("sys_mutex_free, mutex = NULL\r\n");
        return;
    }
    os_mutex_delete(*mutex);
    *mutex = NULL;
}

void sys_mutex_get(os_mutex_t *mutex)
{
    i32_t result = os_mutex_lock(*mutex);
    PC_IF(result, PC_ERROR) {
        DEBUGPRINT("get mutex failed\r\n");
    }
}

void sys_mutex_put(os_mutex_t *mutex)
{
    i32_t result = os_mutex_unlock(*mutex);
    PC_IF(result, PC_ERROR) {
        DEBUGPRINT("put mutex failed\r\n");
    }
}

int32_t sys_queue_init(os_queue_t *queue, int32_t queue_size, uint32_t item_size)
{
    *queue = os_msgq_init(NULL, item_size, queue_size, "queue");
    if (os_id_is_invalid(*queue)) {
        DEBUGPRINT("sys_queue_init failed\r\n");
        return OS_ERROR;
    }

    return OS_OK;
}

void sys_queue_free(os_queue_t *queue)
{
    os_msgq_delete(*queue);
    *queue = NULL;
}

int32_t sys_queue_post(os_queue_t *queue, void *msg)
{
    i32_t ret = os_msgq_put(*queue, msg, 0, false, OS_TIME_NOWAIT_VAL);
    PC_IF(ret, PC_PASS_INFO) {
        return OS_OK;
    }
    DEBUGPRINT("sys_queue_post failed\r\n");
    return OS_ERROR;
}

int32_t sys_queue_fetch(os_queue_t *queue, void *msg, uint32_t timeout_ms, uint8_t is_blocking)
{
    if (is_blocking) {
        if (!timeout_ms) {
            timeout_ms = OS_TIME_WAIT_FOREVER;
        }
    } else {
        timeout_ms = OS_TIME_NOWAIT;
    }
    i32_t result = os_msgq_get(*queue, msg, 0, false, timeout_ms);
    PC_IF(result, PC_PASS_INFO) {
        if ((result == OS_PC_TIMEOUT) || (result == OS_PC_NODATA)) {
            return OS_TIMEOUT;
        } else {
            return OS_OK;
        }
    }
    DEBUGPRINT("sys_queue_post failed\r\n");
    return OS_ERROR;
}

uint32_t sys_current_time_get(void)
{
    return (uint32_t)os_timer_system_total_ms();
}

uint32_t sys_time_get(void *p)
{
    return sys_current_time_get();
}

void sys_ms_sleep(uint32_t ms)
{
    if (!ms) {
        ms = 1;
    }
    os_thread_sleep(ms);
}

void sys_us_delay(uint32_t nus)
{
    if (!nus) {
        nus = 1;
    }
    os_timer_system_busy_wait(nus);
}

void sys_yield(void)
{
    os_thread_yield();
}

void sys_sched_lock(void)
{
    os_kernel_lock();
}

void sys_sched_unlock(void)
{
    os_kernel_unlock();
}

static int32_t _arc4_random(void)
{
    uint32_t res = sys_current_time_get();
    uint32_t seed = seed_rand32();

    seed = ((seed & 0x007F00FF) << 7) ^
        ((seed & 0x0F80FF00) >> 8) ^
        (res << 13) ^ (res >> 9);

    return (int32_t)seed;
}

int32_t sys_random_bytes_get(void *dst, uint32_t size)
{
    uint32_t ranbuf;
    uint32_t *lp;
    int32_t i, count;
    count = size / sizeof(uint32_t);
    lp = (uint32_t *)dst;

    for (i = 0; i < count; i++) {
        lp[i] = _arc4_random();
        size -= sizeof(uint32_t);
    }

    if(size > 0) {
        ranbuf = _arc4_random();
        sys_memcpy(&lp[i], &ranbuf, size);
    }

    return OS_OK;
}

static void _sys_timer_callback(void *p_tmr)
{
    timer_wrapper_t *timer_wrapper = (timer_wrapper_t *)p_tmr;

    if (timer_wrapper->timer_func) {
        timer_wrapper->timer_func(timer_wrapper->timer_id, timer_wrapper->p_arg);
    }
}

void sys_timer_init(os_timer_t *timer, const uint8_t *name, uint32_t delay, uint8_t periodic, timer_func_t func, void *arg)
{
    if (!delay) {
        delay = OS_TIME_WAIT_FOREVER;
    }
    timer_wrapper_t *timer_wrapper;

    timer_wrapper = (timer_wrapper_t *)sys_malloc(sizeof(timer_wrapper_t));
    if (timer_wrapper == NULL) {
        DEBUGPRINT("sys_timer_init: malloc wrapper failed\r\n");
        return ;
    }

    timer_wrapper->p_arg = arg;
    timer_wrapper->timer_func = func;
    timer_wrapper->delay = delay;
    timer_wrapper->periodic = periodic;
    timer_wrapper->timer_id = os_timer_init(_sys_timer_callback, (void *)timer_wrapper, "timer");
    if (os_id_is_invalid(timer_wrapper->timer_id)) {
        DEBUGPRINT("sys_timer_init failed\r\n");
        sys_mfree(timer_wrapper);
    }

    *timer = &timer_wrapper->timer_id;
}

void sys_timer_delete(os_timer_t *timer)
{
    i32_t result;
    timer_wrapper_t *timer_wrapper;
    timer_wrapper = (timer_wrapper_t *)CONTAINEROF(*timer, timer_wrapper_t, timer_id);

    result = os_timer_delete(timer_wrapper->timer_id);
    PC_IF(result, PC_ERROR) {
        DEBUGPRINT("sys_timer_start_ext failed\r\n");
    }
}

void sys_timer_start(os_timer_t *timer, uint8_t from_isr)
{
    i32_t  result;
    timer_wrapper_t *timer_wrapper;
    timer_wrapper = (timer_wrapper_t *)CONTAINEROF(*timer, timer_wrapper_t, timer_id);

    result = os_timer_start(timer_wrapper->timer_id, (timer_wrapper->periodic ? OS_TIMER_CTRL_CYCLE : OS_TIMER_CTRL_ONCE), timer_wrapper->delay);
    PC_IF(result, PC_ERROR) {
       DEBUGPRINT("sys_timer_start failed\r\n");
    }
}

void sys_timer_start_ext(os_timer_t *timer, uint32_t delay, uint8_t from_isr)
{
    i32_t  result;
    timer_wrapper_t *timer_wrapper;
    timer_wrapper = (timer_wrapper_t *)CONTAINEROF(*timer, timer_wrapper_t, timer_id);

    timer_wrapper->delay = delay;
    result = os_timer_start(timer_wrapper->timer_id, (timer_wrapper->periodic ? OS_TIMER_CTRL_CYCLE : OS_TIMER_CTRL_ONCE), timer_wrapper->delay);
    PC_IF(result, PC_ERROR) {
        DEBUGPRINT("sys_timer_start_ext failed\r\n");
    }
}

uint8_t sys_timer_stop(os_timer_t *timer, uint8_t from_isr)
{
    i32_t result;
    timer_wrapper_t *timer_wrapper;
    timer_wrapper = (timer_wrapper_t *)CONTAINEROF(*timer, timer_wrapper_t, timer_id);

    result = os_timer_stop(timer_wrapper->timer_id);
    PC_IF(result, PC_ERROR) {
        DEBUGPRINT("sys_timer_start_ext failed\r\n");
        return 0;
    }

    return 1;
}

uint8_t sys_timer_pending(os_timer_t *timer)
{
    timer_wrapper_t *timer_wrapper;
    timer_wrapper = (timer_wrapper_t *)CONTAINEROF(*timer, timer_wrapper_t, timer_id);

    return os_timer_busy(timer_wrapper->timer_id);
}

void sys_os_misc_init(void)
{

}

extern void systick_config(void);

void sys_os_init(void)
{

}

void sys_os_start(void)
{
    os_kernel_run();
}

uint8_t sys_task_exist(const uint8_t *name)
{
    os_task_t task_id = os_thread_name_toId((const char_t *)name);
    if (os_id_is_invalid(task_id)) {
        return 1;
    }

    return 0;
}

