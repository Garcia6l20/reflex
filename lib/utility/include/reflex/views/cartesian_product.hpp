#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace reflex
{
namespace ranges
{

template <std::size_t N, std::ranges::forward_range R>
class cartesian_product_view : public std::ranges::view_interface<cartesian_product_view<N, R>>
{
  R base_;

  using iterator_t      = std::ranges::borrowed_iterator_t<const R>;
  using reference_t     = std::ranges::range_reference_t<const R>;
  using value_element_t = std::remove_cvref_t<reference_t>;

  struct iterator
  {
    const cartesian_product_view* parent_ = nullptr;
    std::array<iterator_t, N>     its_;
    bool                          end_ = false;

    constexpr iterator() = default;

    constexpr iterator(const cartesian_product_view* p, bool end) : parent_(p), end_(end)
    {
      if(!end)
      {
        auto first = std::ranges::begin(parent_->base_);
        auto last  = std::ranges::end(parent_->base_);
        if(first == last)
        {
          end_ = true;
          return;
        }
        its_.fill(first);
      }
    }

    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type        = std::array<value_element_t, N>;
    using reference         = std::array<value_element_t, N>;
    using difference_type   = std::ptrdiff_t;

    constexpr iterator& operator++()
    {
      for(std::size_t i = 0; i < N; ++i)
      {
        ++its_[i];
        if(its_[i] != std::ranges::end(parent_->base_))
        {
          return *this; // carry done
        }
        its_[i] = std::ranges::begin(parent_->base_); // reset and carry
      }
      end_ = true; // overflow
      return *this;
    }

    constexpr iterator operator++(int)
    {
      iterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr bool operator==(const iterator& other) const
    {
      return end_ == other.end_ && (end_ || its_ == other.its_);
    }
    constexpr bool operator!=(const iterator& other) const
    {
      return !(*this == other);
    }

    constexpr auto operator*() const
    {
      return deref(std::make_index_sequence<N>{});
    }

    template <std::size_t... I> constexpr auto deref(std::index_sequence<I...>) const
    {
      return reference({*its_[I]...});
    }
  };

public:
  constexpr cartesian_product_view() = default;
  constexpr cartesian_product_view(R base) : base_(std::move(base))
  {
  }

  constexpr auto begin() const
  {
    return iterator{this, false};
  }
  constexpr auto end() const
  {
    return iterator{this, true};
  }
};

template <std::size_t N> struct cartesian_product_fn
{
  template <std::ranges::viewable_range R> constexpr auto operator()(R&& r) const
  {
    return cartesian_product_view<N, std::views::all_t<R>>(std::forward<R>(r));
  }

  template <std::ranges::viewable_range R> friend constexpr auto operator|(R&& r, const cartesian_product_fn& self)
  {
    return self(std::forward<R>(r));
  }
};

} // namespace ranges

namespace views
{
template <std::size_t N> inline constexpr ranges::cartesian_product_fn<N> cartesian_product;
} // namespace views
} // namespace reflex