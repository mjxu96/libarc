
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_coro ${LIBARC_SOURCE_DIR}/tests/test_coro.cc)
add_executable(test_coro_socket ${LIBARC_SOURCE_DIR}/tests/test_coro_socket.cc)

target_link_libraries(test_coro arc GTest::GTest)
target_link_libraries(test_coro_socket arc GTest::GTest)

add_test(NAME AllTestsInFoo COMMAND test_coro test_coro_socket)
