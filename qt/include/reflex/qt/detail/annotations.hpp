#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/detail/property.hpp>
#include <reflex/qt/detail/version.hpp>

#include <QtCore/qobject.h>

#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace reflex::qt
{
template <typename Super> class gadget;
template <typename Super, typename ParentT = QObject> class object;

struct invocable_t
{
};

/** @brief marks a member function as `Q_INVOKABLE`
 *
 * ```cpp
 * [[= reflex::qt::invocable]] int twice(int n) const { return 2 * n; }
 * ```
 */
inline constexpr invocable_t invocable{};

struct slot_t
{
};

/** @brief marks a member function as a slot
 *
 * ```cpp
 * [[= reflex::qt::slot]] void bump() { setProperty<^^value>(value + 1); }
 * ```
 */
inline constexpr slot_t slot{};

/** @brief wraps a `signal` argument type that carries a default value
 *
 * Qt publishes one method table entry per default-argument arity, so the
 * metaobject needs the defaulted-argument count while it reflects over the
 * class, before any object exists. The defaults themselves are constructor
 * arguments and never appear in the member's type, so the count cannot be
 * inferred and the marker carries it.
 *
 * ```cpp
 * signal<int, reflex::qt::defaulted<int>> pair{this, 42};
 * ```
 */
template <typename T> struct defaulted
{
  using type = T;
};

namespace detail
{
template <typename Super> struct gadget_impl;
template <typename Super, typename ParentT> struct object_impl;
template <typename Tag, typename Super> struct meta_strings;

template <typename T> struct drop_default
{
  using type = T;
};

template <typename T> struct drop_default<defaulted<T>>
{
  using type = T;
};

template <typename T> using drop_default_t = typename drop_default<T>::type;

consteval auto default_types_of(std::initializer_list<meta::info> args) -> std::vector<meta::info>
{
  std::vector<meta::info> types;
  for(auto a : args)
  {
    if(meta::is_template_instance_of(a, ^^defaulted))
    {
      types.push_back(template_arguments_of(a)[0]);
    }
  }
  return types;
}

/** @brief a Qt signal, declared as a data member of @p ObjectT
 *
 * ```cpp
 * struct counter : reflex::qt::object<counter>
 * {
 *   signal<int, reflex::qt::defaulted<int>> changed{this, 0};
 * };
 * ```
 *
 * Calling the member emits: the missing trailing arguments are taken from the
 * defaults given at construction, and the whole list is handed to
 * `QMetaObject::activate` at the signal's own method index.
 */
template <typename ObjectT, typename... Args> class signal_decl
{
public:
  using arguments_type = std::tuple<drop_default_t<Args>...>;
  using defaults_type  = [:substitute(^^std::tuple, default_types_of({^^Args...})):];

  static constexpr std::size_t argument_count = sizeof...(Args);
  static constexpr std::size_t default_count  = std::tuple_size_v<defaults_type>;
  static constexpr std::size_t required_count = argument_count - default_count;

  template <typename... Defaults>
    requires(sizeof...(Defaults) == default_count)
  constexpr signal_decl(ObjectT* owner, Defaults... defaults) noexcept
      : owner_{owner}, defaults_{defaults...}
  {
  }

  template <typename... CallArgs>
    requires(sizeof...(CallArgs) <= argument_count and sizeof...(CallArgs) >= required_count)
  void operator()(CallArgs&&... args)
  {
    emit_with(std::make_index_sequence<argument_count>(), std::forward<CallArgs>(args)...);
  }

private:
  template <typename Tag, typename Super> friend struct meta_strings;

  void fn(drop_default_t<Args>...)
  {
  }

  template <std::size_t I, typename... CallArgs>
  constexpr decltype(auto) argument_at(CallArgs&&... args) const
  {
    if constexpr(I < sizeof...(CallArgs))
    {
      return (args...[I]);
    }
    else
    {
      return std::get<I - required_count>(defaults_);
    }
  }

  template <std::size_t... I, typename... CallArgs>
  void emit_with(std::index_sequence<I...>, CallArgs&&... args)
  {
    arguments_type full = arguments_type(argument_at<I>(std::forward<CallArgs>(args)...)...);
    std::apply([this](auto const&... a) { owner_->trigger(this, a...); }, full);
  }

  ObjectT*      owner_ = nullptr;
  defaults_type defaults_;
};

/** @brief the function whose parameters spell @p R's published signature
 *
 * A slot or an invocable is already a function. A signal is a data member, and
 * its argument list lives in `signal_decl::fn`.
 */
consteval auto call_function_of(meta::info R) -> meta::info
{
  if(is_function(R))
  {
    return R;
  }
  return meta::member_named(meta::dealias(meta::remove_const(type_of(R))),
                            "fn",
                            meta::access_context::unchecked());
}

consteval auto call_return_type_of(meta::info R) -> meta::info
{
  return is_function(R) ? return_type_of(R) : ^^void;
}

consteval auto signal_default_count_of(meta::info R) -> std::size_t
{
  std::size_t count = 0;
  for(auto a : template_arguments_of(meta::dealias(meta::remove_const(type_of(R)))))
  {
    if(meta::is_template_instance_of(a, ^^defaulted))
    {
      ++count;
    }
  }
  return count;
}
}

/** @brief a `Q_CLASSINFO` entry, annotating the class itself */
struct classinfo
{
  constant_string key;
  constant_string value;
};
}
