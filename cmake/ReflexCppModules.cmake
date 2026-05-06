if (CMAKE_VERSION VERSION_LESS 4.2)
    message(FATAL_ERROR "CMake 4.2 or higher is required to build this project.")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_BMI_DIRECTORY "${CMAKE_BINARY_DIR}/bmi")
set(CMAKE_CXX_MODULE_GCM_CACHE "${CMAKE_BINARY_DIR}/gcm.cache")

set(CMAKE_CXX_SCAN_FOR_MODULES ${REFLEX_CXX_MODULES_ENABLED})
set(CMAKE_CXX_MODULE_STD ${REFLEX_CXX_MODULES_ENABLED})

option(REFLEX_CXX_MODULES_USE_GLOBAL_CACHE "Use global cache for C++ modules" ON)

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

enable_language(CXX)


if (REFLEX_CXX_MODULES_USE_GLOBAL_CACHE AND NOT TARGET __reflex_dummy)

  # gcc only
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Global cache for C++ modules is only supported with GCC")
  endif()

  # string(CONCAT CMAKE_CXX_SCANDEP_SOURCE
  #   "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E -x c++ <SOURCE>"
  #   " -MT <DYNDEP_FILE> -MD -MF <DEP_FILE>"
  #   " -fmodules-ts -fdeps-file=<DYNDEP_FILE> -fdeps-target=<OBJECT> -fdeps-format=p1689r5"
  #   " -o <PREPROCESSED_SOURCE>")
  # set(CMAKE_CXX_MODULE_MAP_FORMAT "gcc")
  string(CONCAT CMAKE_CXX_MODULE_MAP_FLAG
    # Turn on modules.
    "-fmodules"
    # Read the module mapper file.
     # here is the hack ! we don't use any mapper when using global cache, since the mapper is only needed to point to the BMI files, but with global cache.
    # " -fmodule-mapper=<MODULE_MAP_FILE>"
    # Make sure dependency tracking is enabled (missing from `try_*`).
    " -MD"
    # Suppress `CXX_MODULES +=` from generated depfile snippets.
    " -fdeps-format=p1689r5"
    # Force C++ as a language.
    " -x c++")
  set(CMAKE_CXX_MODULE_BMI_ONLY_FLAG "-fmodule-only")

  # create a dummy target that generates the std modules
  add_executable(__reflex_dummy
    ${CMAKE_CURRENT_LIST_DIR}/dummy.cpp
  )
  add_custom_command(
    TARGET __reflex_dummy
    POST_BUILD
    # hack: cmake tracks dependency within its own CMakeFiles directory
    COMMAND cp --preserve=timestamps
        ${CMAKE_BINARY_DIR}/gcm.cache/std.gcm
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__cmake_cxx_std_${CMAKE_CXX_STANDARD}.dir/std.gcm
    COMMAND cp --preserve=timestamps
        ${CMAKE_BINARY_DIR}/gcm.cache/std.compat.gcm
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__cmake_cxx_std_${CMAKE_CXX_STANDARD}.dir/std.compat.gcm
    COMMENT "Copying module BMI files for reflex-dummy"
  )

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

  if (_cppm_sources OR _cppm_private_sources)
    # hack: cmake tracks dependency within its own CMakeFiles directory
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND cp --preserve=timestamps
        ${CMAKE_CXX_MODULE_GCM_CACHE}/*.gcm
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir/
      COMMENT "Copying module library for ${target}"
    )
  endif()

  if (ARGS_INSTALL_TARGETS AND REFLEX_INSTALL)
    install(TARGETS ${target}
      EXPORT ${ARGS_INSTALL_TARGETS}
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      FILE_SET public_headers DESTINATION include
      FILE_SET cxx_modules DESTINATION share/cxxmodules
    )
  endif()

  if (REFLEX_CXX_MODULES_USE_GLOBAL_CACHE)
    add_dependencies(${target} __reflex_dummy)
  endif()

endfunction()



function(_reflex_resolve_header out_var header include_dirs)
    foreach(dir IN LISTS include_dirs)
        if(EXISTS "${dir}/${header}")
            set(${out_var} "${dir}/${header}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Fallback: let compiler try
    set(${out_var} "${header}" PARENT_SCOPE)
endfunction()

function(reflex_add_cxx_importable_header_module target)

    set(options)
    set(oneValueArgs)
    set(multiValueArgs HEADERS LIBRARIES INCLUDE_DIRECTORIES)
    cmake_parse_arguments(ARGS
      "${options}" "${oneValueArgs}" "${multiValueArgs}"
      ${ARGN}
    )
    
    set(headers ${ARGS_HEADERS})
    set(libraries ${ARGS_LIBRARIES})

    if (NOT headers)
        message(FATAL_ERROR "No headers provided to reflex_add_cxx_importable_header_module")
    endif()

    set(gcm_dir ${CMAKE_BINARY_DIR}/gcm.cache)

    set(module_targets)

    # Collect include dirs from libraries
    set(include_dirs ${ARGS_INCLUDE_DIRECTORIES})
    set(include_dirs_flags ${include_dirs})
    list(TRANSFORM include_dirs_flags PREPEND -I)
    foreach(lib IN LISTS libraries)
      if (TARGET ${lib})
        # Get include directories from target properties
        get_target_property(lib_includes ${lib} INTERFACE_INCLUDE_DIRECTORIES)
        if(lib_includes)
          foreach(inc IN LISTS lib_includes)
            list(APPEND include_dirs ${inc})
            list(APPEND include_dirs_flags -I${inc})
          endforeach()
        endif()
      endif()
    endforeach()

    foreach(header IN LISTS headers)
    
        # Detect system header (<...>)
        set(is_system FALSE)
        if(header MATCHES "^<.*>$")
            set(is_system TRUE)
            string(REGEX REPLACE "[<>]" "" header_rel "${header}")
            set(import_name "<${header_rel}>")
            _reflex_resolve_header(header_path "${header_rel}" "${include_dirs}")
        else()
            get_filename_component(header_path ${header} ABSOLUTE)
            get_filename_component(header_name ${header} NAME)
            set(import_name "${header_name}")
        endif()

        # Sanitize name
        set(safe_name ${header})
        string(REPLACE "/" "_" safe_name "${safe_name}")
        string(REPLACE "\\" "_" safe_name "${safe_name}")
        string(REPLACE "." "_" safe_name "${safe_name}")
        string(REPLACE "<" "" safe_name "${safe_name}")
        string(REPLACE ">" "" safe_name "${safe_name}")
        string(REPLACE "-" "_" safe_name "${safe_name}")

        # set(gcm_file ${gcm_dir}/${safe_name}.gcm)
        set(gcm_file ${gcm_dir}/${header_path}.gcm)
        set(module_target reflex_module_${target}_${safe_name})

        if(is_system)
            set(header_cmd -x c++-system-header ${header_path})
        else()
            set(header_cmd -x c++-header ${header_path})
        endif()

        message(STATUS "Adding header module ${import_name} -> ${gcm_file}")
        add_custom_command(
            OUTPUT ${gcm_file}
            COMMAND ${CMAKE_CXX_COMPILER}
                    -std=c++${CMAKE_CXX_STANDARD}
                    -fmodules
                    ${include_flags}
                    ${header_cmd}
                    ${include_dirs_flags}
                    ${REFLEX_REQUIRED_FLAGS}

            DEPENDS ${header_path}
            COMMENT "Building GCC header unit ${header}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            VERBATIM
        )

        add_custom_target(${module_target} DEPENDS ${gcm_file})
        list(APPEND module_targets ${module_target})

        file(APPEND ${mapper_file} "${import_name} ${gcm_file}\n")
    endforeach()

    add_library(${target} INTERFACE)

    # Make sure all header units are built before target
    add_dependencies(${target} ${module_targets})

    target_include_directories(${target} INTERFACE
        ${CMAKE_SOURCE_DIR}
        ${include_dirs}
    )
    target_link_libraries(${target} INTERFACE ${libraries})

    # Clean support
    set_property(DIRECTORY APPEND PROPERTY
        ADDITIONAL_MAKE_CLEAN_FILES
        ${gcm_dir}
        ${mapper_file}
    )
endfunction()

