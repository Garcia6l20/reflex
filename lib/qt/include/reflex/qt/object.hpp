#pragma once

#include <reflex/meta.hpp>
#include <reflex/qt/detail/metatype.hpp>

#include <QObject>
#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

#include <memory>

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
} // namespace detail

inline constexpr detail::signal_annotation signal;
inline constexpr detail::slot_annotation   slot;

template <typename Super> struct object : QObject
{
private:
  static consteval auto __members()
  {
    return meta::nonstatic_data_members_of(^^Super, std::meta::access_context::current());
  }

  static consteval auto __signals()
  {
    return members_of(^^Super, meta::access_context::current()) //
           | std::views::filter(meta::is_user_declared)         //
           | std::views::filter(meta::is_function)              //
           | std::views::filter(
                 [](auto member)
                 {
                   return std::ranges::contains(annotations_of(member) | std::views::transform(meta::type_of),
                                                ^^detail::signal_annotation);
                 });
  }

  static consteval auto __slots()
  {
    return members_of(^^Super, meta::access_context::current()) //
           | std::views::filter(meta::is_user_declared)         //
           | std::views::filter(meta::is_function)              //
           | std::views::filter(
                 [](auto member)
                 {
                   return std::ranges::contains(annotations_of(member) | std::views::transform(meta::type_of),
                                                ^^detail::slot_annotation);
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
private:
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

    template for(constexpr auto s : define_static_array(__slots()))
    {
      strings.push_back(meta::static_identifier_wrapper_type_of<s>());
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

      constexpr auto sigs         = define_static_array(__signals());
      const auto     signals_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::make_tuple(make_data_impl.template operator()<sigs[I], QtMocConstants::MethodSignal>()...);
      }(std::make_index_sequence<sigs.size()>());

      constexpr auto slts       = define_static_array(__slots());
      const auto     slots_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::make_tuple(make_data_impl.template operator()<slts[I], QtMocConstants::MethodSlot>()...);
      }(std::make_index_sequence<slts.size()>());

      return std::apply([]<typename... Args>(Args... args)
                        { return QtMocHelpers::UintData<Args...>{std::move(args)...}; },
                        std::tuple_cat(signals_data, slots_data));
    }();

    QtMocHelpers::UintData qt_properties{};
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

  static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
  {
    auto* _t = static_cast<Super*>(_o);

    constexpr auto sigs = define_static_array(__signals());
    if(_c == QMetaObject::InvokeMetaMethod)
    {
      template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
      {
        if(ii == _id)
        {
          static constexpr auto s          = sigs[ii];
          static constexpr auto parameters = define_static_array(parameters_of(s));
          static const auto     get_arg    = []<size_t I>(void** args)
          {
            constexpr auto p = type_of(parameters[I]);
            using T          = typename[:p:];
            return *reinterpret_cast<T*>(args[I]);
          };
          []<size_t ...I>(std::index_sequence<I...>, Super* self, void **args) {//
            self->[:s:](get_arg.template operator()<I>(args)...);
          }(std::make_index_sequence<parameters.size()>(), _t, _a);
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
    }

    // auto* _t = static_cast<QTestObject*>(_o);
    // if(_c == QMetaObject::InvokeMetaMethod)
    // {
    //   switch(_id)
    //   {
    //     case 0:
    //       _t->emptySig();
    //       break;
    //     case 1:
    //       _t->emptySig2();
    //       break;
    //     case 2:
    //       _t->emtpySlot();
    //       break;
    //     default:;
    //   }
    // }
    // if(_c == QMetaObject::IndexOfMethod)
    // {
    //   if(QtMocHelpers::indexOfMethod<void (QTestObject::*)()>(_a, &QTestObject::emptySig, 0))
    //     return;
    //   if(QtMocHelpers::indexOfMethod<void (QTestObject::*)()>(_a, &QTestObject::emptySig2, 1))
    //     return;
    // }
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
    // ensure_signals_initialized();

    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
  }
  QT_WARNING_POP
};

// template <typename Super>
// const QMetaObject object<Super>::staticMetaObject = {{QMetaObject::SuperData::link<QObject::staticMetaObject>(),
//                                                       qt_staticMetaObjectStaticContent<tag>.stringdata,
//                                                       qt_staticMetaObjectStaticContent<tag>.data,
//                                                       qt_static_metacall,
//                                                       nullptr,
//                                                       qt_staticMetaObjectRelocatingContent<tag>.metaTypes,
//                                                       nullptr}};

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
