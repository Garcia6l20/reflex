# Require GCC
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "The std module bootstrap only supports GCC.")
endif()

# Require GCC >= 16
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR
        "GCC >= 16 required for libstdc++ std module "
        "(found ${CMAKE_CXX_COMPILER_VERSION})")
endif()

message(STATUS "Using GCC ${CMAKE_CXX_COMPILER_VERSION}")

if(UNIX)
    # Locate libstdc++.so to determine GCC prefix
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++.so
        OUTPUT_VARIABLE LIBSTDCPP_SO
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT EXISTS "${LIBSTDCPP_SO}")
        message(FATAL_ERROR "Failed to locate libstdc++.so")
    endif()

    get_filename_component(GCC_LIB_DIR "${LIBSTDCPP_SO}" DIRECTORY)
    get_filename_component(GCC_PREFIX "${GCC_LIB_DIR}" DIRECTORY)

    message(STATUS "GCC prefix: ${GCC_PREFIX}")

    # libstdc++ headers root
    set(LIBSTDCPP_INCLUDE
        "${GCC_PREFIX}/include/c++/${CMAKE_CXX_COMPILER_VERSION}"
    )
elseif(WIN32)
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=include/c++
        OUTPUT_VARIABLE LIBSTDCPP_INCLUDE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(NOT EXISTS "${LIBSTDCPP_INCLUDE}")
    message(FATAL_ERROR
        "libstdc++ headers not found: ${LIBSTDCPP_INCLUDE}")
endif()

message(STATUS "libstdc++ include root: ${LIBSTDCPP_INCLUDE}")

# Module sources
set(STD_MODULE_SRC
    "${LIBSTDCPP_INCLUDE}/bits/std.cc"
)

set(STD_COMPAT_MODULE_SRC
    "${LIBSTDCPP_INCLUDE}/bits/std.compat.cc"
)

foreach(f ${STD_MODULE_SRC} ${STD_COMPAT_MODULE_SRC})
    if(NOT EXISTS "${f}")
        message(FATAL_ERROR "Missing std module source: ${f}")
    endif()
endforeach()

message(STATUS "std module: ${STD_MODULE_SRC}")
message(STATUS "std.compat module: ${STD_COMPAT_MODULE_SRC}")

# Module target
add_library(std)

target_sources(std
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${LIBSTDCPP_INCLUDE}
        FILES
            ${STD_MODULE_SRC}
            ${STD_COMPAT_MODULE_SRC}
)

# Constexpr all the things + reflection
target_compile_options(std
    PUBLIC
        -fimplicit-constexpr
        -freflection
)
