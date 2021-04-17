
include_directories("${LIBARC_SOURCE_DIR}/include")

find_package (Threads)
find_package(OpenSSL)

set(ARC_CORO_FILES
  ${LIBARC_SOURCE_DIR}/src/coro/eventloop.cc
  ${LIBARC_SOURCE_DIR}/src/coro/events/time_event.cc
  ${LIBARC_SOURCE_DIR}/src/coro/task.cc
)

set(ARC_IO_FILES
  ${LIBARC_SOURCE_DIR}/src/io/io_base.cc
  ${LIBARC_SOURCE_DIR}/src/io/ssl.cc
)

set(ARC_EXCEPTION_FILES
  ${LIBARC_SOURCE_DIR}/src/exception/base.cc
  ${LIBARC_SOURCE_DIR}/src/exception/io.cc
  ${LIBARC_SOURCE_DIR}/src/exception/net.cc
)

set(ARC_LOGGING_FILES
  ${LIBARC_SOURCE_DIR}/src/logging/logger.cc
  ${LIBARC_SOURCE_DIR}/src/logging/logging.cc
)

add_library(arc
  ${LIBARC_SOURCE_DIR}/src/arc.cc
  ${ARC_CORO_FILES}
  ${ARC_IO_FILES}
  ${ARC_EXCEPTION_FILES}
  ${ARC_LOGGING_FILES}
)

target_link_libraries(arc
  ${CMAKE_THREAD_LIBS_INIT}
  OpenSSL::SSL OpenSSL::Crypto
)

target_include_directories(arc
  PUBLIC
    $<BUILD_INTERFACE:${LIBARC_SOURCE_DIR}/include>  
    $<INSTALL_INTERFACE:include>
)

install(DIRECTORY "${LIBARC_SOURCE_DIR}/include/"
        DESTINATION "include"
        FILES_MATCHING
        PATTERN "*.h"
)

include(GNUInstallDirs)
install(
  TARGETS arc
  EXPORT arcConfig
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

export(EXPORT arcConfig NAMESPACE arc:: FILE ${CMAKE_CURRENT_BINARY_DIR}/arcConfig.cmake)
