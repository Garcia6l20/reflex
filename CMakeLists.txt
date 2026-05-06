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
option(REFLEX_INSTALL "Install Reflex targets" ON)

add_compile_options(-Wall -Wextra -Werror
  -fconcepts-diagnostics-depth=5 -fmax-errors=10
)

add_library(reflex.common_flags INTERFACE)
add_library(reflex::common_flags ALIAS reflex.common_flags)
target_compile_options(reflex.common_flags INTERFACE ${REFLEX_REQUIRED_FLAGS})

if (REFLEX_CXX_MODULES_ENABLED)
  message(STATUS "C++ modules enabled")
  target_compile_definitions(reflex.common_flags INTERFACE REFLEX_CXX_MODULES_ENABLED=1)
  # add_subdirectory(std)
  # target_link_libraries(reflex.std PUBLIC reflex.common_flags)
else()
  target_compile_definitions(reflex.common_flags INTERFACE REFLEX_CXX_MODULES_ENABLED=0)

  # add_library(reflex.std ALIAS reflex.common_flags)
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

if (REFLEX_INSTALL)

  install(TARGETS reflex.common_flags
    EXPORT reflex-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
  )

  include(CMakePackageConfigHelpers)

  configure_package_config_file(
    cmake/reflexConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/reflexConfig.cmake
    INSTALL_DESTINATION lib/cmake/reflex
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/reflexConfig.cmake
    DESTINATION lib/cmake/reflex
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/reflexConfigVersion.cmake
    VERSION 0.0.1
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/reflexConfigVersion.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflexCppModules.cmake
    DESTINATION lib/cmake/reflex
  )

  install(EXPORT reflex-targets
    NAMESPACE reflex::
    DESTINATION lib/cmake/reflex
  )

  include(CPack)
endif()
