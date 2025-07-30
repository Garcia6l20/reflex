#pragma once

#include <reflex/meta.hpp>

namespace reflex::testing
{

template <typename T, meta::info R> struct reflected_ref
{
  std::reference_wrapper<T> r_;

  static constexpr auto info = R;

  template <typename Self> decltype(auto) get(this Self&& self) noexcept
  {
    return self.r_.get();
  }

  operator T&() noexcept
  {
    return get();
  }
  operator T const&() const& noexcept
  {
    return get();
  }
};

consteval bool is_reflected_ref(meta::info R)
{
  return has_template_arguments(decay(R)) and template_of(decay(R)) == ^^reflected_ref;
}

#define rr(_obj_) reflex::testing::reflected_ref<std::remove_reference_t<decltype(_obj_)>, ^^_obj_>(std::ref(_obj_))
} // namespace reflex::testing

template <typename T, std::meta::info R, typename CharT>
struct std::formatter<reflex::testing::reflected_ref<T, R>, CharT> : std::formatter<std::remove_const_t<T>, CharT>
{
  using super = std::formatter<std::remove_const_t<T>, CharT>;

  decltype(auto) format(reflex::testing::reflected_ref<T, R> const& r, auto& ctx) const
  {
    return super::format(static_cast<T const&>(r), ctx);
  }
};
