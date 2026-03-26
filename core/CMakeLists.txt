reflex_add_cxx_module_library(reflex.core
  src/parse.cpp
  src/core.cppm

  include/reflex/visit.hpp
  include/reflex/exception.hpp
  include/reflex/heapless/string.hpp
  include/reflex/heapless/vector.hpp
  include/reflex/parse.hpp
  include/reflex/to_tuple.hpp
  include/reflex/constant_string.hpp
  include/reflex/formatters.hpp
  include/reflex/const_assert.hpp
  include/reflex/core.hpp
  include/reflex/meta.hpp
  include/reflex/views/cartesian_product.hpp
  include/reflex/concepts.hpp
  include/reflex/caseconv.hpp
  include/reflex/utils.hpp
  include/reflex/named_arg.hpp
  include/reflex/meta/reg.hpp
  include/reflex/match.hpp
)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
