module_switch(ENABLE_APPMODEL "Enable AppModel")

if (NOT ENABLE_APPMODEL)
  return()
endif()

set (APPMODEL_SOURCES
    appmodel.c
    appmodel-parser.c
    appmodel-plugin.c
    appmodel-context.c
    app-object-generator.c
    app-parser-generator.c
    app-transform-generator.c
    application.c
    transformation.c
)

add_module(
  TARGET appmodel
  GRAMMAR appmodel-grammar
  SOURCES ${APPMODEL_SOURCES}
)

add_test_subdirectory(tests)

