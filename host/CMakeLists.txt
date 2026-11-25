cmake_minimum_required(VERSION 3.14)
project(voica C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_STANDARD 99)

# --- WAMR (the runtime core) ---
set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wasm-micro-runtime)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(WAMR_BUILD_PLATFORM "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(WAMR_BUILD_PLATFORM "darwin")
else()
  set(WAMR_BUILD_PLATFORM "linux")
endif()

if(NOT DEFINED WAMR_BUILD_TARGET)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
    set(WAMR_BUILD_TARGET "AARCH64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WAMR_BUILD_TARGET "X86_64")
  else()
    set(WAMR_BUILD_TARGET "X86_32")
  endif()
endif()

# Interpreter only, no AOT/JIT, no libc/WASI — apps are freestanding and
# get everything through the "voica" bridge module. This is what keeps
# the host small.
set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_FAST_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 0)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_MINI_LOADER 0)
set(WAMR_BUILD_SIMD 0)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_BULK_MEMORY 1)

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

add_library(vmlib STATIC ${WAMR_RUNTIME_LIB_SOURCE})

# --- Voica host ---
add_executable(voica main.c bridge.c)
target_include_directories(voica PRIVATE ${WAMR_ROOT_DIR}/core/iwasm/include)
target_link_libraries(voica vmlib)
if(NOT WIN32)
  target_link_libraries(voica -lm -ldl -lpthread)
endif()
