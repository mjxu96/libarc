cmake_minimum_required(VERSION 3.12.0)

project(arc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # using Clang
  set(CXX_COROUTINE_COMPILE_FLAGS "-stdlib=libc++")
  include_directories("/usr/lib/llvm-11/include/c++/v1/")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using Gcc
  set(CXX_COROUTINE_COMPILE_FLAGS "-fcoroutines")
endif()

option(ARC_BUILD_TESTS "whehter build tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CXX_COROUTINE_COMPILE_FLAGS}")

set(LIBARC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory("cmake")

install (
    EXPORT arcConfig
    DESTINATION ${CMAKE_INSTALL_PREFIX}/arc/cmake NAMESPACE arc::
)
