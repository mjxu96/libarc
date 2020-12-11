cmake_minimum_required(VERSION 3.5.1)

# set(CMAKE_CXX_COMPILER /usr/bin/clang++-10)
set(CMAKE_CXX_COMPILER /usr/bin/g++-10)
set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE Release)
project(libarc)


set(CXX_COROUTINE_COMPILE_FLAGS "-std=c++20 -fcoroutines -pthread")
# set(CXX_COROUTINE_COMPILE_FLAGS "-std=c++20 -stdlib=libc++ -pthread")
# set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CXX_COROUTINE_COMPILE_FLAGS}")
set(LIBARC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})


add_subdirectory("cmake")
