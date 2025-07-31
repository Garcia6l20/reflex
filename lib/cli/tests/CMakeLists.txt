link_libraries(reflex.cli)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

reflex_add_test(reflex-cli-calculator-test test-calculator.cpp)
