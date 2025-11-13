#pragma once

#include <reflex/meta.hpp>

#include <QObject>
#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

namespace reflex::qt
{

template <typename Super> struct gadget;
template <typename Super, typename ParentT = QObject> struct object;

namespace detail
{
template <typename Super> struct gadget_impl;
template <typename Super, typename ParentT = QObject> struct object_impl;
template <typename Tag, typename Super> struct meta_strings;

template <typename T> struct defaulted
{
  using type = T;
};

template <typename T> struct drop_defaulted_fn
{
  using type = T;
};

template <typename T> struct drop_defaulted_fn<defaulted<T>>
{
  using type = T;
};

template <typename Type, typename Parent> struct object_impl;

template <typename ObjectT, typename... Args> struct signal_decl
{
private:
  friend struct object<ObjectT, typename ObjectT::parent_type>;
  friend object_impl<ObjectT, typename ObjectT::parent_type>;

  friend struct gadget<ObjectT>;
  friend struct gadget_impl<ObjectT>;

  template <typename Tag, typename Super> friend struct meta_strings;

  // for signature resolution
  inline void fn(typename drop_defaulted_fn<Args>::type...)
  {
  }

  using default_tuple_type = [:substitute(
                                   ^^std::tuple,
                                   std::vector<meta::info>{
                                       ^^Args...}
                                       | std::views::filter([](auto R)
                                                            { return meta::is_template_instance_of(R, ^^defaulted); })
                                       | std::views::transform([](auto R) { return template_arguments_of(R)[0]; })):];

  const default_tuple_type defaults_;
  static constexpr auto    defaulted_args_count_ = std::tuple_size_v<default_tuple_type>;

  ObjectT* p_ = nullptr;

public:
  template <typename... VArgs>
    requires((sizeof...(VArgs) >= defaulted_args_count_)      // Missing default argument(s)
             and (sizeof...(VArgs) <= defaulted_args_count_)) // Too much default argument(s)
  signal_decl(ObjectT* parent, VArgs... defaults) noexcept : p_{parent}, defaults_{defaults...}
  {
  }

  template <typename... VArgs>
    requires((sizeof...(VArgs) <= sizeof...(Args))                          // Too much argument(s)
             and                                                            //
             (sizeof...(VArgs) >= sizeof...(Args) - defaulted_args_count_)) // Missing required argument(s)
  inline void operator()(VArgs&&... args)
  {
    assert(p_ != nullptr); // you should declare your signal like this: signal_decl<...> mySignal{this};

    auto get_arg = [&]<size_t I>
    {
      if constexpr(I >= sizeof...(VArgs))
      {
        constexpr auto default_arg_index = sizeof...(VArgs) - I;
        return std::get<default_arg_index>(defaults_);
      }
      else
      {
        return std::forward<decltype(args...[I])>(args...[I]);
      }
    };
    std::apply([this](auto&&... args) { p_->trigger(this, std::forward<decltype(args)>(args)...); },
               [&]<size_t... I>(std::index_sequence<I...>)
               { return std::make_tuple(get_arg.template operator()<I>()...); }(std::index_sequence_for<Args...>()));
  }
};

struct slot
{
};
struct invocable
{
};

template <constant_string... spec> struct property
{
  static constexpr auto _specs      = std::tuple{spec...};
  static constexpr auto _read_pos   = std::get<0>(_specs).get().find('r');
  static constexpr auto _write_pos  = std::get<0>(_specs).get().find('w');
  static constexpr auto _notify_pos = std::get<0>(_specs).get().find('n');

  static constexpr auto readable = _read_pos != std::string_view::npos;
  static constexpr auto writable = _write_pos != std::string_view::npos;
  static constexpr auto notify   = _notify_pos != std::string_view::npos;
};

template <meta::info of> struct listener_of
{
};

template <meta::info of> struct setter_of
{
};

template <meta::info of> struct getter_of
{
};

struct timer_event
{
};

template <size_t N> struct string_storage_wrapper
{
  char data[N + 1];

  constexpr string_storage_wrapper(char const* s)
  {
    std::copy(s, s + N, data);
    data[N] = '\0'; // FIXME why is it necessary ??
  }
};

#ifdef __REFLEX_QT_ENABLE_TYPE_REGISTRY
struct objects_scope
{
};
using object_reg = meta::registry<objects_scope>;
#endif

} // namespace detail

} // namespace reflex::qt