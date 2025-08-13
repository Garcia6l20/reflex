#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <ranges>
#include <vector>

namespace reflex
{
namespace ranges
{
template <std::size_t N, std::ranges::forward_range R>
class permutations_view : public std::ranges::view_interface<permutations_view<N, R>>
{
  using T = std::ranges::range_value_t<R>;

  R                        base_;
  std::vector<T>           elements_;
  std::vector<std::size_t> indices_;
  bool                     started_ = false;
  bool                     done_    = false;

  constexpr void init_elements()
  {
    elements_.clear();
    for(auto&& x : base_)
      elements_.push_back(x);
    indices_.resize(elements_.size());
    std::iota(indices_.begin(), indices_.end(), 0);
    std::sort(indices_.begin(), indices_.end());

    while(!done_ && !is_unique_first_N())
      done_ = !std::next_permutation(indices_.begin(), indices_.end());
  }

  constexpr bool is_unique_first_N() const
  {
    for(std::size_t i = 0; i < N; ++i)
      for(std::size_t j = i + 1; j < N; ++j)
        if(indices_[i] == indices_[j])
          return false;
    return true;
  }

  struct iterator
  {
    using value_type        = std::array<T, N>;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;
    using reference         = const value_type&;

    permutations_view* parent_;
    mutable value_type current_;

    constexpr iterator() = default;
    constexpr explicit iterator(permutations_view* p) : parent_(p)
    {
    }

    constexpr reference operator*() const
    {
      for(std::size_t i = 0; i < N; ++i)
        current_[i] = parent_->elements_[parent_->indices_[i]];
      return current_;
    }

    constexpr iterator& operator++()
    {
      do
      {
        parent_->done_ = !std::next_permutation(parent_->indices_.begin(), parent_->indices_.end());
      } while(!parent_->done_ && !parent_->is_unique_first_N());
      return *this;
    }

    constexpr void operator++(int)
    {
      ++(*this);
    }

    constexpr bool operator==(std::default_sentinel_t) const
    {
      return parent_->done_;
    }
  };

public:
  constexpr permutations_view() = default;

  constexpr explicit permutations_view(R r) : base_(std::move(r))
  {
    init_elements();
  }

  constexpr auto begin()
  {
    if(!started_)
      started_ = true;
    return iterator{this};
  }

  // ❗️ These const overloads are required for std::ranges::range
  constexpr auto begin() const
  {
    return const_cast<permutations_view*>(this)->begin();
  }

  constexpr auto end() const
  {
    return std::default_sentinel;
  }
};

template <std::size_t N> struct permutations_fn
{
  template <std::ranges::viewable_range R> constexpr auto operator()(R&& r) const
  {
    return permutations_view<N, R>(std::forward<R>(r));
  }

  template <std::ranges::viewable_range R> friend constexpr auto operator|(R&& r, const permutations_fn& self)
  {
    return self(std::forward<R>(r));
  }
};
} // namespace ranges

namespace views
{
template <std::size_t N> inline constexpr ranges::permutations_fn<N> permutations;
}
} // namespace reflex