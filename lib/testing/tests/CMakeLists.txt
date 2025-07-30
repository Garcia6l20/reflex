link_libraries(reflex.testing)

include(CPM)
CPMAddPackage("gh:catchorg/Catch2@3.4.0")

link_libraries(reflex.cli Catch2::Catch2)
include_directories(${CMAKE_BINARY_DIR}/_deps/catch2-src/src/catch2/internal)
add_executable(test-testcase test-testcase.cpp)