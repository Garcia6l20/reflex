reflex_add_cxx_module_library(reflex.cli
  src/cli.cpp
  src/cli.cppm
  src/cli-base.cppm
  src/cli-completion.cppm
)
target_link_libraries(reflex.cli PUBLIC reflex.core)

if (REFLEX_HAVE_TERMIOS_H AND REFLEX_HAVE_UNISTD_H)
  reflex_add_cxx_module_library(reflex.shell
    src/shell.cppm
    src/shell-term.cppm
  )
  target_link_libraries(reflex.shell PUBLIC reflex.cli)
else()
  message(STATUS "termios.h and unistd.h not found, skipping reflex.shell module")
endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
