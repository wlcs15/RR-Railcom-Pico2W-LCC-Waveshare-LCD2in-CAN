# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "pico, pico_w, pico2, or pico2_w")
set(LCC_TRANSPORT "WIFI" CACHE STRING "WIFI, CAN, or BOTH")
set(LCD_PANEL "NONE" CACHE STRING "NONE, LCD2, or RES35")
set(RR_KEY0_GPIO "15" CACHE STRING "GPIO number for KEY0")

cmake_minimum_required(VERSION 3.13)

set(_rr_boards pico pico_w pico2 pico2_w)
if(NOT PICO_BOARD IN_LIST _rr_boards)
    message(FATAL_ERROR "PICO_BOARD must be pico, pico_w, pico2, or pico2_w (got '${PICO_BOARD}')")
endif()
if(NOT LCC_TRANSPORT MATCHES "^(WIFI|CAN|BOTH)$")
    message(FATAL_ERROR "LCC_TRANSPORT must be WIFI, CAN, or BOTH (got '${LCC_TRANSPORT}')")
endif()
if(NOT LCD_PANEL MATCHES "^(NONE|LCD2|RES35)$")
    message(FATAL_ERROR "LCD_PANEL must be NONE, LCD2, or RES35 (got '${LCD_PANEL}')")
endif()
if(LCD_PANEL STREQUAL "RES35")
    if(NOT PICO_BOARD STREQUAL "pico_w" OR NOT LCC_TRANSPORT STREQUAL "WIFI")
        message(FATAL_ERROR "LCD_PANEL=RES35 is only the Pico W Wi-Fi preview. It shares GP5 and GP21 with Pico-CAN-B, so CAN and BOTH are refused.")
    endif()
endif()
if(NOT RR_KEY0_GPIO MATCHES "^[0-9]+$")
    message(FATAL_ERROR "RR_KEY0_GPIO must be an integer (got '${RR_KEY0_GPIO}')")
endif()

set(PICO_MBEDTLS_CONFIG_FILE "rr_mbedtls_config.h" CACHE STRING "" FORCE)
include(pico_sdk_import.cmake)
project(rr_railcom_pico2w_lcc C CXX ASM)
pico_sdk_init()

set(FREERTOS_KERNEL_PATH "$ENV{HOME}/Git/wlcs15/FreeRTOS/FreeRTOS-Kernel" CACHE PATH "Pico 2 W kernel branch" FORCE)
if(PICO_RP2350)
    set(RR_FREERTOS_PORT "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ")
else()
    set(RR_FREERTOS_PORT "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040")
endif()
if(NOT EXISTS "${RR_FREERTOS_PORT}/library.cmake")
    message(FATAL_ERROR "FreeRTOS port missing at ${RR_FREERTOS_PORT}. On the pico2w branch run: git submodule update --init portable/ThirdParty/Community-Supported-Ports")
endif()
add_subdirectory("${RR_FREERTOS_PORT}" freertos_kernel)

set(RR_WIFI_WRAP 0)
if(PICO_CYW43_SUPPORTED AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/local/wifi_psk_wrap.inc")
    set(RR_WIFI_WRAP 1)
endif()

add_executable(lcc_node
    src/lcc_main.c
    src/uart_ring.c
    src/pin_map.c
    src/gridconnect.c
    src/lcc_login.c
    src/lcc_node_services.c
    tests/test_pin_map.c
    tests/test_lcc.c
    third_party/Unity/src/unity.c)
target_include_directories(lcc_node PRIVATE include third_party/Unity/src)
if(RR_WIFI_WRAP)
    target_compile_definitions(lcc_node PRIVATE 
        MBEDTLS_CONFIG_FILE="rr_mbedtls_config.h")
    target_include_directories(lcc_node PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/local"
        "${PICO_SDK_PATH}/lib/mbedtls/include")
    target_sources(lcc_node PRIVATE
        "${PICO_SDK_PATH}/lib/mbedtls/library/aes.c"
        "${PICO_SDK_PATH}/lib/mbedtls/library/gcm.c"
        "${PICO_SDK_PATH}/lib/mbedtls/library/hkdf.c"
        "${PICO_SDK_PATH}/lib/mbedtls/library/md.c"
        "${PICO_SDK_PATH}/lib/mbedtls/library/sha256.c"
        "${PICO_SDK_PATH}/lib/mbedtls/library/block_cipher.c"
        "${PICO_SDK_PATH}/lib/mbedtls/library/constant_time.c")
endif()
target_compile_definitions(lcc_node PRIVATE
    RR_LCC_TRANSPORT="${LCC_TRANSPORT}"
    RR_LCD_PANEL="${LCD_PANEL}"
    RR_PICO_BOARD="${PICO_BOARD}"
    RR_KEY0_GPIO=${RR_KEY0_GPIO}
    RR_WIFI_WRAP=${RR_WIFI_WRAP}
    RR_FIRMWARE_TESTS=0
    UNITY_SUPPORT_64
    UNITY_INCLUDE_64)
if(LCD_PANEL STREQUAL "RES35")
    target_sources(lcc_node PRIVATE src/restouch35.c)
    target_compile_definitions(lcc_node PRIVATE RR_PANEL_RES35=1)
    target_link_libraries(lcc_node hardware_spi)
else()
    target_compile_definitions(lcc_node PRIVATE RR_PANEL_RES35=0)
endif()

if(LCC_TRANSPORT STREQUAL "CAN" OR LCC_TRANSPORT STREQUAL "BOTH")
    target_sources(lcc_node PRIVATE src/xl2515.c)
    target_link_libraries(lcc_node hardware_spi)
endif()

pico_enable_stdio_usb(lcc_node 1)
pico_enable_stdio_uart(lcc_node 0)
pico_add_extra_outputs(lcc_node)

if(PICO_CYW43_SUPPORTED)
    target_compile_definitions(lcc_node PRIVATE RR_LED_CYW43=1)
    target_link_libraries(lcc_node
        pico_stdlib
        pico_cyw43_arch_lwip_sys_freertos
        FreeRTOS-Kernel-Heap4)
    if(RR_WIFI_WRAP)
        # Software AES only. pico_mbedtls hardware AES breaks GCM tags.
    endif()
else()
    target_compile_definitions(lcc_node PRIVATE RR_LED_CYW43=0)
    target_link_libraries(lcc_node pico_stdlib pico_async_context_freertos FreeRTOS-Kernel-Heap4)
endif()

