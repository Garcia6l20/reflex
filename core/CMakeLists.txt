reflex_add_cxx_module_library(reflex.core)
# target_compile_options(reflex.core PUBLIC -fabi-version=19)
target_compile_options(reflex.core INTERFACE -freflection -fimplicit-constexpr)
target_link_libraries(reflex.core INTERFACE std)

target_compile_definitions(reflex.core INTERFACE $<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:Release>:NDEBUG>)

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
