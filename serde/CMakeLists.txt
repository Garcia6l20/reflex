reflex_add_cxx_module_library(reflex.serde)
target_link_libraries(reflex.serde INTERFACE reflex.core reflex.poly)

reflex_add_cxx_module_library(reflex.serde_json)
target_link_libraries(reflex.serde_json INTERFACE reflex.serde)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()