add_library(reflex.utility INTERFACE)
target_include_directories(reflex.utility INTERFACE include)

# if(REFLEX_BUILD_EXAMPLES)
#   add_subdirectory(examples)
# endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
