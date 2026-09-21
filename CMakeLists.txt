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

include(pico_sdk_import.cmake)
project(rr_railcom_pico2w_lcc C CXX ASM)
pico_sdk_init()

add_executable(blink
    src/blink.c
    src/pin_map.c)
target_include_directories(blink PRIVATE include)
target_compile_definitions(blink PRIVATE
    RR_LCC_TRANSPORT="${LCC_TRANSPORT}"
    RR_LCD_PANEL="${LCD_PANEL}"
    RR_PICO_BOARD="${PICO_BOARD}"
    RR_KEY0_GPIO=${RR_KEY0_GPIO})

pico_enable_stdio_usb(blink 1)
pico_enable_stdio_uart(blink 0)
pico_add_extra_outputs(blink)

if(PICO_CYW43_SUPPORTED)
    target_compile_definitions(blink PRIVATE RR_LED_CYW43=1)
    target_link_libraries(blink pico_stdlib pico_cyw43_arch_none)
else()
    target_compile_definitions(blink PRIVATE RR_LED_CYW43=0)
    target_link_libraries(blink pico_stdlib)
endif()
