reflex_add_cxx_module_library(reflex.cli
  src/cli.cpp
  src/cli.cppm
  src/cli-base.cppm
  src/cli-completion.cppm
)
target_link_libraries(reflex.cli PUBLIC reflex.core)

reflex_add_cxx_module_library(reflex.shell
  src/shell.cppm
  src/shell-term.cppm
)
target_link_libraries(reflex.shell PUBLIC reflex.cli)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
