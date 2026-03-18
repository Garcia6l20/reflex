reflex_add_cxx_module_library(reflex.serde
  MODULE_STD
  SOURCES
    src/serde.cppm
    src/annotations.cppm
    src/object_visit.cppm
    src/poly.cppm
)
target_link_libraries(reflex.serde PUBLIC reflex.core reflex.poly)

reflex_add_cxx_module_library(reflex.serde_json
  MODULE_STD
  SOURCES
    src/json.cppm
    src/json_value.cppm
)
target_link_libraries(reflex.serde_json PUBLIC reflex.serde)

reflex_add_cxx_module_library(reflex.serde_bson
  MODULE_STD
  SOURCES
    src/bson.cppm
    src/bson_value.cppm
)
target_link_libraries(reflex.serde_bson PUBLIC reflex.serde)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()