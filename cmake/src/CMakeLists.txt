
include_directories("${LIBARC_ROOT_DIR}/include")

add_library(arccoro
  ${LIBARC_ROOT_DIR}/src/coro/eventloop.cc
)

add_library(arcio
  ${LIBARC_ROOT_DIR}/src/io/io_base.cc
)

add_library(arc
  ${LIBARC_ROOT_DIR}/src/arc.cc
)

target_link_libraries(arc
  arccoro
  arcio
)

target_include_directories(arc PUBLIC "${LIBARC_ROOT_DIR}/include")