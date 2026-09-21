cmake_minimum_required(VERSION 3.13)
project(rr_pin_map_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RR_ENABLE_COVERAGE
       "Clang source-based coverage (llvm-cov). Host tests only." OFF)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(NOT EXISTS "${ROOT}/third_party/Unity/src/unity.c")
    message(FATAL_ERROR "Unity missing at third_party/Unity/src/unity.c")
endif()

add_library(unity_host STATIC "${ROOT}/third_party/Unity/src/unity.c")
target_include_directories(unity_host PUBLIC "${ROOT}/third_party/Unity/src")

add_library(pin_map_host STATIC
    "${ROOT}/src/pin_map.c"
    "${ROOT}/src/gridconnect.c"
    "${ROOT}/src/lcc_login.c")
target_include_directories(pin_map_host PUBLIC "${ROOT}/include")

add_executable(pin_map_tests
    "${ROOT}/tests/test_pin_map.c"
    "${ROOT}/tests/test_lcc.c")
target_include_directories(pin_map_tests PRIVATE
    "${ROOT}/include"
    "${ROOT}/third_party/Unity/src")
target_link_libraries(pin_map_tests PRIVATE unity_host pin_map_host)

if(RR_ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "RR_ENABLE_COVERAGE requires Clang, not GCC or MSVC.")
    endif()
    find_program(LLVM_PROFDATA NAMES llvm-profdata llvm-profdata.exe)
    find_program(LLVM_COV NAMES llvm-cov llvm-cov.exe)
    if(NOT LLVM_PROFDATA OR NOT LLVM_COV)
        message(FATAL_ERROR "RR_ENABLE_COVERAGE needs llvm-profdata and llvm-cov on PATH")
    endif()
    target_compile_options(pin_map_tests PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(pin_map_tests PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_compile_options(pin_map_host PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" _bin)
    file(TO_CMAKE_PATH "${ROOT}/src/pin_map.c" _pin_src)
    file(TO_CMAKE_PATH "${ROOT}/src/gridconnect.c" _gc_src)
    file(TO_CMAKE_PATH "${ROOT}/src/lcc_login.c" _login_src)
    set(_profraw "${_bin}/pin_map_tests.profraw")
    set(_profdata "${_bin}/pin_map_tests.profdata")
    set(_htmldir "${_bin}/coverage")
    add_custom_target(coverage
        DEPENDS pin_map_tests
        COMMAND ${CMAKE_COMMAND} -E env
                "LLVM_PROFILE_FILE=${_profraw}"
                "$<TARGET_FILE:pin_map_tests>"
        COMMAND "${LLVM_PROFDATA}" merge -sparse "${_profraw}" -o "${_profdata}"
        COMMAND "${LLVM_COV}" report
                "$<TARGET_FILE:pin_map_tests>"
                "-instr-profile=${_profdata}"
                "-ignore-filename-regex=third_party|Unity"
                "${_pin_src}" "${_gc_src}" "${_login_src}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_htmldir}"
        COMMAND "${LLVM_COV}" show
                "$<TARGET_FILE:pin_map_tests>"
                "-instr-profile=${_profdata}"
                -format=html
                "-output-dir=${_htmldir}"
                "-ignore-filename-regex=third_party|Unity"
                "${_pin_src}" "${_gc_src}" "${_login_src}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Unity + llvm-cov for src/pin_map.c"
        USES_TERMINAL
        VERBATIM)
endif()
