reflex_add_cxx_module_library(reflex.poly)
target_link_libraries(reflex.poly INTERFACE reflex.core)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
