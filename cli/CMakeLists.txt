add_library(reflex.cli
  src/cli.cpp
)
target_sources(reflex.cli
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
          src/cli.cppm
)
target_link_libraries(reflex.cli PUBLIC reflex.core)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
