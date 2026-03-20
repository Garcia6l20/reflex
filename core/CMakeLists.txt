reflex_add_cxx_module_library(reflex.core)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
