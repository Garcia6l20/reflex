reflex_add_cxx_module_library(reflex.poly
  MODULE_STD
  SOURCES
    src/poly.cppm
    src/var.cppm
)
target_link_libraries(reflex.poly PUBLIC reflex.core)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
