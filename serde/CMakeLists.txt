reflex_add_cxx_module_library(reflex.serde
  MODULE_STD
  SOURCES
    src/serde.cppm
)
target_link_libraries(reflex.serde PUBLIC reflex.core)

reflex_add_cxx_module_library(reflex.serde_json
  MODULE_STD
  SOURCES
    src/json.cppm
    src/json_value.cppm
)
target_link_libraries(reflex.serde_json PUBLIC reflex.serde reflex.poly)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()