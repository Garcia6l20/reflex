#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/serde/poly.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde::json
{
  using null_t = reflex::poly::null_t;

  constexpr null_t null{};

  using string  = std::string;
  using number  = double;
  using boolean = bool;

  using value  = poly::var<number, boolean, string>;
  using object = value::obj_type;
  using array  = value::arr_type;

} // namespace reflex::serde::json
