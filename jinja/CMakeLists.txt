reflex_add_cxx_module_library(reflex.jinja
  MODULE_STD
  SOURCES
    src/jinja.cppm
    src/expr.cppm
)
target_link_libraries(reflex.jinja PUBLIC reflex.core reflex.serde_json)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
