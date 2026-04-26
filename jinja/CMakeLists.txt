reflex_add_cxx_module_library(reflex.jinja
  INSTALL_TARGETS reflex-targets

  modules/reflex/jinja.cppm

  include/reflex/jinja.hpp
  include/reflex/jinja/expr.hpp
  include/reflex/jinja/context.hpp
)
target_link_libraries(reflex.jinja PUBLIC reflex::core reflex::serde::json)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
