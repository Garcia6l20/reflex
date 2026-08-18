#pragma once

#include <reflex/meta.hpp>
#include <reflex/qt/detail/meta_strings.hpp>

#include <QtCore/qmetaobject.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>

#include <cstddef>
#include <ranges>
#include <utility>

namespace reflex::qt::detail
{
/** @brief the `QMetaObject` of @p Super and the dispatcher it points at */
template <typename Super> struct gadget_impl
{
  struct tag
  {
  };

  using strings = meta_strings<tag, Super>;

  template <typename MetaObjectTagType> static consteval auto qt_create_metaobjectdata()
  {
    return meta_strings<MetaObjectTagType, Super>::create_meta_objectdata();
  }

  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectContent =
      qt_create_metaobjectdata<MetaObjectTagType>();
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectStaticContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.staticData;
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectRelocatingContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.relocatingData;

  static constexpr QMetaObject metaObject()
  {
    return {
        {nullptr,
         qt_staticMetaObjectStaticContent<tag>.stringdata,
         qt_staticMetaObjectStaticContent<tag>.data,
         qt_static_metacall,
         nullptr,
         qt_staticMetaObjectRelocatingContent<tag>.metaTypes,
         nullptr}
    };
  }

  template <meta::info Fn> static void invoke(Super* self, void** args)
  {
    static constexpr auto parameters = define_static_array(parameters_of(Fn));

    const auto call = [&]<std::size_t... I>(std::index_sequence<I...>) -> decltype(auto)
    {
      return self->[:Fn:](
          *reinterpret_cast<typename[:meta::remove_cvref(type_of(parameters[I])):]*>(args[I + 1])...);
    };

    constexpr auto return_type = return_type_of(Fn);
    if constexpr(return_type == ^^void)
    {
      call(std::make_index_sequence<parameters.size()>());
    }
    else
    {
      using return_value_type = [:return_type:];
      return_value_type value = call(std::make_index_sequence<parameters.size()>());
      if(args[0])
      {
        *reinterpret_cast<return_value_type*>(args[0]) = std::move(value);
      }
    }
  }

  static void qt_static_metacall(QObject* o, QMetaObject::Call c, int id, void** a)
  {
    [[maybe_unused]] auto* self = reinterpret_cast<Super*>(o);

    if(c == QMetaObject::InvokeMetaMethod)
    {
      template for(constexpr auto i : std::views::iota(0uz, strings::invocable_count))
      {
        if(int(i) == id)
        {
          invoke<strings::invocables[i]>(self, a);
          return;
        }
      }
    }
    else if(c == QMetaObject::ReadProperty)
    {
      template for(constexpr auto i : std::views::iota(0uz, strings::property_count))
      {
        if(int(i) == id)
        {
          using property_type = [:type_of(strings::properties[i]):];
          *reinterpret_cast<property_type*>(a[0]) =
              self->template property<strings::properties[i]>();
          return;
        }
      }
    }
    else if(c == QMetaObject::WriteProperty)
    {
      template for(constexpr auto i : std::views::iota(0uz, strings::property_count))
      {
        if(int(i) == id)
        {
          using property_type = [:type_of(strings::properties[i]):];
          self->template setProperty<strings::properties[i]>(
              *reinterpret_cast<property_type*>(a[0]));
          return;
        }
      }
    }
    else if(c == QMetaObject::RegisterPropertyMetaType)
    {
      *reinterpret_cast<int*>(a[0]) = -1;
      template for(constexpr auto i : std::views::iota(0uz, strings::property_count))
      {
        if(int(i) == id)
        {
          using property_type           = [:type_of(strings::properties[i]):];
          *reinterpret_cast<int*>(a[0]) = qRegisterMetaType<property_type>();
          return;
        }
      }
    }
    else if(c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      auto* result = reinterpret_cast<QMetaType*>(a[0]);
      *result      = QMetaType();

      template for(constexpr auto i : std::views::iota(0uz, strings::invocable_count))
      {
        if(int(i) == id)
        {
          static constexpr auto parameters = define_static_array(parameters_of(strings::invocables[i]));
          const auto            argument   = *reinterpret_cast<int*>(a[1]);
          template for(constexpr auto j : std::views::iota(0uz, parameters.size()))
          {
            if(int(j) == argument)
            {
              using parameter_type = [:meta::remove_cvref(type_of(parameters[j])):];
              *result              = QMetaType::fromType<parameter_type>();
              return;
            }
          }
          return;
        }
      }
    }
  }
};
}
