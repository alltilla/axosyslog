add_library(ehttp-source STATIC
  elasticsearch-bulk-parser.c
  elasticsearch-bulk-parser.h
  elasticsearch-bulk-source.c
  elasticsearch-bulk-source.h
  http-decompression.c
  http-decompression.h
  http-source-parser.c
  http-source-parser.h
  http-source.c
  http-source.h
  ${CMAKE_CURRENT_BINARY_DIR}/http-source-grammar.c
  ${CMAKE_CURRENT_BINARY_DIR}/http-source-grammar.h
  ${CMAKE_CURRENT_BINARY_DIR}/elasticsearch-bulk-grammar.c
  ${CMAKE_CURRENT_BINARY_DIR}/elasticsearch-bulk-grammar.h
)

target_link_libraries(ehttp-source PUBLIC syslog-ng ${ZLIB_LIBRARIES})
target_include_directories(ehttp-source
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${ZLIB_INCLUDE_DIRS}
)

generate_y_from_ym(modules/http/source/http-source-grammar)
bison_target(EHTTPSourceGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/http-source-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/http-source-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/http-source-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS}
)

generate_y_from_ym(modules/http/source/elasticsearch-bulk-grammar)
bison_target(ESBulkSourceGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/elasticsearch-bulk-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/elasticsearch-bulk-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/elasticsearch-bulk-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS}
)
