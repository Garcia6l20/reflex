cmake_minimum_required(VERSION 4.2)

# must be set before project()
# it setup cmake 'import std' support
# and setup std module required flags
find_package(reflex CONFIG REQUIRED)

project(reflex-example LANGUAGES CXX)


add_executable(hello-cli hello-cli.cpp)
target_link_libraries(hello-cli PRIVATE reflex::cli)
