#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/to_tuple.hpp>
#include <reflex/utility.hpp>

#ifdef __REFLEX_QT_ENABLE_TYPE_REGISTRY
#include <reflex/regitry.hpp>
#endif

#include <reflex/qt/detail/metatype.hpp>

#include <QObject>
#include <QTimerEvent>
#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

#include <any>
#include <bit>
#include <memory>
#include <print>

namespace reflex::qt
{

template <typename Super, typename ParentT = QObject> struct object;

namespace detail
{

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

template <typename Type, typename Parent> struct meta_impl;

template <typename ObjectT, typename... Args> struct signal_decl
{
private:
  // template <typename Super, typename ParentT> struct object;
  friend struct object<ObjectT, typename ObjectT::parent_type>;
  friend meta_impl<ObjectT, typename ObjectT::parent_type>;

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

template <typename Super, typename ParentT> struct object : ParentT
{
  friend detail::meta_impl<Super, ParentT>;

#ifdef __REFLEX_QT_ENABLE_TYPE_REGISTRY
  consteval
  {
    detail::object_reg::add(^^Super);
  };
#endif

public:
  using parent_type = ParentT;

  template <typename ObjectT, typename... Args> friend struct detail::signal_decl;

  template <typename... Args> using signal = detail::signal_decl<Super, Args...>;
  template <typename T> using defaulted    = detail::defaulted<T>;

  static constexpr detail::slot        slot;
  static constexpr detail::invocable   invocable;
  static constexpr detail::timer_event timer_event;

  template <constant_string... spec> static constexpr detail::property<spec...> prop;
  template <meta::info of> static constexpr detail::listener_of<of>             listener_of;
  template <meta::info of> static constexpr detail::getter_of<of>               getter_of;
  template <meta::info of> static constexpr detail::setter_of<of>               setter_of;

private:
  template <typename Tag>
  static constexpr auto __signals = [] consteval
  {
    return define_static_array(                                               //
        nonstatic_data_members_of(^^Super, meta::access_context::unchecked()) //
        | std::views::filter([&](auto M) { return meta::is_template_instance_of(type_of(M), ^^detail::signal_decl); }));
  }();

  template <typename Tag>
  static constexpr auto __signal_count = [] consteval
  {
    size_t count = 0;
    template for(constexpr auto s : __signals<Tag>)
    {
      using CurrentSignalT = [:type_of(s):];
      count += CurrentSignalT::defaulted_args_count_ + 1;
    }
    return count;
  }();

  template <typename Tag>
  static constexpr auto __slots = [] consteval
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::slot,
                                              meta::access_context::unchecked()));
  }();

  template <typename Tag>
  static constexpr auto __slot_count = [] consteval
  {
    size_t count = 0;
    template for(constexpr auto s : __slots<Tag>)
    {
      constexpr size_t n_overloads = 1 + std::ranges::count_if(parameters_of(s), meta::has_default_argument);
      count += n_overloads;
    }
    return count;
  }();

  template <typename Tag>
  static constexpr auto __invocables = [] consteval
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::invocable,
                                              meta::access_context::unchecked()));
  }();

  template <typename Tag>
  static constexpr auto __invocable_count = [] consteval
  {
    size_t count = 0;
    template for(constexpr auto s : __invocables<Tag>)
    {
      constexpr size_t n_overloads = 1 + std::ranges::count_if(parameters_of(s), meta::has_default_argument);
      count += n_overloads;
    }
    return count;
  }();

  template <typename Tag>
  static constexpr auto __properties = [] consteval
  {
    return define_static_array(                              //
        meta::nonstatic_data_members_annotated_with(^^Super, //
                                                    ^^detail::property,
                                                    meta::access_context::unchecked()));
  }();

  template <typename Tag> static constexpr auto __property_count = __properties<Tag>.size();

  template <typename Tag>
  static constexpr auto __timer_events = [] consteval
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::timer_event,
                                              meta::access_context::unchecked()));
  }();

  template <typename Tag>
  static constexpr auto __custom_types = [] consteval
  {
    std::vector<meta::info> types;

    auto try_push = [&types](auto R)
    {
      auto T = decay(R);
      if(meta::is_template_instance_of(T, ^^detail::defaulted))
      {
        T = decay(template_arguments_of(T)[0]);
      }
      auto tid = detail::static_meta_type_id_of(T);
      if(tid == detail::custom_type and not std::ranges::contains(types, T))
      {
        types.push_back(T);
      }
    };

    for(auto R : __signals<Tag>)
    {
      for(auto A : template_arguments_of(type_of(R)) | std::views::drop(1)) // first arg is ^^Super
      {
        try_push(A);
      }
    }

    for(auto R : __properties<Tag>)
    {
      try_push(type_of(R));
    }

    for(auto R : __slots<Tag>)
    {
      for(auto P : parameters_of(R))
      {
        try_push(type_of(P));
      }
    }

    for(auto R : __invocables<Tag>)
    {
      for(auto P : parameters_of(R))
      {
        try_push(type_of(P));
      }
      try_push(return_type_of(R));
    }

    return define_static_array(types);
  }();

  template <std::meta::info R>
  static constexpr void __serializeClassInfos(std::vector<constant_string>& strings, std::string_view parent = "")
  {
    template for(constexpr auto field :
                 std::define_static_array(std::meta::members_of(R, meta::access_context::unchecked())
                                          | std::views::filter(std::meta::has_identifier)))
    {
      std::string key;
      if(not parent.empty())
      {
        key = parent;
        key += ".";
        key += identifier_of(field);
      }
      else
      {
        key = identifier_of(field);
      }
      if constexpr(field == meta::null)
      {
        continue;
      }
      else if constexpr(meta::is_type(field))
      {
        __serializeClassInfos<field>(strings, key);
      }
      else
      {
        strings.push_back(key);
        strings.push_back([:field:]);
      }
    }
  };

  template <typename Tag>
  static constexpr auto __classInfosStrings = [] consteval
  {
    std::vector<constant_string> strings;

    constexpr auto ci = meta::member_named(^^Super, "ClassInfos", meta::access_context::unchecked());
    if constexpr(ci != meta::null)
    {
      __serializeClassInfos<ci>(strings);
    }

    return define_static_array(strings);
  }();

  template <typename Tag>
  static constexpr auto __strings = [] consteval
  {
    std::vector<constant_string> strings;

    strings.push_back(identifier_of(^^Super));

    template for(constexpr auto s : __classInfosStrings<Tag>)
    {
      strings.push_back(s);
    }

    strings.push_back(""); // add an empty entry

    template for(constexpr auto s : __signals<Tag>)
    {
      strings.push_back(identifier_of(s));
    }

    template for(constexpr auto p : __properties<Tag>)
    {
      strings.push_back(std::string{identifier_of(p)} + "Changed");
    }

    template for(constexpr auto s : __slots<Tag>)
    {
      strings.push_back(identifier_of(s));
    }

    template for(constexpr auto i : __invocables<Tag>)
    {
      strings.push_back(identifier_of(i));
    }

    template for(constexpr auto p : __properties<Tag>)
    {
      strings.push_back(identifier_of(p));
    }

    template for(constexpr auto t : __custom_types<Tag>)
    {
      auto id =
          display_string_of(t) | std::views::filter([](auto c) { return c != ' '; }) | std::ranges::to<std::string>();
      strings.push_back(id);
    }
    return define_static_array(strings);
  }();

  template <typename Tag> static consteval auto __make_object_data()
  {
    struct object_data;
    consteval
    {
      constexpr auto timer_events = __timer_events<Tag>;
      define_aggregate(^^object_data,
                       {
                           data_member_spec(^^int[timer_events.size()],
                                            {
                                                .name = "timer_events"}),
                       });
    };
    object_data data{};
    if constexpr(not __timer_events<Tag>.empty())
    {
      std::ranges::fill(data.timer_events, -1);
    }
    return data;
  }

  std::any object_data_ = __make_object_data<tag>();

  template <typename Self> decltype(auto) __get_object_data(this Self& self)
  {
    using object_data_type = decltype(__make_object_data<tag>());
    return std::any_cast<const_like_t<Self, object_data_type&>>(self.object_data_);
  }

public:
  using ParentT::ParentT;
  virtual ~object() = default;

  template <meta::info Signal, typename... Args> auto trigger(Args... args)
  {
    using std::views::iota;
    size_t local_signal_index = 0;
    template for(constexpr auto ii : std::views::iota(size_t(0), __signals<tag>.size()))
    {
      constexpr auto s     = __signals<tag>[ii];
      using CurrentSignalT = [:type_of(s):];

      if constexpr(s == Signal)
      {
        static_cast<Super*>(this)->[:s:](std::forward<Args>(args)...);
        return;
      }
      local_signal_index += CurrentSignalT::defaulted_args_count_ + 1;
    }
  }

  template <typename... Args, typename... VArgs> auto trigger(signal<Args...>* sig, VArgs&&... args)
  {
    using std::views::iota;
    constexpr auto expected_signal_type = dealias(^^signal<Args...>);

    auto& self = *static_cast<Super*>(this);

    size_t local_signal_index = 0;
    template for(constexpr auto ii : iota(size_t(0), __signals<tag>.size()))
    {
      constexpr auto s     = __signals<tag>[ii];
      using CurrentSignalT = [:type_of(s):];
      if constexpr(dealias(decay(type_of(s))) == expected_signal_type)
      {
        if(&self.[:s:] == sig)
        {
          for(auto jj : iota(size_t(0), signal<Args...>::defaulted_args_count_ + 1))
          {
            QMetaObject::activate<void>(this,
                                        &staticMetaObject,
                                        local_signal_index + jj,
                                        nullptr,
                                        std::forward<VArgs>(args)...);
          }
          return;
        }
      }
      local_signal_index += CurrentSignalT::defaulted_args_count_ + 1;
    }
  }

  QT_TR_FUNCTIONS
public:
  struct tag
  {
  };

  static consteval auto parameters_types_of(meta::info R)
  {
    if(is_function(R))
    {
      return parameters_of(R) | std::views::transform(meta::type_of);
    }
    else
    {
      // assume signal
      auto type    = type_of(R);
      auto call_fn = meta::member_named(type, "fn", meta::access_context::unchecked());
      return parameters_of(call_fn) //
           | std::views::transform(meta::type_of);
    }
  }
  template <meta::info R> static consteval auto parameters_types_of()
  {
    return parameters_types_of(R);
  }

  static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a);

public:
  static constinit const QMetaObject staticMetaObject;

  const QMetaObject* metaObject() const override
  {
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
  }

  void* qt_metacast(const char* _clname) override;
  int   qt_metacall(QMetaObject::Call _c, int _id, void** _a) override;

  using QObject::property;

  template <meta::info Property> auto property(this auto& self)
  {
    using std::ranges::contains;
    static constexpr auto props = __properties<tag>;
    if constexpr(contains(props, Property))
    {
      static constexpr auto getter = meta::first_member_function_annotated_with(^^Super, //
                                                                                ^^detail::getter_of<Property>,
                                                                                meta::access_context::unchecked());
      if constexpr(getter != meta::null)
      {
        return self.[:getter:]();
      }
      else
      {
        return self.[:Property:];
      }
    }
    else
    {
      static_assert(always_false<Super>, "no such property");
    }
  }

  template <constant_string name> auto property(this auto& self)
  {
    using std::ranges::contains;
    static constexpr auto prop = meta::member_named(^^Super, *name, meta::access_context::unchecked());
    static_assert(prop != meta::null, "no such property");
    return self.template property<prop>();
  }

  using QObject::setProperty;

  template <meta::info Property, typename T> void setProperty(this auto& self, T&& value)
  {
    using std::ranges::find;
    static constexpr auto props = __properties<tag>;
    static constexpr auto it    = find(props, Property);
    if constexpr(it != end(props))
    {
      static constexpr auto setter = meta::first_member_function_annotated_with(^^Super, //
                                                                                ^^detail::setter_of<Property>,
                                                                                meta::access_context::unchecked());
      if constexpr(setter != meta::null)
      {
        self.[:setter:](value);
      }
      else
      {
        auto& val = self.[:Property:];
        if(value == val)
        {
          return; // no write need
        }
        val = std::forward<T>(value);
      }

      static constexpr auto listener = meta::first_member_function_annotated_with(^^Super, //
                                                                                  ^^detail::listener_of<Property>,
                                                                                  meta::access_context::unchecked());
      if constexpr(listener != meta::null)
      {
        self.[:listener:]();
      }

      static constexpr auto relative_offset         = std::distance(begin(props), it);
      static constexpr auto notifier_signals_offset = __signal_count<tag>;
      QMetaObject::activate<void>(&self, &staticMetaObject, notifier_signals_offset + relative_offset, nullptr);
    }
    else
    {
      static_assert(always_false<Super>, "no such property");
    }
  }

  template <constant_string name, typename T> void setProperty(this auto& self, T&& value)
  {
    using std::ranges::contains;
    static constexpr auto prop = meta::member_named(^^Super, *name, meta::access_context::unchecked());
    static_assert(prop != meta::null, "no such property");
    self.template setProperty<prop>(std::forward<T>(value));
  }

  template <constant prop> void propertyChanged()
  {
    std::unreachable(); // NOTE: actually unused, just to allow QObject::connect to match the auto-generated signal
  }

public:
  using QObject::startTimer;

  template <meta::info Event> void startTimer(int periodMs)
  {
    using std::ranges::find;
    static constexpr auto events = __timer_events<tag>;
    static constexpr auto it     = find(events, Event);
    if constexpr(it != end(events))
    {
      auto&                 data            = __get_object_data();
      static constexpr auto relative_offset = std::distance(begin(events), it);
      if(data.timer_events[relative_offset] == -1)
      {
        data.timer_events[relative_offset] = startTimer(periodMs);
      }
      else
      {
        std::println("timer event \"{}\" already started", identifier_of(Event));
      }
    }
    else
    {
      static_assert(always_false<Super>, "no such timer event");
    }
  }

  using QObject::killTimer;

  template <meta::info Event> void killTimer()
  {
    using std::ranges::find;
    static constexpr auto events = __timer_events<tag>;
    static constexpr auto it     = find(events, Event);
    if constexpr(it != end(events))
    {
      auto&                 data            = __get_object_data();
      static constexpr auto relative_offset = std::distance(begin(events), it);
      if(data.timer_events[relative_offset] != -1)
      {
        killTimer(data.timer_events[relative_offset]);
        data.timer_events[relative_offset] = -1;
      }
      else
      {
        std::println("timer event \"{}\" not started", identifier_of(Event));
      }
    }
    else
    {
      static_assert(always_false<Super>, "no such timer event");
    }
  }

protected:
  void timerEvent(QTimerEvent* e) override
  {
    static constexpr auto events = __timer_events<tag>;
    if constexpr(not events.empty())
    {
      auto& data = __get_object_data();
      template for(constexpr auto ii : std::views::iota(size_t(0), events.size()))
      {
        constexpr auto event = events[ii];
        if(data.timer_events[ii] == e->timerId())
        {
          static_cast<Super*>(this)->[:event:]();
          return;
        }
      }
    }
    ParentT::timerEvent(e);
  }
};

#define REFLEX_QT_OBJECT_IMPL(__type__) template class reflex::qt::object<__type__, typename __type__::parent_type>;

} // namespace reflex::qt

namespace QtPrivate
{
consteval bool is_reflex_object(std::meta::info R)
{
  const auto first_base_type = type_of(bases_of(R, std::meta::access_context::current())[0]);
  if(reflex::meta::is_template_instance_of(first_base_type, ^^reflex::qt::object))
  {
    return true;
  }
  return false;
}
template <typename Super>
  requires(is_reflex_object(^^Super))
struct HasQ_OBJECT_Macro<Super>
{
  enum
  {
    Value = 1
  };
};

template <typename ObjectT, typename... Args>
struct FunctionPointer<reflex::qt::detail::signal_decl<ObjectT, Args...> ObjectT::*>
{
  using Object     = ObjectT;
  using Arguments  = List<typename reflex::qt::detail::drop_defaulted_fn<Args>::type...>;
  using ReturnType = void;
  typedef ReturnType (reflex::qt::detail::signal_decl<ObjectT, Args...>::*Function)(
      typename reflex::qt::detail::drop_defaulted_fn<Args>::type...);
  enum
  {
    ArgumentCount             = sizeof...(Args),
    IsPointerToMemberFunction = true
  };
  template <typename SignalArgs, typename R> static void call(Function f, Object* o, void** arg)
  {
    FunctorCall<std::index_sequence_for<Args...>, SignalArgs, R, Function>::call(f, o, arg);
  }
};

} // namespace QtPrivate
