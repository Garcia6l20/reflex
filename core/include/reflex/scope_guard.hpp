#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <type_traits>
#include <functional>
#endif

REFLEX_EXPORT namespace reflex
{
template <typename Fn> class scope_guard
{
public:
  scope_guard(Fn&& fn) noexcept : fn_{static_cast<Fn&&>(fn)}
  {}
  scope_guard(Fn const& fn) noexcept : fn_{fn}
  {}
  scope_guard(scope_guard&&)                 = delete;
  scope_guard(scope_guard const&)            = delete;
  scope_guard& operator=(scope_guard&&)      = delete;
  scope_guard& operator=(scope_guard const&) = delete;

  ~scope_guard() noexcept(std::is_nothrow_invocable_v<Fn>)
  {
    if(enabled_)
    {
      std::invoke(fn_);
    }
  }
  void disable() noexcept
  {
    enabled_ = false;
  }

private:
  bool enabled_{true};
  Fn   fn_;
};
} // namespace reflex