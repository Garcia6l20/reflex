reflex_add_cxx_module_library(reflex.core
  MODULE_STD
  SOURCES
    src/core.cppm
    src/concepts.cppm
    src/utils.cppm
    src/views/cartesian_product.cppm
    src/meta.cppm
    src/to_tuple.cppm
    src/constant_string.cppm
    src/parse.cppm
    src/visit.cppm
    src/match.cppm
    src/caseconv.cppm
    src/const_assert.cppm
    src/formatters.cppm
    src/meta/tu.cppm
)
target_compile_options(reflex.core PUBLIC -fabi-version=19)
target_compile_options(reflex.core PUBLIC -freflection -fimplicit-constexpr)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
