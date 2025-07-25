add_library(reflex.di INTERFACE)
target_include_directories(reflex.di INTERFACE include)
target_link_libraries(reflex.di INTERFACE reflex.utility)

if(REFLEX_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
