add_executable(fxtool fxtool.c)

include_directories(fxtool
  ${CORE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/lib/eventlog/src
  )

target_link_libraries(fxtool
  GLib::GLib
  GLib::GModule
  GLib::GThread
  syslog-ng
  )

install(TARGETS fxtool RUNTIME DESTINATION bin)
