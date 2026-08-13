#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cstdint>

#include <reflex/serde/poly.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde::toml
{
  using null_t = reflex::poly::null_t;

  constexpr null_t null{};

  // Integer and Float are two distinct TOML types rather than two spellings of
  // one, so they are two alternatives here: collapsing both into a double would
  // cost every integer past 2^53 its value.
  //
  // `float` is a keyword and `Float` is the spec's own name for the type, so the
  // pair reads integer/number rather than integer/float.
  using integer = std::int64_t;
  using number  = double;
  using boolean = bool;
  using string  = std::string;

  // The four date-time types are carried as `string`, verbatim.
  //
  // TOML has no null. null_t is still an alternative because poly::var has one
  // whether it is named here or not, and because it is the natural spelling of
  // an absent value on the way in. The serializer refuses to write one.
  using value  = poly::var<integer, number, boolean, string>;
  using object = value::obj_type;
  using array  = value::arr_type;

} // namespace reflex::serde::toml
