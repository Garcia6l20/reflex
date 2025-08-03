#pragma once

#include <reflex/meta.hpp>

#include <format>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace reflex::testing::detail
{
struct base_eval_context
{
  base_eval_context() : parent_{current}
  {
    current = this;
  }
  ~base_eval_context()
  {
    current = parent_;
  }

  using on_match = std::function<void(std::string_view, std::string_view)>;
  virtual void do_search(std::string_view expression, on_match const&) const = 0;

  static void search(std::string_view expression, on_match const& m)
  {
    auto tmp = current;
    while(tmp != nullptr)
    {
      tmp->do_search(expression, m);
      tmp = tmp->parent_;
    }
  }

  const base_eval_context*               parent_ = nullptr;
  inline static const base_eval_context* current = nullptr;
};
template <meta::info... Items> struct eval_context : base_eval_context
{
  using ItemsType = [:substitute(^^std::tuple,
                                 std::array{
                                     Items...}                                //
                                     | std::views::transform(meta::type_of)   //
                                     | std::views::transform(meta::add_const) //
                                     | std::views::transform(meta::add_lvalue_reference)):];
  ItemsType items_;
  eval_context(auto&... items) : base_eval_context{}, items_{items...}
  {
  }
  eval_context(ItemsType const& items) : base_eval_context{}, items_{items}
  {
  }
  void do_search(std::string_view expression, on_match const& m) const final
  {
    template for(constexpr auto ii : std::views::iota(0uz, sizeof...(Items)))
    {
      constexpr auto I = Items...[ii];
      using ItemT      = [:type_of(I):];
      if constexpr(std::formattable<ItemT, char>)
      {
        constexpr auto s = display_string_of(I);
        if(expression.find(s) != std::string_view::npos)
        {
          m(s, std::format("{}", std::get<ii>(items_)));
        }
      }
    }
  }
};

template <meta::info Fn> struct fn_eval_context : base_eval_context
{
  static constexpr auto params_ = define_static_array(parameters_of(Fn));
  using ItemsType               = [:substitute(^^std::tuple,
                                 params_                                      //
                                     | std::views::transform(meta::type_of)   //
                                     | std::views::transform(meta::add_const) //
                                     | std::views::transform(meta::add_lvalue_reference)):];
  ItemsType items_;
  fn_eval_context(auto const&... items) : base_eval_context{}, items_{items...}
  {
  }
  void do_search(std::string_view expression, on_match const& m) const final
  {
    template for(constexpr auto ii : std::views::iota(0uz, std::tuple_size_v<ItemsType>))
    {
      constexpr auto I = params_[ii];
      using ItemT      = [:type_of(I):];
      if constexpr(std::formattable<ItemT, char>)
      {
        constexpr auto s = identifier_of(I);
        if(expression.find(s) != std::string_view::npos)
        {
          m(s, std::format("{}", std::get<ii>(items_)));
        }
      }
    }
  }
};

template <meta::info Cls> struct class_eval_context : base_eval_context
{
  static constexpr auto members_ =
      define_static_array(nonstatic_data_members_of(Cls, meta::access_context::unchecked()));
  [:Cls:] const& obj_;
  class_eval_context(auto const& object) : base_eval_context{}, obj_{object}
  {
  }
  void do_search(std::string_view expression, on_match const& m) const final
  {
    template for(constexpr auto ii : std::views::iota(0uz, members_.size()))
    {
      constexpr auto I = members_[ii];
      using ItemT      = [:type_of(I):];
      if constexpr(std::formattable<ItemT, char>)
      {
        constexpr auto s = identifier_of(I);
        if(expression.find(s) != std::string_view::npos)
        {
          m(s, std::format("{}", obj_.[:I:]));
        }
      }
    }
  }
};

static inline void* current_instance = nullptr;
} // namespace reflex::testing::detail
