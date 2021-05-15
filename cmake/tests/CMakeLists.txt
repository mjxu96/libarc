
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_main ${LIBARC_SOURCE_DIR}/tests/test_main.cc)

target_link_libraries(test_main arc GTest::GTest)

add_test(NAME AllTestsInFoo COMMAND test_main)
