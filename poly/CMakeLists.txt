reflex_add_cxx_module_library(reflex.poly
  src/poly.cppm

  include/reflex/poly.hpp
  include/reflex/poly/var.hpp
)
target_link_libraries(reflex.poly PUBLIC reflex.core)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
