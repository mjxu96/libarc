cmake_minimum_required(VERSION 3.5.1)

project(libarc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # using Clang
  set(CXX_COROUTINE_COMPILE_FLAGS "-std=c++20 -stdlib=libc++ -pthread")
  include_directories("/usr/lib/llvm-11/include/c++/v1/")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using Gcc
  set(CXX_COROUTINE_COMPILE_FLAGS "-std=c++20 -fcoroutines -pthread -g")
endif()

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CXX_COROUTINE_COMPILE_FLAGS}")
set(LIBARC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory("cmake")
