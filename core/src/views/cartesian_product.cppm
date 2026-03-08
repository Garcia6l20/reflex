export module reflex.core:views.cartesian_product;

import std;

export namespace reflex
{

namespace ranges
{

template<std::size_t N, std::ranges::forward_range R>
class cartesian_product_view
  : public std::ranges::view_interface<cartesian_product_view<N,R>>
{
  R base_;

  using reference_t = std::ranges::range_reference_t<const R>;
  using value_t     = std::remove_cvref_t<reference_t>;

public:

  struct iterator
  {
    using iterator_concept  = std::random_access_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = std::array<value_t, N>;
    using reference         = value_type&;

    const cartesian_product_view* parent_ = nullptr;

    std::size_t index_ = 0;
    std::size_t base_size_ = 0;

    constexpr iterator() = default;

    constexpr iterator(const cartesian_product_view* p, std::size_t i)
        : parent_(p), index_(i)
    {
        base_size_ = std::ranges::size(parent_->base_);
    }

    constexpr value_type operator*() const
    {
        auto b = std::ranges::begin(parent_->base_);

        std::size_t idx = index_;
        value_type result{};

        for(std::size_t i = 0; i < N; ++i)
        {
            auto digit = idx % base_size_;
            idx /= base_size_;

            result[i] = *std::next(b, digit);
        }

        return result;
    }

    constexpr iterator& operator++()
    {
        ++index_;
        return *this;
    }

    constexpr iterator operator++(int)
    {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr iterator& operator--()
    {
        --index_;
        return *this;
    }

    constexpr iterator operator--(int)
    {
        auto tmp = *this;
        --(*this);
        return tmp;
    }

    constexpr iterator& operator+=(difference_type n)
    {
        index_ += n;
        return *this;
    }

    constexpr iterator& operator-=(difference_type n)
    {
        index_ -= n;
        return *this;
    }

    friend constexpr iterator operator+(iterator it, difference_type n)
    {
        it += n;
        return it;
    }

    friend constexpr iterator operator+(difference_type n, iterator it)
    {
        it += n;
        return it;
    }

    friend constexpr iterator operator-(iterator it, difference_type n)
    {
        it -= n;
        return it;
    }

    friend constexpr difference_type operator-(const iterator& a,
                                               const iterator& b)
    {
        return static_cast<difference_type>(a.index_)
             - static_cast<difference_type>(b.index_);
    }

    friend constexpr bool operator==(const iterator& a,
                                     const iterator& b)
    {
        return a.index_ == b.index_;
    }

    friend constexpr bool operator<(const iterator& a,
                                    const iterator& b)
    {
        return a.index_ < b.index_;
    }

    friend constexpr bool operator>(const iterator& a,
                                    const iterator& b)
    {
        return b < a;
    }

    friend constexpr bool operator<=(const iterator& a,
                                     const iterator& b)
    {
        return !(b < a);
    }

    friend constexpr bool operator>=(const iterator& a,
                                     const iterator& b)
    {
        return !(a < b);
    }
  };

public:

  constexpr cartesian_product_view() = default;

  constexpr explicit cartesian_product_view(R base)
      : base_(std::move(base))
  {}

  constexpr auto begin() const
  {
      return iterator{this, 0};
  }

  constexpr auto end() const
  {
      return iterator{this, size()};
  }

  constexpr std::size_t size() const
    requires std::ranges::sized_range<const R>
  {
      auto n = std::ranges::size(base_);

      std::size_t r = 1;
      for(std::size_t i = 0; i < N; ++i)
          r *= n;

      return r;
  }
};

template<std::size_t N>
struct cartesian_product_fn
{
  template<std::ranges::viewable_range R>
  constexpr auto operator()(R&& r) const
  {
      return cartesian_product_view<
          N,
          std::views::all_t<R>
      >(std::views::all(std::forward<R>(r)));
  }

  template<std::ranges::viewable_range R>
  friend constexpr auto operator|(R&& r,
                                  const cartesian_product_fn& self)
  {
      return self(std::forward<R>(r));
  }
};

} // namespace ranges

namespace views
{
template<std::size_t N>
inline constexpr ranges::cartesian_product_fn<N> cartesian_product;
}

} // namespace reflex