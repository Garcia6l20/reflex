reflex_add_cxx_module_library(reflex.serde
  src/serde.cppm

  include/reflex/serde.hpp
  include/reflex/serde/annotations.hpp
  include/reflex/serde/object_visit.hpp
  include/reflex/serde/poly.hpp
)
target_link_libraries(reflex.serde PUBLIC reflex.core reflex.poly)

reflex_add_cxx_module_library(reflex.serde_json
  src/serde_json.cppm

  include/reflex/serde/json_value.hpp
  include/reflex/serde/json.hpp
)
target_link_libraries(reflex.serde_json PUBLIC reflex.serde)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()