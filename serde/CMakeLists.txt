reflex_add_cxx_module_library(reflex.serde
  src/serde.cppm
  src/annotations.cppm
  src/object_visit.cppm
  src/poly.cppm
)
target_link_libraries(reflex.serde PUBLIC reflex.core reflex.poly)

reflex_add_cxx_module_library(reflex.serde_json
  src/json.cppm
  src/json_value.cppm
)
target_link_libraries(reflex.serde_json PUBLIC reflex.serde)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()