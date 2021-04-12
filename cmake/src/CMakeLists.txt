
include_directories("${CMAKE_SOURCE_DIR}/include")

find_package (Threads)

set(ARC_CORO_FILES
  ${CMAKE_SOURCE_DIR}/src/coro/eventloop.cc
  ${CMAKE_SOURCE_DIR}/src/coro/task.cc
)

set(ARC_IO_FILES
  ${CMAKE_SOURCE_DIR}/src/io/io_base.cc
)

set(ARC_LOGGING_FILES
  ${CMAKE_SOURCE_DIR}/src/logging/logger.cc
  ${CMAKE_SOURCE_DIR}/src/logging/logging.cc
)

add_library(arc
  ${CMAKE_SOURCE_DIR}/src/arc.cc
  ${ARC_CORO_FILES}
  ${ARC_IO_FILES}
  ${ARC_LOGGING_FILES}
)

target_link_libraries(arc
  ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(arc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>  
    $<INSTALL_INTERFACE:include>
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
        DESTINATION "include"
        FILES_MATCHING
        PATTERN "*.h"
)

install(
  TARGETS arc
  EXPORT arcConfig
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

export(EXPORT arcConfig NAMESPACE arc:: FILE ${CMAKE_CURRENT_BINARY_DIR}/arcConfig.cmake)
