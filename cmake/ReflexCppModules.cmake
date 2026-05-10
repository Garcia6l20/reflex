if (CMAKE_VERSION VERSION_LESS 4.2)
    message(FATAL_ERROR "CMake 4.2 or higher is required to build this project.")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_BMI_DIRECTORY "${CMAKE_BINARY_DIR}/bmi")

set(CMAKE_CXX_SCAN_FOR_MODULES ${REFLEX_CXX_MODULES_ENABLED})
set(CMAKE_CXX_MODULE_STD ${REFLEX_CXX_MODULES_ENABLED})


# see: https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Dialect-Options.html#index-fabi-version
# summary:
#   - fixes unnecessary captures in noexcept lambdas (c++/119764)
#   - layout of a base class with all explicitly defaulted constructors (c++/120012)
#   - and mangling of class and array objects with implicitly zero-initialized non-trailing subobjects (c++/121231)
set(REFLEX_CXX_ABI_VERSION 21)
set(REFLEX_REQUIRED_FLAGS
    -fimplicit-constexpr
    -freflection
    -Wabi=${REFLEX_CXX_ABI_VERSION}
    -fabi-version=${REFLEX_CXX_ABI_VERSION}
)

if (CMAKE_CXX_MODULE_STD)
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

  add_compile_options(${REFLEX_REQUIRED_FLAGS})
endif()

function(reflex_add_cxx_module_library target)

  set(options SHARED STATIC MODULE_STD)
  set(oneValueArgs INSTALL_TARGETS EXPORT_NAME)
  set(multiValueArgs)
  cmake_parse_arguments(ARGS
    "${options}" "${oneValueArgs}" "${multiValueArgs}"
    ${ARGN}
  )

  set(_type STATIC)

  if (ARGS_SHARED)
    if (ARGS_STATIC)
      message(FATAL_ERROR "Cannot use SHARED and STATIC together")
    endif()
    set(_type SHARED)
  endif()

  if (ARGS_STATIC)
    if (ARGS_SHARED)
      message(FATAL_ERROR "Cannot use SHARED and STATIC together")
    endif()
    set(_type STATIC)
  endif()

  set(_all_sources ${ARGS_UNPARSED_ARGUMENTS})
  if (NOT _all_sources)
    set(_type INTERFACE)
  endif()

  set(_cpp_sources ${_all_sources})
  list(FILTER _cpp_sources INCLUDE REGEX ".*\\.cpp$")

  set(_cppm_sources ${_all_sources})
  list(FILTER _cppm_sources INCLUDE REGEX ".*\\.cppm$")

  set(_hpp_sources ${_all_sources})
  list(FILTER _hpp_sources INCLUDE REGEX ".*\\.h$|.*\\.hpp$")

  if (${_type} STREQUAL INTERFACE)
    set(_public_dep_mode INTERFACE)
  else()
    set(_public_dep_mode PUBLIC)
  endif()

  add_library(${target} ${_type} ${_cpp_sources})
  if (_hpp_sources)
    set(_public_hpp_source ${_hpp_sources})
    list(FILTER _public_hpp_source INCLUDE REGEX ".?include/.*\\.h(pp)?$")
    set(_private_hpp_source ${_hpp_sources})
    list(FILTER _private_hpp_source EXCLUDE REGEX ".?include/.*\\.h(pp)?$")
    if (_public_hpp_source)
      target_sources(${target}
        ${_public_dep_mode}
          FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
            FILES ${_public_hpp_source}
      )
    endif()
    if (_private_hpp_source)
      target_sources(${target}
        PRIVATE
          FILE_SET private_headers
            TYPE HEADERS
            FILES ${_private_hpp_source}
      )
    endif()
  endif()

  if (_cppm_sources AND REFLEX_CXX_MODULES_ENABLED)
    set(_public_cppm_source ${_cppm_sources})
    list(FILTER _public_cppm_source INCLUDE REGEX ".?modules/.*\\.cppm$")
    set(_private_cppm_source ${_cppm_sources})
    list(FILTER _private_cppm_source EXCLUDE REGEX ".?modules/.*\\.cppm$")

    if (_public_cppm_source)
      target_sources(${target}
        PUBLIC
          FILE_SET cxx_modules
          TYPE CXX_MODULES
          BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
          FILES ${_public_cppm_source}
      )
    endif()
    if (_private_cppm_source)
      target_sources(${target}
        PUBLIC
          FILE_SET cxx_modules
          TYPE CXX_MODULES
          FILES ${_private_cppm_source}
      )
    endif()
    set_property(TARGET ${target} PROPERTY CXX_MODULE_BMI_DIRECTORY "${CMAKE_CXX_MODULE_BMI_DIRECTORY}")
  endif()

  target_compile_features(${target} ${_public_dep_mode} cxx_std_26)
  target_include_directories(${target} ${_public_dep_mode} 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )

  # if target contains dots (e.g. reflex.poly), create an alias with cmake namespace style (e.g. reflex::poly)
  if (target MATCHES "\\.")
    string(REPLACE "." "::" alias_target ${target})
    add_library(${alias_target} ALIAS ${target})

    if (NOT ARGS_EXPORT_NAME)
      # remove prefix and use it as EXPORT_NAME
      string(REGEX REPLACE "^[^\\.]+\\." "" export_name ${target})
      set_target_properties(${target} PROPERTIES EXPORT_NAME ${export_name})
    endif()

  endif()

  if (ARGS_EXPORT_NAME)
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${ARGS_EXPORT_NAME})
  endif()

  if (ARGS_INSTALL_TARGETS AND REFLEX_INSTALL)
    install(TARGETS ${target}
      EXPORT ${ARGS_INSTALL_TARGETS}
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      FILE_SET public_headers DESTINATION include
      FILE_SET cxx_modules DESTINATION lib/cxx
    )
  endif()

endfunction()
