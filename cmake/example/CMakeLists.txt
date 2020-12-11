
add_executable(example1
  ${LIBARC_ROOT_DIR}/example.cc
)

target_link_libraries(example1 arc)