#pragma once

#include <reflex/const_check.hpp>
#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/detail/property.hpp>
#include <reflex/qt/detail/version.hpp>

#include <QtCore/qobject.h>

#include <cstddef>
#include <string>
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

/** @brief rejects a `defaulted<T>` signal argument that another argument follows
 *
 * The emitter fills the omitted arguments from the end, so a `defaulted<T>` that
 * is not trailing would take the value of a later argument. C++ forbids the same
 * shape on a function declaration.
 */
consteval bool check_trailing_defaults(std::initializer_list<meta::info> args)
{
  bool seen_default = false;
  for(auto a : args)
  {
    const bool is_default = meta::is_template_instance_of(a, ^^defaulted);
    REFLEX_META_CHECK(is_default or not seen_default,
                      "the signal argument " + std::string{display_string_of(a)}
                          + " follows a defaulted<> one; every defaulted<> argument of a signal "
                            "has to be trailing, the way a C++ default argument has to be",
                      a);
    seen_default = seen_default or is_default;
  }
  return true;
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
  consteval
  {
    check_trailing_defaults({^^Args...});
  }

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

/** @brief a QML type version, the way `QML_ADDED_IN_VERSION` spells one
 *
 * moc writes the pair as the single integer `major * 256 + minor`, measured on
 * a real `QML_ADDED_IN_VERSION(2, 3)`, which lands in the document as `515`.
 * Major version `0` reads as "no version given", since a QML type registered
 * under a module major version of `0` does not exist.
 */
struct qml_version
{
  int major_version = 0;
  int minor_version = 0;

  /** @brief Whether a version was written at all. */
  constexpr bool given() const noexcept
  {
    return major_version > 0;
  }

  /** @brief The single integer moc writes for the version. */
  constexpr int encoded() const noexcept
  {
    return major_version * 256 + minor_version;
  }
};

/** @brief publishes the class to QML, as the `QML_*` macros do for a moc'ed one
 *
 * One aggregate rather than one annotation per macro, so the options compose:
 * a named uncreatable singleton is three fields, not a name nobody invented.
 *
 * ```cpp
 * struct [[= qt::qml{}]] counter : qt::object<counter> { };
 * struct [[= qt::qml{.singleton = true}]] settings : qt::object<settings> { };
 * struct [[= qt::qml{.name = "Gauge", .uncreatable = "ask Factory"}]] meter : qt::object<meter> { };
 * struct [[= qt::qml{.name = "span"}]] span : qt::gadget<span> { };
 * ```
 *
 * Every field is a class info, which is all moc emits for a `QML_*` macro:
 * `QML.Element` carries @ref name, `QML.Singleton` carries @ref singleton, and
 * @ref uncreatable carries `QML.Creatable` = `false` plus
 * `QML.UncreatableReason`. `qt::classinfo` still reaches the same table, so a
 * macro Qt adds later needs no new field here.
 *
 * @ref singleton and @ref uncreatable also drive the `QQmlPrivate` traits
 * `qmlRegisterTypesAndRevisions` reads, which the `QML_SINGLETON` macro
 * supplies as a nested enumeration and a marker member function that a CRTP
 * base cannot inject. Include `reflex/qt/qml.hpp` in the header the QML
 * registration compiles against, or the class registers as an ordinary type.
 */
struct qml
{
  /** @brief `QML.Element`: the QML name, `auto` for the C++ one */
  constant_string name{"auto"};

  /** @brief `QML.Singleton`: one engine-owned instance instead of a type */
  bool singleton = false;

  /** @brief `QML.UncreatableReason`: why QML may not instantiate the type */
  constant_string uncreatable{""};

  /** @brief `QML.AddedInVersion`: the version introducing the type */
  qml_version added_in{};

  /** @brief `QML.RemovedInVersion`: the version retiring the type */
  qml_version removed_in{};
};

namespace detail
{
/** @brief @p Super's `qml` annotation, or the default one when it carries none */
consteval auto qml_spec_of(meta::info Super) -> qml
{
  for(auto a : meta::annotations_of_with(Super, ^^qt::qml))
  {
    return extract<qt::qml>(constant_of(a));
  }
  return qml{};
}

/** @brief rejects a `qml` annotation that cannot be honoured */
consteval bool validate_qml(meta::info Super)
{
  std::size_t count = 0;
  for(auto a : meta::annotations_of_with(Super, ^^qt::qml))
  {
    ++count;
    REFLEX_META_CHECK(not(*extract<qt::qml>(constant_of(a)).name).empty(),
                      std::string{identifier_of(Super)}
                          + " carries an empty qml{} name; drop .name to publish it as "
                            "auto",
                      Super);
  }
  REFLEX_META_CHECK(count <= 1,
                    std::string{identifier_of(Super)}
                        + " carries more than one qml{} annotation; keep one",
                    Super);
  return true;
}

/** @brief the class infos moc emits for the `QML_*` macros @p Super's `qml` stands for
 *
 * Every string here was measured by running moc on a class carrying the
 * corresponding macro, not read off the macro definitions.
 */
consteval auto qml_class_infos_of(meta::info Super) -> std::vector<std::string>
{
  std::vector<std::string> list;
  if(not meta::has_annotation(Super, ^^qt::qml))
  {
    return list;
  }
  validate_qml(Super);

  const auto spec = qml_spec_of(Super);
  auto       push = [&list](std::string key, std::string value)
  {
    list.push_back(std::move(key));
    list.push_back(std::move(value));
  };

  push("QML.Element", std::string{*spec.name});
  if(spec.singleton)
  {
    push("QML.Singleton", "true");
  }
  if(not(*spec.uncreatable).empty())
  {
    push("QML.Creatable", "false");
    push("QML.UncreatableReason", std::string{*spec.uncreatable});
  }
  if(spec.added_in.given())
  {
    push("QML.AddedInVersion", std::to_string(spec.added_in.encoded()));
  }
  if(spec.removed_in.given())
  {
    push("QML.RemovedInVersion", std::to_string(spec.removed_in.encoded()));
  }
  return list;
}
}
}
