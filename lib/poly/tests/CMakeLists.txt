link_libraries(reflex.poly)

include(CPM)
CPMAddPackage("gh:catchorg/Catch2@3.4.0")

link_libraries(reflex.cli Catch2::Catch2WithMain)
add_executable(test-var test-var.cpp)
add_executable(test-recursive-var test-recursive-var.cpp)