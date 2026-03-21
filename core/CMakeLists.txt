reflex_add_cxx_module_library(reflex.core
  src/parse.cpp
)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
