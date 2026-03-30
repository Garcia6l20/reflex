cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(ReflexCppModules)

project(reflex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

option(REFLEX_BUILD_PROGRAMS "Build reflex programs" ON)
option(REFLEX_BUILD_EXAMPLES "Build reflex examples" OFF)
option(REFLEX_BUILD_TESTS "Build reflex tests" OFF)

option(REFLEX_CXX_MODULES_ENABLED "Enable Reflex CXX Modules" ON)

if (REFLEX_CXX_MODULES_ENABLED)
  message(STATUS "C++ modules enabled")
  add_subdirectory(std)
  link_libraries(std)
  add_compile_definitions(REFLEX_CXX_MODULES_ENABLED=1)
else()
  add_compile_definitions(REFLEX_CXX_MODULES_ENABLED=0)
endif()

include(CMakeDependentOption)

option(REFLEX_CORE_ENABLED "Enable Reflex Core" ON)
cmake_dependent_option(REFLEX_CLI_ENABLED "Enable Reflex CLI" ON
  REFLEX_CORE_ENABLED OFF
)
cmake_dependent_option(REFLEX_POLY_ENABLED "Enable Reflex Poly" ON
  REFLEX_CORE_ENABLED OFF
)
cmake_dependent_option(REFLEX_SERDE_ENABLED "Enable Reflex Serde" ON
  "REFLEX_CORE_ENABLED AND REFLEX_POLY_ENABLED" OFF
)
cmake_dependent_option(REFLEX_JINJA_ENABLED "Enable Reflex Jinja" ON
  REFLEX_SERDE_ENABLED OFF
)


include(ReflexRequirements)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

if(REFLEX_BUILD_TESTS)
  include(ReflexTesting)
  enable_testing()
endif()

if(REFLEX_CORE_ENABLED)
  add_subdirectory(core)
endif()
if(REFLEX_CLI_ENABLED)
  add_subdirectory(cli)
endif()
if(REFLEX_POLY_ENABLED)
  add_subdirectory(poly)
endif()
if(REFLEX_SERDE_ENABLED)
  add_subdirectory(serde)
endif()
if(REFLEX_JINJA_ENABLED)
  add_subdirectory(jinja)
endif()

