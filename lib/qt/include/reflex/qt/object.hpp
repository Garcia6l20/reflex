#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/to_tuple.hpp>
#include <reflex/utility.hpp>

#include <reflex/qt/detail/annotations.hpp>
#include <reflex/qt/detail/meta_strings.hpp>
#include <reflex/qt/detail/metatype.hpp>
#include <reflex/qt/gadget.hpp>

#include <QObject>
#include <QTimerEvent>
#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

#include <any>
#include <print>

namespace reflex::qt
{
template <typename Super, typename ParentT> struct object : ParentT, gadget<Super>
{
  friend detail::object_impl<Super, ParentT>;

public:
  using parent_type = ParentT;

  template <typename ObjectT, typename... Args> friend struct detail::signal_decl;

  template <typename... Args> using signal = detail::signal_decl<Super, Args...>;
  template <typename T> using defaulted    = detail::defaulted<T>;

  static constexpr detail::slot        slot;
  static constexpr detail::invocable   invocable;
  static constexpr detail::timer_event timer_event;

  using prop = detail::property;

  template <meta::info of> static constexpr detail::listener_of<of> listener_of;
  template <meta::info of> static constexpr detail::getter_of<of>   getter_of;
  template <meta::info of> static constexpr detail::setter_of<of>   setter_of;

private:
  template <typename Tag>
  static constexpr auto __timer_events = [] consteval
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::timer_event,
                                              meta::access_context::unchecked()));
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
    static constexpr auto ms = detail::meta_strings<tag, Super>{};
    using std::views::iota;
    size_t local_signal_index = 0;
    template for(constexpr auto ii : std::views::iota(size_t(0), ms.signals.size()))
    {
      constexpr auto s     = ms.signals[ii];
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
    static constexpr auto ms = detail::meta_strings<tag, Super>{};

    using std::views::iota;
    constexpr auto expected_signal_type = dealias(^^signal<Args...>);

    auto& self = *static_cast<Super*>(this);

    size_t local_signal_index = 0;
    template for(constexpr auto ii : iota(size_t(0), ms.signals.size()))
    {
      constexpr auto s     = ms.signals[ii];
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

  using gadget<Super>::qt_static_metacall;

public:
  // static constinit const QMetaObject staticMetaObject;
  using gadget<Super>::staticMetaObject;

  const QMetaObject* metaObject() const override
  {
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
  }

  void* qt_metacast(const char* _clname) override;
  int   qt_metacall(QMetaObject::Call _c, int _id, void** _a) override;

  using QObject::property;
  using gadget<Super>::property;

  using QObject::setProperty;
  using gadget<Super>::setProperty;

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
