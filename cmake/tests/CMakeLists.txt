
enable_testing()
find_package(GTest REQUIRED)

add_executable(coro_test ${CMAKE_SOURCE_DIR}/tests/test_coro.cc)
target_link_libraries(coro_test arc GTest::GTest)

add_test(NAME AllTestsInFoo COMMAND coro_test)