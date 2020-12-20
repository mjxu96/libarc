
include_directories("${LIBARC_ROOT_DIR}/include")

find_package (Threads)

add_library(arccoro
  ${LIBARC_ROOT_DIR}/src/coro/eventloop.cc
  ${LIBARC_ROOT_DIR}/src/coro/task.cc
)

add_library(arcio
  ${LIBARC_ROOT_DIR}/src/io/io_base.cc
)

add_library(arclogging
  ${LIBARC_ROOT_DIR}/src/logging/logger.cc
  ${LIBARC_ROOT_DIR}/src/logging/logging.cc
)

add_library(arc
  ${LIBARC_ROOT_DIR}/src/arc.cc
)

target_link_libraries(arc
  arccoro
  arcio
  arclogging
  ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(arc PUBLIC "${LIBARC_ROOT_DIR}/include")