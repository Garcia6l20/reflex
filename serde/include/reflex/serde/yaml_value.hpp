#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/serde/poly.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde::yaml
{
  using null_t = reflex::poly::null_t;

  constexpr null_t null{};

  using string  = std::string;
  using number  = double;
  using boolean = bool;

  // Deliberately the same alternatives as json::value, so a document converted
  // between the two backends keeps its shape. It also inherits json's limit:
  // an integer past 2^53 loses precision on the way through.
  using value  = poly::var<number, boolean, string>;
  using object = value::obj_type;
  using array  = value::arr_type;

} // namespace reflex::serde::yaml
