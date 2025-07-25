add_library(reflex.cli INTERFACE)
target_include_directories(reflex.cli INTERFACE include)
target_link_libraries(reflex.cli INTERFACE reflex.utility)

if(REFLEX_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
