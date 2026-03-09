reflex_add_cxx_module_library(reflex.cli
  MODULE_STD
  SOURCES
    src/cli.cppm
)
target_link_libraries(reflex.cli PUBLIC reflex.core)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
