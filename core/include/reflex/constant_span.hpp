#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/meta.hpp>

REFLEX_EXPORT namespace reflex
{
  template <typename T, std::size_t extent> struct constant_span
  {
    using value_type = T;
    using span_type  = std::span<T, extent>;

    const T* const    data_ = nullptr;
    const std::size_t size_ = 0;

    constexpr constant_span() = default;

    // template <std::size_t N> constexpr constant_span(const T (&arr)[N])
    // {
    //   static_assert(N == extent, "Array size must match constant_span extent");
    //   data_ = define_static_array(std::span<const T, N>(arr));
    //   size_ = N;
    // }

    constexpr constant_span(std::span<T, extent> sp) : data_(sp.data()), size_(sp.size())
    {}

    constexpr auto size() const noexcept
    {
      return size_;
    }

    constexpr auto view() const noexcept
    {
      return std::span<T>(data_, size_);
    }

    constexpr decltype(auto) operator[](std::size_t ii) const noexcept
    {
      return data_[ii];
    }

    constexpr operator span_type() const noexcept
    {
      return view();
    }

    constexpr auto begin() const noexcept
    {
      return view().begin();
    }

    constexpr auto end() const noexcept
    {
      return view().end();
    }

    constexpr auto operator*() const noexcept
    {
      return view();
    }
  };
} // namespace reflex
