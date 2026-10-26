cmake_minimum_required(VERSION 3.25)
project(riscv_on_c LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(XLEN 64 CACHE STRING "Register width of the emulated hart (32 or 64)")
set_property(CACHE XLEN PROPERTY STRINGS 32 64)
if(NOT XLEN MATCHES "^(32|64)$")
  message(FATAL_ERROR "XLEN must be 32 or 64, got '${XLEN}'")
endif()

option(SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" ON)

add_library(project_options INTERFACE)
if(MSVC)
  target_compile_options(project_options INTERFACE /W4)
else()
  target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(SANITIZE)
  if(MSVC)
    target_compile_options(project_options INTERFACE /fsanitize=address)
  else()
    set(SANITIZER_FLAGS
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer)
    target_compile_options(project_options INTERFACE ${SANITIZER_FLAGS})
    target_link_options(project_options INTERFACE ${SANITIZER_FLAGS})
  endif()
endif()

# Emulator core: everything in src/ except the entry point.
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/*.c)
list(REMOVE_ITEM CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)

add_library(riscv_core STATIC ${CORE_SOURCES})
target_include_directories(riscv_core PUBLIC include)
target_compile_definitions(riscv_core PUBLIC XLEN=${XLEN})
target_link_libraries(riscv_core PUBLIC project_options)

add_executable(riscv src/main.c)
target_link_libraries(riscv PRIVATE riscv_core)

# Tests: every non-empty .c under tests/unit and tests/integration becomes its
# own executable and CTest test, named after the file (test_decoder, ...).
include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(unity)

  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
       tests/unit/*.c
       tests/integration/*.c)

  foreach(test_source IN LISTS TEST_SOURCES)
    file(SIZE ${test_source} test_size)
    if(test_size EQUAL 0)
      continue()
    endif()

    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE tests)
    target_link_libraries(${test_name} PRIVATE riscv_core unity::framework)
    set_target_properties(${test_name} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()

# Official riscv-tests (base integer, M, A, machine- and supervisor-mode
# suites, in the physical-memory environment), built with a RISC-V cross
# compiler when one is available.
option(RISCV_TESTS "Build and run the official riscv-tests" ON)
find_program(RISCV_GCC NAMES riscv64-unknown-elf-gcc riscv64-linux-gnu-gcc)

if(BUILD_TESTING AND RISCV_TESTS AND RISCV_GCC)
  FetchContent_Declare(riscv_tests
    GIT_REPOSITORY https://github.com/riscv-software-src/riscv-tests.git
    GIT_TAG        3df703cd52d5f0d8224e3c50bf03c6cb70efff98
    GIT_SUBMODULES env)
  FetchContent_MakeAvailable(riscv_tests)

  if(XLEN EQUAL 64)
    set(RISCV_TESTS_ABI -march=rv64g -mabi=lp64)
  else()
    set(RISCV_TESTS_ABI -march=rv32g -mabi=ilp32)
  endif()

  add_executable(riscv_tests_runner tests/riscv-tests/runner.c)
  target_link_libraries(riscv_tests_runner PRIVATE riscv_core)

  set(RISCV_TESTS_DIR ${CMAKE_BINARY_DIR}/riscv-tests)
  set(RISCV_TEST_SUITES ui um ua mi si)
  # Privileged tests are enabled one by one as the features they need land.
  set(RISCV_MI_TESTS
      ld-misaligned lh-misaligned lw-misaligned sd-misaligned sh-misaligned
      sw-misaligned ma_addr mcsr sbreak scall shamt zicntr instret_overflow
      csr)
  set(RISCV_SI_TESTS csr ma_fetch sbreak scall)
  foreach(suite IN LISTS RISCV_TEST_SUITES)
    file(GLOB RISCV_TEST_SOURCES ${riscv_tests_SOURCE_DIR}/isa/rv${XLEN}${suite}/*.S)
    foreach(test_source IN LISTS RISCV_TEST_SOURCES)
      get_filename_component(name ${test_source} NAME_WE)
      if(suite STREQUAL "mi" AND NOT name IN_LIST RISCV_MI_TESTS)
        continue()
      endif()
      if(suite STREQUAL "si" AND NOT name IN_LIST RISCV_SI_TESTS)
        continue()
      endif()
      set(test_name rv${XLEN}${suite}-p-${name})
      set(test_elf ${RISCV_TESTS_DIR}/${test_name})
      add_custom_command(
        OUTPUT ${test_elf}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RISCV_TESTS_DIR}
        COMMAND ${RISCV_GCC} ${RISCV_TESTS_ABI} -static -mcmodel=medany
                -fvisibility=hidden -nostdlib -nostartfiles
                -I${riscv_tests_SOURCE_DIR}/env/p
                -I${riscv_tests_SOURCE_DIR}/isa/macros/scalar
                -T${riscv_tests_SOURCE_DIR}/env/p/link.ld
                ${test_source} -o ${test_elf}
        DEPENDS ${test_source}
        VERBATIM)
      list(APPEND RISCV_TEST_ELFS ${test_elf})
      add_test(NAME ${test_name} COMMAND riscv_tests_runner ${test_elf})
    endforeach()
  endforeach()
  add_custom_target(riscv_tests_elfs ALL DEPENDS ${RISCV_TEST_ELFS})
endif()
