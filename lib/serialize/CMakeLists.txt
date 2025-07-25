add_library(reflex.serialize INTERFACE)
target_include_directories(reflex.serialize INTERFACE include)
target_link_libraries(reflex.serialize INTERFACE reflex.utility)

# if(REFLEX_BUILD_EXAMPLES)
#   add_subdirectory(examples)
# endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
