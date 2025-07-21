#pragma once

#include <reflex/meta.hpp>
#include <reflex/utility.hpp>

#include <reflex/qt/detail/metatype.hpp>

#include <QObject>
#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

#include <memory>
#include <print>

namespace reflex::qt
{

namespace detail
{
struct signal_annotation
{
};
struct slot_annotation
{
};
struct invocable_annotation
{
};
template <fixed_string... spec> struct property_annotation
{
  static constexpr auto specs      = std::tuple{spec...};
  static constexpr auto read_pos   = std::get<0>(specs).view().find('r');
  static constexpr auto write_pos  = std::get<0>(specs).view().find('w');
  static constexpr auto notify_pos = std::get<0>(specs).view().find('n');

  static constexpr auto readable = read_pos != std::string_view::npos;
  static constexpr auto writable = write_pos != std::string_view::npos;
  static constexpr auto notify   = notify_pos != std::string_view::npos;

  static constexpr std::string_view read_name()
  {
    if constexpr(not readable)
    {
      return "";
    }
    else
    {
      return std::get<1 + read_pos>(specs);
    }
  }

  static constexpr std::string_view write_name()
  {
    if constexpr(not writable)
    {
      return "";
    }
    else
    {
      return std::get<1 + write_pos>(specs);
    }
  }

  static constexpr std::string_view notify_name()
  {
    if constexpr(not notify)
    {
      return "";
    }
    else
    {
      return std::get<1 + notify_pos>(specs);
    }
  }
};
} // namespace detail

inline constexpr detail::signal_annotation                                            signal;
inline constexpr detail::slot_annotation                                              slot;
inline constexpr detail::invocable_annotation                                         invocable;
template <fixed_string... spec> inline constexpr detail::property_annotation<spec...> property;

template <typename Super> struct object : QObject
{
public:
  static consteval auto __signals()
  {
    return members_of(^^Super, meta::access_context::unchecked()) //
           | std::views::filter(meta::is_user_declared)           //
           | std::views::filter(meta::is_function)                //
           | std::views::filter(
                 [](auto member)
                 {
                   return std::ranges::contains(annotations_of(member) | std::views::transform(meta::type_of),
                                                ^^detail::signal_annotation);
                 });
  }

  static consteval auto __slots()
  {
    return members_of(^^Super, meta::access_context::unchecked()) //
           | std::views::filter(meta::is_user_declared)           //
           | std::views::filter(meta::is_function)                //
           | std::views::filter(
                 [](auto member)
                 {
                   return std::ranges::contains(annotations_of(member) | std::views::transform(meta::type_of),
                                                ^^detail::slot_annotation);
                 });
  }

  static consteval auto __invocables()
  {
    return members_of(^^Super, meta::access_context::unchecked()) //
           | std::views::filter(meta::is_user_declared)           //
           | std::views::filter(meta::is_function)                //
           | std::views::filter(
                 [](auto member)
                 {
                   return std::ranges::contains(annotations_of(member) | std::views::transform(meta::type_of),
                                                ^^detail::invocable_annotation);
                 });
  }

  static consteval auto __properties()
  {
    return nonstatic_data_members_of(^^Super, meta::access_context::unchecked()) //
           | std::views::filter(
                 [](auto member)
                 {
                   return std::ranges::contains(annotations_of(member)                                 //
                                                    | std::views::transform(meta::type_of)             //
                                                    | std::views::filter(meta::has_template_arguments) //
                                                    | std::views::transform(meta::template_of),
                                                ^^detail::property_annotation);
                 });
  }

public:
  explicit object(QObject* parent = nullptr) : QObject{parent}
  {
  }
  virtual ~object() = default;

  template <meta::info Signal, typename... Args> auto trigger(Args... args)
  {
    constexpr auto sigs = define_static_array(__signals());

    template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
    {
      constexpr auto s = sigs[ii];
      if constexpr(s == Signal)
      {
        QMetaObject::activate<void>(this, &staticMetaObject, ii, nullptr, std::forward<Args>(args)...);
      }
    }
  }

  QT_WARNING_PUSH
  Q_OBJECT_NO_OVERRIDE_WARNING
  // static const QMetaObject   staticMetaObject;
  // virtual const QMetaObject* metaObject() const;
  // virtual void*              qt_metacast(const char*);
  // virtual int                qt_metacall(QMetaObject::Call, int, void**);
  QT_TR_FUNCTIONS
public:
  // QT_OBJECT_GADGET_COMMON
  // QT_META_OBJECT_VARS

  static constexpr auto __get_strings()
  {
    namespace QMC = QtMocConstants;

    std::vector<meta::info> strings;
    strings.push_back(meta::static_identifier_wrapper_type_of<^^Super>());
    strings.push_back(^^meta::static_string_wrapper<'\0'>); // add an empty entry

    template for(constexpr auto s : define_static_array(__signals()))
    {
      strings.push_back(meta::static_identifier_wrapper_type_of<s>());
    }

    template for(constexpr auto p : define_static_array(__properties()))
    {
      constexpr auto id          = meta::static_identifier_wrapper_of<p>();
      constexpr auto signal_name = id.template with_suffix<"Changed">();
      strings.push_back(type_of(^^signal_name));
    }

    template for(constexpr auto s : define_static_array(__slots()))
    {
      strings.push_back(meta::static_identifier_wrapper_type_of<s>());
    }

    template for(constexpr auto i : define_static_array(__invocables()))
    {
      strings.push_back(meta::static_identifier_wrapper_type_of<i>());
    }

    template for(constexpr auto p : define_static_array(__properties()))
    {
      strings.push_back(meta::static_identifier_wrapper_type_of<p>());
    }

    return define_static_array(strings);
  }

  template <typename> static constexpr auto qt_create_metaobjectdata()
  {
    namespace QMC                 = QtMocConstants;
    static constexpr auto strings = __get_strings();

    auto qt_stringData = [&] constexpr
    {
      return [&]<std::size_t... I>(std::index_sequence<I...>)
      { return QtMocHelpers::StringRefStorage{[:strings[I]:] ::data...}; }(std::make_index_sequence<strings.size()>());
    }();

    size_t strings_index = 2;

    constexpr auto sigs   = define_static_array(__signals());
    constexpr auto invocs = define_static_array(__invocables());
    constexpr auto slts   = define_static_array(__slots());
    constexpr auto props  = define_static_array(__properties());

    auto qt_methods = [&] constexpr
    {
      auto make_data_impl = [&]<meta::info s, auto qt_method_type>() constexpr
      {
        constexpr auto method_type = type_of(s);
        using SignalT              = [:method_type:];
        using RetT                 = [:return_type_of(method_type):];
        using SignatureT           = typename[:meta::signature_of<s>():];
        using DataT                = QtMocHelpers::FunctionData<SignatureT, qt_method_type>;

        auto make_parameters_data = [&] constexpr
        {
          constexpr auto parameters     = define_static_array(parameters_of(s));
          auto           make_parameter = [&]<meta::info p>
          {
            constexpr auto param_type = type_of(p);
            return typename DataT::FunctionParameter{
                .typeIdx = detail::static_meta_type_id_of(param_type),
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
            is_public(s)      ? QMC::AccessPublic
            : is_protected(s) ? QMC::AccessProtected
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

    // QtMocHelpers::UintData qt_properties{};
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

  template <meta::info R> static consteval auto make_property_listener_name()
  {
    constexpr auto             identifier         = identifier_of(R);
    constexpr std::string_view suffix             = "_changed";
    constexpr auto             listener_name_size = identifier.size() + suffix.size();
    char                       listener_name[listener_name_size];
    std::ranges::copy(identifier, listener_name);
    std::ranges::copy(suffix, listener_name + identifier.size());
    return std::array{listener_name};
  }

  static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
  {
    auto* _t = static_cast<Super*>(_o);

    constexpr auto sigs   = define_static_array(__signals());
    constexpr auto invocs = define_static_array(__invocables());
    constexpr auto slts   = define_static_array(__slots());
    constexpr auto props  = define_static_array(__properties());

    constexpr auto sig_count  = sigs.size();
    constexpr auto slts_count = slts.size();

    const auto do_invoke = [&]<meta::info R>()
    {
      static constexpr auto parameters = define_static_array(parameters_of(R));
      const auto            get_arg    = [&]<size_t I>
      {
        constexpr auto p = type_of(parameters[I]);
        using T          = typename[:p:];
        return *reinterpret_cast<T*>(_a[I + 1]);
      };
      constexpr auto return_type = return_type_of(R);
      if constexpr(return_type == ^^void)
      {
        [&]<size_t ...I>(std::index_sequence<I...>) {//
              _t->[:R:](get_arg.template operator()<I>()...);
            }(std::make_index_sequence<parameters.size()>());
      }
      else
      {
        using Ret = [:return_type:];
        Ret ret = [&]<size_t ...I>(std::index_sequence<I...>) {//
              return _t->[:R:](get_arg.template operator()<I>()...);
            }(std::make_index_sequence<parameters.size()>());
        if(_a[0])
        {
          *reinterpret_cast<Ret*>(_a[0]) = std::move(ret);
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
          do_invoke.template    operator()<s>();
          return;
        }
      }
      _id -= sig_count;
      template for(constexpr auto ii : std::views::iota(size_t(0), slts.size()))
      {
        if(ii == _id)
        {
          static constexpr auto s = slts[ii];
          do_invoke.template    operator()<s>();
          return;
        }
      }
      _id -= slts_count;
      template for(constexpr auto ii : std::views::iota(size_t(0), invocs.size()))
      {
        if(ii == _id)
        {
          static constexpr auto s = invocs[ii];
          do_invoke.template    operator()<s>();
          return;
        }
      }
    }
    if(_c == QMetaObject::IndexOfMethod)
    {
      template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
      {
        constexpr auto s     = sigs[ii];
        using signature_type = [:meta::signature_of<s, ^^Super>():];
        if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s:], ii))
        {
          return;
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
        static constexpr auto s2 = ^^object::propertyChanged<fixed_name>;
        using signature_type2    = [:meta::signature_of<s2, ^^object>():];
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
          *reinterpret_cast<T*>(_v) = _t->[:p:];
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
  // static const QMetaObject staticMetaObject;

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
    static constexpr auto sigs              = define_static_array(__signals());
    static constexpr auto slts              = define_static_array(__slots());
    static constexpr auto signal_slot_count = sigs.size() + slts.size();

    _id = QObject::qt_metacall(_c, _id, _a);
    if(_id < 0)
      return _id;

    if(_c == QMetaObject::InvokeMetaMethod)
    {
      if(_id < signal_slot_count)
        qt_static_metacall(this, _c, _id, _a);
      _id -= signal_slot_count;
    }
    if(_c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      if(_id < signal_slot_count)
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

public:
  using QObject::property;

  template <meta::info Property> auto property()
  {
    using std::ranges::contains;
    static constexpr auto props = define_static_array(__properties());
    if constexpr(contains(props, Property))
    {
      return static_cast<Super*>(this)->[:Property:];
    }
    else
    {
      static_assert(always_false<Super>, "no such property");
    }
  }

  template <fixed_string name> auto property()
  {
    using std::ranges::contains;
    static constexpr auto prop = meta::member_named(^^Super, name.view(), meta::access_context::unchecked());
    return property<prop>();
  }

  using QObject::setProperty;

  template <meta::info Property, typename T> void setProperty(T&& value)
  {
    using std::ranges::find;
    static constexpr auto props = define_static_array(__properties());
    static constexpr auto it    = find(props, Property);
    if constexpr(it != end(props))
    {
      auto& val = static_cast<Super*>(this)->[:Property:];

      if(value != val)
      {
        val = std::forward<T>(value);

        static constexpr auto identifier =
            meta::static_identifier_wrapper_of<Property>().template with_suffix<"_written">().view();
        static constexpr auto listener = meta::member_named(^^Super, identifier, meta::access_context::unchecked());
        if constexpr(listener != ^^void)
        {
          static_cast<Super*>(this)->[:listener:]();
        }

        static constexpr auto relative_offset   = std::distance(begin(props), it);
        static constexpr auto sigs              = define_static_array(__signals());
        static constexpr auto slts              = define_static_array(__slots());
        static constexpr auto signal_slot_count = sigs.size() /* + slts.size() */;
        QMetaObject::activate<void>(this, &staticMetaObject, signal_slot_count + relative_offset, nullptr);
      }
    }
    else
    {
      static_assert(always_false<Super>, "no such property");
    }
  }

  template <fixed_string name, typename T> void setProperty(T&& value)
  {
    using std::ranges::contains;
    static constexpr auto prop = meta::member_named(^^Super, name.view(), meta::access_context::unchecked());
    setProperty<prop>(std::forward<T>(value));
  }

  template <meta::info Property> void propertyChanged()
  {
    std::unreachable();
  }

  template <fixed_string name> void propertyChanged()
  {
    std::unreachable();
  }
};

} // namespace reflex::qt

namespace QtPrivate
{
template <typename Super>
  requires(type_of(bases_of(^^Super, std::meta::access_context::current())[0]) == ^^reflex::qt::object<Super>)
struct HasQ_OBJECT_Macro<Super>
{
  enum
  {
    Value = 1
  };
};
} // namespace QtPrivate
