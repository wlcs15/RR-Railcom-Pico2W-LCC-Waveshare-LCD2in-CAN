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
    message(FATAL_ERROR "LCD_PANEL=RES35 is not built here. The 3.5 inch HAT shares GP5 and GP21 with Pico-CAN-B.")
endif()
if(NOT RR_KEY0_GPIO MATCHES "^[0-9]+$")
    message(FATAL_ERROR "RR_KEY0_GPIO must be an integer (got '${RR_KEY0_GPIO}')")
endif()

set(PICO_MBEDTLS_CONFIG_FILE "rr_mbedtls_config.h" CACHE STRING "" FORCE)
include(pico_sdk_import.cmake)
project(rr_railcom_pico2w_lcc C CXX ASM)
pico_sdk_init()

set(RR_WIFI_WRAP 0)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/local/wifi_psk_wrap.inc")
    set(RR_WIFI_WRAP 1)
endif()

add_executable(lcc_node
    src/lcc_main.c
    src/pin_map.c
    src/gridconnect.c
    src/lcc_login.c)
target_include_directories(lcc_node PRIVATE include)
if(RR_WIFI_WRAP)
    target_include_directories(lcc_node PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/local")
endif()
target_compile_definitions(lcc_node PRIVATE
    RR_LCC_TRANSPORT="${LCC_TRANSPORT}"
    RR_LCD_PANEL="${LCD_PANEL}"
    RR_PICO_BOARD="${PICO_BOARD}"
    RR_KEY0_GPIO=${RR_KEY0_GPIO}
    RR_WIFI_WRAP=${RR_WIFI_WRAP})

pico_enable_stdio_usb(lcc_node 1)
pico_enable_stdio_uart(lcc_node 0)
pico_add_extra_outputs(lcc_node)

if(PICO_CYW43_SUPPORTED)
    target_compile_definitions(lcc_node PRIVATE RR_LED_CYW43=1)
    target_link_libraries(lcc_node
        pico_stdlib
        pico_cyw43_arch_lwip_threadsafe_background)
    if(RR_WIFI_WRAP)
        target_compile_definitions(lcc_node PRIVATE MBEDTLS_NO_PLATFORM_ENTROPY)
        target_link_libraries(lcc_node pico_mbedtls)
    endif()
else()
    target_compile_definitions(lcc_node PRIVATE RR_LED_CYW43=0)
    target_link_libraries(lcc_node pico_stdlib)
endif()
