set(PICO_BOARD pico2 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(rp2350_can C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(rp2350_can "main.c" "xl2515.c" )

pico_set_program_name(rp2350_can "rp2350_can")
pico_set_program_version(rp2350_can "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(rp2350_can 0)
pico_enable_stdio_usb(rp2350_can 1)

# Add the standard library to the build
target_link_libraries(rp2350_can
        pico_stdlib
        hardware_spi)

# Add the standard include files to the build
target_include_directories(rp2350_can PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(rp2350_can)

