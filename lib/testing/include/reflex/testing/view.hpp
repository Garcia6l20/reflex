#pragma once

#include <ranges>

namespace reflex::testing
{
namespace detail
{
template <typename Expr, meta::access_context Ctx> struct validator;
}

template <typename ValidatorT> class validation_view : public std::ranges::view_interface<validation_view<ValidatorT>>
{
  ValidatorT base_;
  using value_type = ValidatorT::expression_type::value_type;
  // using parent_iterator = ValidatorT::expression_type::iterator;
  using parent_iterator = ValidatorT::expression_type::const_iterator;

  struct iterator
  {
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;
    // using reference         = const value_type&;
    using value_type = parent_iterator::value_type;
    using reference  = parent_iterator::reference;

    validation_view*        parent_;
    mutable parent_iterator current_;

    constexpr iterator() = default;
    constexpr explicit iterator(validation_view* p) : parent_(p), current_{std::begin(parent_->base_.expression())}
    {
    }

    constexpr reference operator*() const
    {
      return *current_;
    }

    constexpr iterator& operator++()
    {
      current_++;
      return *this;
    }

    constexpr void operator++(int)
    {
      ++current_;
    }

    constexpr bool operator==(std::default_sentinel_t) const
    {
      return current_ == std::end(parent_->base_.expression());
    }
  };

public:
  constexpr validation_view() = default;

  constexpr explicit validation_view(ValidatorT r) : base_(std::move(r))
  {
    // init_elements();
  }

  constexpr auto begin()
  {
    // if(!started_)
    //   started_ = true;
    return iterator{this};
  }

  constexpr auto begin() const
  {
    return const_cast<validation_view*>(this)->begin();
  }

  constexpr auto end() const
  {
    return std::default_sentinel;
  }
};

// template <std::size_t N> struct validation_fn
// {
//   template <std::ranges::viewable_range R> constexpr auto operator()(R&& r) const
//   {
//     return validation_view<N, R>(std::forward<R>(r));
//   }

//   template <std::ranges::viewable_range R> friend constexpr auto operator|(R&& r, const validation_fn& self)
//   {
//     return self(std::forward<R>(r));
//   }
// };

// template <std::size_t N> inline constexpr validation_fn<N> validation;

} // namespace reflex::testing
