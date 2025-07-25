add_library(reflex.poly INTERFACE)
target_include_directories(reflex.poly INTERFACE include)
target_link_libraries(reflex.poly INTERFACE reflex.utility)

# if(REFLEX_BUILD_EXAMPLES)
#   add_subdirectory(examples)
# endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
