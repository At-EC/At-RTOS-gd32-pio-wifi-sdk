
cmake_minimum_required(VERSION 3.15)

add_library(app)

if (CONFIG_IPERF_TEST)
    if (CONFIG_STATIC_LIB_BUILDED)
        add_library(iperf3 STATIC IMPORTED GLOBAL)

        set_target_properties(iperf3
            PROPERTIES IMPORTED_LOCATION
            ${PROJECT_SOURCE_DIR}/NSPE/WIFI_IOT/lib/GCC/libiperf3.a)
    endif()
endif()

if (NOT CONFIG_STATIC_LIB_BUILDED)
    add_library(librf STATIC)

    target_sources(librf
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/cmd_rf_test.c
    )

    target_link_libraries(librf
        gd32w51x_peripheral_api
        gd32w51x_wifi_api
        bsp_api
        common_api
        lwIP_api
        mbedtls_ssl_api
        wifi_api
        os_api
        cmsis_api
    )

    add_custom_command(TARGET librf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:librf> ${PROJECT_SOURCE_DIR}/NSPE/WIFI_IOT/lib/GCC/librf.a
    )
else()
    add_library(librf INTERFACE)
endif()

target_sources(app
    PRIVATE
        atcmd.c
        cmd_shell.c
        console.c
        main.c
        mqtt_app/mqtt_client_config.c
        mqtt_app/mqtt_cmd.c
        mqtt_app/mqtt5_client_config.c
    )

target_sources(app
    PRIVATE
        ../demo/iperf3_main.c
        ../demo/ota_demo.c
        ../demo/ssl_certs.c
        ../demo/ssl_client.c
        ../demo/tcp_test.c
        ../demo/udp_test.c
    )

target_link_libraries(app
    gd32w51x_peripheral_api
    gd32w51x_wifi_api
    bsp_api
    common_api
    lwIP_api
    mbedtls_ssl_api
    wifi_api
    librf
    os_api
    cmsis_api
    $<$<BOOL:${CONFIG_ALICLOUD_SUPPORT}>:alicloud>
    $<$<BOOL:${CONFIG_IPERF_TEST}>:iperf3>
    $<$<BOOL:${CONFIG_IPERF_TEST}>:iperf3_api>
    $<$<BOOL:${CONFIG_FATFS_SUPPORT}>:fatfs>
    )

target_sources(app
    PRIVATE
        _build_date.h
)

if(${CMAKE_HOST_WIN32})
    add_custom_command(OUTPUT _build_date.h
        COMMAND ${TAGET_PROJECT_DIR}/build_date.bat ${PROJECT_SOURCE_DIR}/NSPE
    )
elseif(${CMAKE_HOST_UNIX})
    add_custom_command(OUTPUT _build_date.h
        COMMAND ${TAGET_PROJECT_DIR}/build_date.sh ${PROJECT_SOURCE_DIR}/NSPE
    )
else()
    message( FATAL_ERROR "Only support Windows platform or Linux platform" )
endif ()