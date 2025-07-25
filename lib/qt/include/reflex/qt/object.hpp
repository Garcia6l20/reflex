#pragma once

#include <reflex/meta.hpp>
#include <reflex/to_tuple.hpp>
#include <reflex/utility.hpp>

#include <reflex/qt/detail/metatype.hpp>

#include <QObject>
#include <QTimerEvent>
#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

#include <any>
#include <memory>
#include <print>

namespace reflex::qt
{

template <typename Super, typename ParentT = QObject> struct object;

namespace detail
{

template <typename ObjectT, typename... Args> struct signal_decl
{
  ObjectT* p_ = nullptr;

  template <typename... VArgs> inline void operator()(VArgs&&... args)
  {
    assert(p_ != nullptr); // you should declare your signal like this: signal_decl<...> mySignal{this};
    p_->trigger(this, std::forward<VArgs>(args)...);
  }

private:
  // template <typename Super, typename ParentT> struct object;
  friend struct object<ObjectT, typename ObjectT::parent_type>;

  // for signature resolution
  inline void fn(Args...)
  {
  }
};

struct slot
{
};
struct invocable
{
};

template <fixed_string... spec> struct property
{
  static constexpr auto _specs      = std::tuple{spec...};
  static constexpr auto _read_pos   = std::get<0>(_specs).view().find('r');
  static constexpr auto _write_pos  = std::get<0>(_specs).view().find('w');
  static constexpr auto _notify_pos = std::get<0>(_specs).view().find('n');

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
} // namespace detail

template <typename Super, typename ParentT> struct object : ParentT
{
public:
  using parent_type = ParentT;

  template <typename ObjectT, typename... Args> friend struct detail::signal_decl;

  template <typename... Args> using signal = detail::signal_decl<Super, Args...>;

  static constexpr detail::slot        slot;
  static constexpr detail::invocable   invocable;
  static constexpr detail::timer_event timer_event;

  template <fixed_string... spec> static constexpr detail::property<spec...> prop;
  template <meta::info of> static constexpr detail::listener_of<of>          listener_of;
  template <meta::info of> static constexpr detail::getter_of<of>            getter_of;
  template <meta::info of> static constexpr detail::setter_of<of>            setter_of;

public:
  static consteval auto __signals()
  {
    return define_static_array(                                               //
        nonstatic_data_members_of(^^Super, meta::access_context::unchecked()) //
        | std::views::filter(
              [&](auto M)
              { return has_template_arguments(type_of(M)) and template_of(type_of(M)) == ^^detail::signal_decl; }));
  }

  static consteval auto __slots()
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::slot,
                                              meta::access_context::unchecked()));
  }

  static consteval auto __invocables()
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::invocable,
                                              meta::access_context::unchecked()));
  }

  static consteval auto __properties()
  {
    return define_static_array(                              //
        meta::nonstatic_data_members_annotated_with(^^Super, //
                                                    ^^detail::property,
                                                    meta::access_context::unchecked()));
  }

  static consteval auto __timer_events()
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::timer_event,
                                              meta::access_context::unchecked()));
  }

  static consteval auto __custom_types()
  {
    std::vector<meta::info> types;

    auto try_push = [&types](auto R)
    {
      auto T   = decay(R);
      auto tid = detail::static_meta_type_id_of(T);
      if(tid == detail::custom_type and not std::ranges::contains(types, T))
      {
        types.push_back(T);
      }
    };

    for(auto R : __signals())
    {
      for(auto A : template_arguments_of(type_of(R)) | std::views::drop(1)) // first arg is ^^Super
      {
        try_push(A);
      }
    }

    for(auto R : __properties())
    {
      try_push(type_of(R));
    }

    for(auto R : __slots())
    {
      for(auto P : parameters_of(R))
      {
        try_push(type_of(P));
      }
    }

    for(auto R : __invocables())
    {
      for(auto P : parameters_of(R))
      {
        try_push(type_of(P));
      }
      try_push(return_type_of(R));
    }

    return define_static_array(types);
  }

  template <typename... Strings> struct introspection_data
  {
    std::tuple<Strings...> strings;

    const size_t signals_offset;
    const size_t slots_offset;
    const size_t invocables_offset;
    const size_t properties_offset;
    const size_t custom_types_offset;

    constexpr auto size() const
    {
      return sizeof...(Strings);
    }

    template <meta::info R> consteval size_t custom_type_offset_of() const
    {
      constexpr auto id = []
      {
        if(has_identifier(R))
        {
          return identifier_of(R);
        }
        else if(is_type(R))
        {
          return display_string_of(remove_const(remove_reference(R)));
        }
        std::unreachable();
      }();
      template for(constexpr auto ii : std::views::iota(size_t(0), sizeof...(Strings)))
      {
        if(ii >= custom_types_offset)
        {
          // using std::ranges::equal;
          // using std::views::filter;
          // auto no_white_space = id | filter([](auto c) { return c != ' '; });
          // if(equal(no_white_space, std::get<ii>(strings).view()))
          if (id == std::get<ii>(strings).view())
          {
            return ii;
          }
        }
      }
      std::unreachable();
    }

    template <size_t index> consteval auto string_view_at() const
    {
      return std::get<index>(strings).view();
    }
  };

  static consteval auto __get_strings()
  {
    struct __strings
    {
      std::vector<std::string> strings;

      size_t signals_offset;
      size_t slots_offset;
      size_t invocables_offset;
      size_t properties_offset;
      size_t custom_types_offset;
    };
    __strings data;

    data.strings.push_back(std::string{identifier_of(^^Super)});
    data.strings.push_back(""); // add an empty entry

    data.signals_offset = data.strings.size();

    template for(constexpr auto s : __signals())
    {
      data.strings.push_back(std::string{identifier_of(s)});
    }

    template for(constexpr auto p : __properties())
    {
      data.strings.push_back(std::string{identifier_of(p)} + "Changed");
    }

    data.slots_offset = data.strings.size();

    template for(constexpr auto s : __slots())
    {
      data.strings.push_back(std::string{identifier_of(s)});
    }

    data.invocables_offset = data.strings.size();

    template for(constexpr auto i : __invocables())
    {
      data.strings.push_back(std::string{identifier_of(i)});
    }

    data.properties_offset = data.strings.size();

    template for(constexpr auto p : __properties())
    {
      data.strings.push_back(std::string{identifier_of(p)});
    }

    data.custom_types_offset = data.strings.size();

    template for(constexpr auto t : __custom_types())
    {
      // std::string id{display_string_of(t)};
      // std::erase(id, ' ');
      // data.strings.push_back(std::move(id));
      data.strings.push_back(std::string{display_string_of(t)});
    }
    return data;
  }
  static consteval auto __make_strings_tuple_type()
  {
    std::vector<meta::info> members;
    for(auto s : __get_strings().strings)
    {
      members.push_back(substitute(^^fixed_string,
                                   {
                                       meta::reflect_constant(s.size())}));
    }
    return substitute(^^std::tuple, members);
  }

  static consteval auto __introspection_data()
  {
    auto           data              = __get_strings();
    constexpr auto string_tuple_type = __make_strings_tuple_type();
    using StringTupleT               = [:string_tuple_type:];
    return [&]<size_t... I>(std::index_sequence<I...>)
    {
      return introspection_data<std::tuple_element_t<I, StringTupleT>...>{
          .strings             = StringTupleT{std::tuple_element_t<I, StringTupleT>{data.strings[I].data()}...},
          .signals_offset      = data.signals_offset,
          .slots_offset        = data.slots_offset,
          .invocables_offset   = data.invocables_offset,
          .properties_offset   = data.properties_offset,
          .custom_types_offset = data.custom_types_offset,
      };
    }(std::make_index_sequence<std::tuple_size_v<StringTupleT>>());
  }

  static consteval auto __make_object_data()
  {
    struct object_data;
    consteval
    {
      constexpr auto timer_events = __timer_events();
      define_aggregate(^^object_data,
                       {
                           data_member_spec(^^int[timer_events.size()],
                                            {
                                                .name = "timer_events"}),
                       });
    };
    object_data data{};
    if constexpr(not __timer_events().empty())
    {
      std::ranges::fill(data.timer_events, -1);
    }
    return data;
  }
  std::any object_data_ = __make_object_data();

  template <typename Self> decltype(auto) __get_object_data(this Self& self)
  {
    using object_data_type = decltype(__make_object_data());
    return std::any_cast<const_like_t<Self, object_data_type&>>(self.object_data_);
  }

public:
  using ParentT::ParentT;
  virtual ~object() = default;

  template <meta::info Signal, typename... Args> auto trigger(Args... args)
  {
    constexpr auto sigs = __signals();

    template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
    {
      constexpr auto s = sigs[ii];
      if constexpr(s == Signal)
      {
        QMetaObject::activate<void>(this, &staticMetaObject, ii, nullptr, std::forward<Args>(args)...);
      }
    }
  }

  template <typename... Args, typename... VArgs> auto trigger(signal<Args...>* sig, VArgs&&... args)
  {
    constexpr auto sigs                 = __signals();
    constexpr auto expected_signal_type = dealias(^^signal<Args...>);

    auto& self = *static_cast<Super*>(this);

    template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
    {
      constexpr auto s = sigs[ii];
      if constexpr(dealias(decay(type_of(s))) == expected_signal_type)
      {
        if(&self.[:s:] == sig)
        {
          QMetaObject::activate<void>(this, &staticMetaObject, ii, nullptr, std::forward<VArgs>(args)...);
        }
      }
    }
  }

  QT_WARNING_PUSH
  Q_OBJECT_NO_OVERRIDE_WARNING
  QT_TR_FUNCTIONS
private:
  template <typename> static constexpr auto qt_create_metaobjectdata()
  {
    namespace QMC              = QtMocConstants;
    static constexpr auto data = __introspection_data();

    auto qt_stringData = [&] constexpr
    {
      return [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return QtMocHelpers::StringRefStorage{std::get<I>(data.strings).data...};
      }(std::make_index_sequence<data.size()>());
    }();

    size_t strings_index = data.signals_offset;

    constexpr auto sigs   = __signals();
    constexpr auto invocs = __invocables();
    constexpr auto slts   = __slots();
    constexpr auto props  = __properties();

    auto qt_methods = [&] constexpr
    {
      auto make_data_impl = [&]<meta::info Fn, auto Type>() constexpr
      {
        constexpr auto method = []
        {
          if constexpr(is_function(Fn))
          {
            return Fn;
          }
          else
          {
            using FnT = [:type_of(Fn):];
            return ^^FnT::fn;
          }
        }();
        constexpr auto method_type = type_of(method);
        using SignalT              = [:method_type:];
        using RetT                 = [:return_type_of(method_type):];
        using SignatureT           = typename[:meta::signature_of<method>():];
        using DataT                = QtMocHelpers::FunctionData<SignatureT, Type>;

        auto make_parameters_data = [&] constexpr
        {
          constexpr auto parameters     = define_static_array(parameters_of(method));
          auto           make_parameter = [&]<meta::info p>
          {
            constexpr auto param_type = type_of(p);
            constexpr auto tid        = []
            {
              constexpr auto static_tid = uint(detail::static_meta_type_id_of(param_type));
              if constexpr(static_tid == uint(detail::custom_type))
              {
                return static_tid | uint(data.template custom_type_offset_of<param_type>());
                // return static_tid;
              }
              else
              {
                return static_tid;
              }
            }();
            return typename DataT::FunctionParameter{
                .typeIdx = tid,
                .nameIdx = 1, // TODO
            };
          };
          return [&]<std::size_t... I>(std::index_sequence<I...>)
          {
            return typename DataT::ParametersArray{make_parameter.template operator()<parameters[I]>()...};
          }(std::make_index_sequence<parameters.size()>());
        };

        return DataT( //
            /* nameIndex = */ strings_index++,
            /* tagIndex = */ 1,
            is_public(Fn)      ? QMC::AccessPublic
            : is_protected(Fn) ? QMC::AccessProtected
                               : QMC::AccessPrivate,
            detail::static_meta_type_id_of(^^RetT),
            make_parameters_data());
      };

      const auto signals_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::make_tuple(make_data_impl.template operator()<sigs[I], QtMocConstants::MethodSignal>()...);
      }(std::make_index_sequence<sigs.size()>());

      const auto properties_notification_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::make_tuple(
            make_data_impl.template operator()<^^object::propertyChanged<props[I]>, QtMocConstants::MethodSignal>()...);
      }(std::make_index_sequence<props.size()>());

      const auto slots_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::make_tuple(make_data_impl.template operator()<slts[I], QtMocConstants::MethodSlot>()...);
      }(std::make_index_sequence<slts.size()>());

      const auto invocables_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::make_tuple(make_data_impl.template operator()<invocs[I], QtMocConstants::MethodMethod>()...);
      }(std::make_index_sequence<invocs.size()>());

      return std::apply([]<typename... Args>(Args... args)
                        { return QtMocHelpers::UintData<Args...>{std::move(args)...}; },
                        std::tuple_cat(signals_data, properties_notification_data, slots_data, invocables_data));
    }();

    size_t property_index = 0;
    size_t notify_index   = sigs.size();
    auto   qt_properties  = [&]
    {
      auto make_data_impl = [&]<meta::info p>() constexpr
      {
        constexpr auto prop_type = type_of(p);
        using T                  = [:prop_type:];
        using DataT              = QtMocHelpers::PropertyData<T>;

        constexpr auto spec_annotation = annotations_of(p)[0];
        using SpecT                    = [:type_of(spec_annotation):];
        uint flags                     = QMC::DefaultPropertyFlags;
        if constexpr(SpecT::writable)
        {
          flags |= QMC::Writable;
        }

        return DataT( //
            /* nameIndex = */ strings_index++,
            /* typeIndex = */ detail::static_meta_type_id_of(prop_type),
            /* flags =     */ flags,
            /* notifyId =  */ notify_index++,
            /* revision =  */ 0);
      };

      return [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return QtMocHelpers::UintData{make_data_impl.template operator()<props[I]>()...};
      }(std::make_index_sequence<props.size()>());
    }();
    QtMocHelpers::UintData qt_enums{};
    return QtMocHelpers::metaObjectData<Super, tag>(QMC::MetaObjectFlag{},
                                                    qt_stringData,
                                                    qt_methods,
                                                    qt_properties,
                                                    qt_enums);
  }
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectContent = qt_create_metaobjectdata<MetaObjectTagType>();
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectStaticContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.staticData;
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectRelocatingContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.relocatingData;

  Q_OBJECT_NO_ATTRIBUTES_WARNING

  QT_DEFINE_TAG_STRUCT(QPrivateSignal);
  QT_ANNOTATE_CLASS(qt_qobject, "")

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

  static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
  {
    auto* _t = static_cast<Super*>(_o);

    constexpr auto sigs   = __signals();
    constexpr auto invocs = __invocables();
    constexpr auto slts   = __slots();
    constexpr auto props  = __properties();

    constexpr auto sig_count  = sigs.size();
    constexpr auto slts_count = slts.size();
    constexpr auto prop_count = props.size();

    const auto do_invoke = []<meta::info R>(Super* self, void** args)
    {
      static constexpr auto parameters = define_static_array(parameters_of(R));
      const auto            get_arg    = [&]<size_t I>
      {
        constexpr auto p = remove_reference(type_of(parameters[I]));
        using T          = typename[:p:];
        return *reinterpret_cast<T*>(args[I + 1]);
      };

      constexpr auto return_type = return_type_of(R);
      if constexpr(return_type == ^^void)
      {
        [&]<size_t ...I>(std::index_sequence<I...>) {//
            self->[:R:](get_arg.template operator()<I>()...);
            }(std::make_index_sequence<parameters.size()>());
      }
      else
      {
        using Ret = [:return_type:];
        Ret ret = [&]<size_t ...I>(std::index_sequence<I...>) {//
              return self->[:R:](get_arg.template operator()<I>()...);
            }(std::make_index_sequence<parameters.size()>());
        if(args[0])
        {
          *reinterpret_cast<Ret*>(args[0]) = std::move(ret);
        }
      }
    };

    if(_c == QMetaObject::InvokeMetaMethod)
    {
      template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
      {
        if(ii == _id)
        {
          static constexpr auto s = sigs[ii];
          static constexpr auto parameters =
              define_static_array(parameters_types_of<s>() | std::views::transform(meta::remove_reference));
          const auto get_arg = []<size_t I>(void** args)
          {
            constexpr auto p = parameters[I];
            using T          = typename[:p:];
            return *reinterpret_cast<T*>(args[I + 1]);
          };

          [&]<size_t ...I>(std::index_sequence<I...>, Super* self, void**args) {//
            self->template trigger<s>(get_arg.template operator()<I>(args)...);
            }(std::make_index_sequence<parameters.size()>(), _t, _a);
          return;
        }
      }
      _id -= sig_count;

      template for(constexpr auto ii : std::views::iota(size_t(0), prop_count))
      {
        if(ii == _id)
        {
          static constexpr auto p = props[ii];
          static constexpr auto s = ^^object::propertyChanged<p>;
          do_invoke.template    operator()<s>(_t, _a);
          return;
        }
      }
      _id -= prop_count;

      template for(constexpr auto ii : std::views::iota(size_t(0), slts.size()))
      {
        if(ii == _id)
        {
          static constexpr auto s = slts[ii];
          do_invoke.template    operator()<s>(_t, _a);
          return;
        }
      }
      _id -= slts_count;
      template for(constexpr auto ii : std::views::iota(size_t(0), invocs.size()))
      {
        if(ii == _id)
        {
          static constexpr auto s = invocs[ii];
          do_invoke.template    operator()<s>(_t, _a);
          return;
        }
      }
    }
    if(_c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      const auto     arg_num      = *reinterpret_cast<int*>(_a[1]);
      auto*          result       = reinterpret_cast<QMetaType*>(_a[0]);
      constexpr auto custom_types = __custom_types();

      template for(constexpr auto ii : std::views::iota(size_t(0), sig_count))
      {
        if(ii == _id)
        {
          static constexpr auto s = sigs[ii];

          static constexpr auto parameters =
              define_static_array(parameters_types_of<s>() | std::views::transform(meta::remove_reference));

          constexpr auto p_count = parameters.size();
          template for(constexpr auto jj : std::views::iota(size_t(0), p_count))
          {
            if(arg_num == jj)
            {
              using ParamT = [:parameters[jj]:];
              *result      = QMetaType::fromType<ParamT>();
              return;
            }
          }
        }
      }
      _id -= sig_count;

      *result = QMetaType();
    }
    if(_c == QMetaObject::IndexOfMethod)
    {
      template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
      {
        constexpr auto s = sigs[ii];
        if constexpr(is_function(s))
        {
          using signature_type = [:meta::signature_of<s, ^^Super>():];
          if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s:], ii))
          {
            return;
          }
        }
        else
        {
          using SignalT                   = [:type_of(s):];
          constexpr auto offset           = offset_of(s);
          auto           candidate_offset = *reinterpret_cast<size_t*>(_a[1]);
          if(candidate_offset == offset.bytes) // sig)
          {
            int* result = static_cast<int*>(_a[0]);
            *result     = ii;
            return;
          }
        }
      }
      template for(constexpr auto ii : std::views::iota(size_t(0), props.size()))
      {
        static constexpr auto p = props[ii];
        static constexpr auto s = ^^object::propertyChanged<p>;
        using signature_type    = [:meta::signature_of<s, ^^object>():];
        if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s:], ii + sig_count))
        {
          return;
        }
        static constexpr auto fixed_name = meta::static_identifier_wrapper_of<p>().to_fixed();
        static constexpr auto s2         = ^^object::propertyChanged<fixed_name>;
        using signature_type2            = [:meta::signature_of<s2, ^^object>():];
        if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s2:], ii + sig_count))
        {
          return;
        }
      }
    }

    if(_c == QMetaObject::ReadProperty)
    {
      void* _v = _a[0];

      template for(constexpr auto ii : std::views::iota(size_t(0), props.size()))
      {
        if(ii == _id)
        {
          static constexpr auto p   = props[ii];
          using T                   = [:type_of(p):];
          *reinterpret_cast<T*>(_v) = _t->template property<p>();
          return;
        }
      }
    }
    if(_c == QMetaObject::WriteProperty)
    {
      void* _v = _a[0];
      template for(constexpr auto ii : std::views::iota(size_t(0), props.size()))
      {
        if(ii == _id)
        {
          static constexpr auto p = props[ii];
          using T                 = [:type_of(p):];
          _t->template setProperty<p>(*reinterpret_cast<T*>(_v));
          return;
        }
      }
    }
  }

public:
  static const inline QMetaObject staticMetaObject = {{QMetaObject::SuperData::link<QObject::staticMetaObject>(),
                                                       qt_staticMetaObjectStaticContent<tag>.stringdata,
                                                       qt_staticMetaObjectStaticContent<tag>.data,
                                                       qt_static_metacall,
                                                       nullptr,
                                                       qt_staticMetaObjectRelocatingContent<tag>.metaTypes,
                                                       nullptr}};

  const QMetaObject* metaObject() const override
  {
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
  }

  void* qt_metacast(const char* _clname) override
  {
    if(!_clname)
      return nullptr;
    if(!strcmp(_clname, qt_staticMetaObjectStaticContent<tag>.strings))
      return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
  }

  int qt_metacall(QMetaObject::Call _c, int _id, void** _a) override
  {
    static constexpr auto sigs              = __signals();
    static constexpr auto slts              = __slots();
    static constexpr auto invoks            = __invocables();
    static constexpr auto signal_slot_count = sigs.size() + slts.size() + invoks.size();

    _id = QObject::qt_metacall(_c, _id, _a);
    if(_id < 0)
      return _id;

    if(_c == QMetaObject::InvokeMetaMethod)
    {
      if(_id <= signal_slot_count)
        qt_static_metacall(this, _c, _id, _a);
      _id -= signal_slot_count;
    }
    if(_c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      if(_id <= signal_slot_count)
        *reinterpret_cast<QMetaType*>(_a[0]) = QMetaType();
      _id -= signal_slot_count;
    }
    if(_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty || _c == QMetaObject::ResetProperty ||
       _c == QMetaObject::BindableProperty || _c == QMetaObject::RegisterPropertyMetaType)
    {
      qt_static_metacall(this, _c, _id, _a);
      _id -= 1;
    }
    return _id;
  }
  QT_WARNING_POP

  using QObject::property;

  template <meta::info Property> auto property(this auto& self)
  {
    using std::ranges::contains;
    static constexpr auto props = __properties();
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

  template <fixed_string name> auto property(this auto& self)
  {
    using std::ranges::contains;
    static constexpr auto prop = meta::member_named(^^Super, name.view(), meta::access_context::unchecked());
    static_assert(prop != meta::null, "no such property");
    return self.template property<prop>();
  }

  using QObject::setProperty;

  template <meta::info Property, typename T> void setProperty(this auto& self, T&& value)
  {
    using std::ranges::find;
    static constexpr auto props = __properties();
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

      static constexpr auto relative_offset   = std::distance(begin(props), it);
      static constexpr auto sigs              = __signals();
      static constexpr auto slts              = __slots();
      static constexpr auto signal_slot_count = sigs.size() /* + slts.size() */;
      QMetaObject::activate<void>(&self, &staticMetaObject, signal_slot_count + relative_offset, nullptr);
    }
    else
    {
      static_assert(always_false<Super>, "no such property");
    }
  }

  template <fixed_string name, typename T> void setProperty(this auto& self, T&& value)
  {
    using std::ranges::contains;
    static constexpr auto prop = meta::member_named(^^Super, name.view(), meta::access_context::unchecked());
    static_assert(prop != meta::null, "no such property");
    self.template setProperty<prop>(std::forward<T>(value));
  }

  template <meta::info Property> void propertyChanged()
  {
    std::unreachable(); // NOTE: actually unused, just to allow QObject::connect to match the auto-generated signal
  }

  template <fixed_string name> void propertyChanged()
  {
    std::unreachable(); // NOTE: actually unused, just to allow QObject::connect to match the auto-generated signal
  }

public:
  using QObject::startTimer;

  template <meta::info Event> void startTimer(int periodMs)
  {
    using object_data_type = decltype(__make_object_data());

    using std::ranges::find;
    static constexpr auto events = __timer_events();
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
    static constexpr auto events = __timer_events();
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
    static constexpr auto events = __timer_events();
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
  if(has_template_arguments(first_base_type) and template_of(first_base_type) == ^^reflex::qt::object)
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
  using Arguments  = List<Args...>;
  using ReturnType = void;
  typedef ReturnType (reflex::qt::detail::signal_decl<ObjectT, Args...>::*Function)(Args...);
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
