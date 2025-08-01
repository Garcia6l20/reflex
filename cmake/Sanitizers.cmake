include(CheckCXXCompilerFlag)

add_library(__all_sanitizers INTERFACE)
add_library(sanitizers::all ALIAS __all_sanitizers)

add_library(__sanitizer_base INTERFACE)
target_compile_options(__sanitizer_base INTERFACE -Werror)

set(ENABLED_SANITIZERS "" CACHE INTERNAL "List of enabled sanitizers")
if (NOT ENABLED_SANITIZERS)
    return()
endif()

message(STATUS "ENABLED_SANITIZERS: ${ENABLED_SANITIZERS}")

macro(add_sanitizer name flags)
    foreach(flag ${flags})
        check_cxx_compiler_flag(${flag} __has_flag)
        if (NOT __has_flag)
            message(STATUS "sanitizer ${name} disabled (missing flag: ${flag})")
            return()
        endif()
    endforeach()
    

    add_library(__${name}_sanitizer INTERFACE)
    target_link_libraries(__${name}_sanitizer INTERFACE __sanitizer_base)
    target_compile_options(__${name}_sanitizer INTERFACE ${flags})
    target_link_options(__${name}_sanitizer INTERFACE ${flags})
    add_library(sanitizers::${name} ALIAS __${name}_sanitizer)

    if (${name} IN_LIST ENABLED_SANITIZERS)
        target_link_libraries(__all_sanitizers INTERFACE __${name}_sanitizer)
    endif()
endmacro()


add_sanitizer(undefined -fsanitize=undefined)
add_sanitizer(leak -fsanitize=leak)
add_sanitizer(memory -fsanitize=memory)
add_sanitizer(address -fsanitize=address)
add_sanitizer(thread -fsanitize=thread -Wno-error=tsan)


