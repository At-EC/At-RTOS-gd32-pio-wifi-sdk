
cmake_minimum_required(VERSION 3.15)

add_library(rtthread_api INTERFACE)

add_library(rtthread)

target_sources(rtthread
    PRIVATE
        src/clock.c
        src/components.c
        src/cpu.c
        src/device.c
        src/idle.c
        src/ipc.c
        src/irq.c
        src/kservice.c
        src/mem.c
        src/memheap.c
        src/mempool.c
        src/object.c
        src/scheduler.c
        src/signal.c
        src/slab.c
        src/thread.c
        src/timer.c
    PUBLIC
        libcpu/arm/cortex-m33/context_gcc.S
        libcpu/arm/cortex-m33/cpuport.c
        libcpu/arm/cortex-m33/syscall_gcc.S
        libcpu/arm/cortex-m33/trustzone.c
        bsp/gd32w51x/drivers/board.c
        components/drivers/pm/pm_sleep.c
        components/drivers/pm/pm.c

)

target_include_directories(rtthread_api
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/components/drivers/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/components/drivers/pm>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bsp/gd32w51x>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bsp/gd32w51x/drivers>
)

target_link_libraries(rtthread
    rtthread_api
    gd32w51x_wifi_api
    gd32w51x_peripheral_api
    wifi_api
    os_api
)
