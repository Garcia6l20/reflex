/** @file
 * @brief reflection-driven Python bindings, on top of nanobind
 *
 * @code
 * REFLEX_PY_MODULE(my_ext, m)
 * {
 *   m.bind<my_class>();
 * }
 * @endcode
 */
#pragma once

#include <reflex/py/annotations.hpp>
#include <reflex/py/bind.hpp>
#include <reflex/py/nanobind.hpp>
#include <reflex/py/policy.hpp>
