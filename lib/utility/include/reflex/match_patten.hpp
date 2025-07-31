#pragma once

namespace reflex
{
template <class... Ts> struct match_pattern : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> match_pattern(Ts...) -> match_pattern<Ts...>;
} // namespace reflex
