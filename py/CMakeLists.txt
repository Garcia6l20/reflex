# reflex.py exposes headers only. Building an extension is nanobind's job:
# nanobind_add_module() compiles its own runtime, and reflex::py adds the binder
# on top.
#
#   find_package(reflex REQUIRED)
#   find_package(nanobind CONFIG REQUIRED)
#   nanobind_add_module(my_ext my_ext.cpp)
#   target_link_libraries(my_ext PRIVATE reflex::py)
#
# Nothing here fetches anything. Point CMake at an existing nanobind:
#   -Dnanobind_ROOT=$(python3 -m nanobind --cmake_dir)

find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(nanobind CONFIG REQUIRED)

reflex_add_cxx_module_library(reflex.py
  INSTALL_TARGETS reflex-targets

  modules/reflex/py.cppm

  include/reflex/py.hpp
  include/reflex/py/annotations.hpp
  include/reflex/py/bind.hpp
  include/reflex/py/nanobind.hpp
  include/reflex/py/operators.hpp
  include/reflex/py/policy.hpp
)
target_link_libraries(reflex.py PUBLIC reflex::core Python::Module)

# SYSTEM: the project builds with -Wall -Wextra -Werror and nanobind is not ours
# to keep clean. Only the headers are needed; tsl/robin_map is a dependency of
# nanobind's runtime sources, which this target does not compile.
target_include_directories(reflex.py SYSTEM PUBLIC ${NB_DIR}/include)

# NB_DOMAIN is deliberately not set. It keys the type registry, so every
# translation unit that meets another has to agree on it, and nanobind_add_module
# builds its own runtime without seeing anything set here. The default domain is
# the conventional choice and lets a reflex extension share types with any other
# nanobind one.

# An extension is a shared object, so anything it links has to be relocatable.
set_target_properties(reflex.py PROPERTIES POSITION_INDEPENDENT_CODE ON)
