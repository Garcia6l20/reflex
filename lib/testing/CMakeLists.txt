add_library(reflex.testing STATIC src/test_main.cpp)
target_include_directories(reflex.testing PUBLIC include)
target_link_libraries(reflex.testing PUBLIC reflex.utility PRIVATE reflex.cli)

if(REFLEX_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
