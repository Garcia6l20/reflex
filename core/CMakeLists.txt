reflex_add_cxx_module_library(reflex.core
  INSTALL_TARGETS reflex-targets

  src/parse.cpp
  modules/reflex/core.cppm

  include/reflex/visit.hpp
  include/reflex/exception.hpp
  include/reflex/heapless/string.hpp
  include/reflex/heapless/vector.hpp
  include/reflex/parse.hpp
  include/reflex/to_tuple.hpp
  include/reflex/ctp/ctp.hpp
  include/reflex/ctp/core.hpp
  include/reflex/ctp/custom.hpp
  include/reflex/ctp/param.hpp
  include/reflex/ctp/serialize.hpp
  include/reflex/constant.hpp
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
  include/reflex/diags.hpp
  include/reflex/enum.hpp
  include/reflex/scope_guard.hpp
  include/reflex/format.hpp
  include/reflex/derive.hpp
)
target_link_libraries(reflex.core
  PUBLIC
    reflex::common_flags
)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
