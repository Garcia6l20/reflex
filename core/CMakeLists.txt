reflex_add_cxx_module_library(reflex.core
  MODULE_STD
  SOURCES
    src/core.cppm
    src/utils.cppm
    src/views/cartesian_product.cppm
    src/meta.cppm
    src/to_tuple.cppm
    src/constant_string.cppm
)
target_include_directories(reflex.core INTERFACE include)
target_compile_options(reflex.core PRIVATE -fabi-version=19)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
