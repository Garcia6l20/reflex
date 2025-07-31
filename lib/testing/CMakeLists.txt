add_library(reflex.testing STATIC src/test_main.cpp)
target_include_directories(reflex.testing PUBLIC include)
target_link_libraries(reflex.testing PUBLIC reflex.utility PRIVATE reflex.cli)
add_subdirectory(tests)
