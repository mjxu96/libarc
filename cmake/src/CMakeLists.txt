
include_directories("${LIBARC_ROOT_DIR}/include")

add_library(arc
  ${LIBARC_ROOT_DIR}/src/coro/eventloop.cc
)

target_include_directories(arc PUBLIC "${LIBARC_ROOT_DIR}/include")