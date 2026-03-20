reflex_add_cxx_module_library(reflex.jinja)
target_link_libraries(reflex.jinja INTERFACE reflex.core reflex.serde_json)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
