#pragma once

#include <reflex/meta.hpp>
#include <reflex/qt/detail/meta_strings.hpp>

#include <QtCore/qmetaobject.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>

#include <cstddef>
#include <cstring>
#include <ranges>
#include <utility>

namespace reflex::qt::detail
{
/** @brief matches an `IndexOfMethod` candidate against a notify signal
 *
 * `QtMocHelpers::indexOfMethod` reads `_a[1]` as an object of the type it is
 * given. A notify signal is a pointer to member function, twice the size of the
 * pointer to data member a reflex signal is, so reading one where the caller
 * stored the other runs off the end of the caller's object. Only the leading
 * word is compared here, which is in bounds for either.
 */
template <typename Notifier> inline bool index_of_notifier(void** a, Notifier notifier, int index) noexcept
{
  const void* candidate = nullptr;
  const void* target    = nullptr;
  std::memcpy(&candidate, a[1], sizeof(candidate));
  std::memcpy(&target, &notifier, sizeof(target));
  if(candidate != target)
  {
    return false;
  }
  *static_cast<int*>(a[0]) = index;
  return true;
}

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

  static constexpr const char* class_name()
  {
    return qt_staticMetaObjectStaticContent<tag>.strings;
  }

  static constexpr QMetaObject::SuperData superdata()
  {
    if constexpr(strings::is_object)
    {
      return QMetaObject::SuperData::link<Super::parent_type::staticMetaObject>();
    }
    else
    {
      return nullptr;
    }
  }

  static constexpr QMetaObject metaObject()
  {
    return {
        {superdata(),
         qt_staticMetaObjectStaticContent<tag>.stringdata,
         qt_staticMetaObjectStaticContent<tag>.data,
         qt_static_metacall,
         nullptr,
         qt_staticMetaObjectRelocatingContent<tag>.metaTypes,
         nullptr}
    };
  }

  static constexpr Super* recover(QObject* o)
  {
    if constexpr(strings::is_object)
    {
      return static_cast<Super*>(o);
    }
    else
    {
      return reinterpret_cast<Super*>(o);
    }
  }

  template <meta::info Member, std::size_t N> static void invoke(Super* self, void** args)
  {
    static constexpr auto parameters = define_static_array(parameters_of(call_function_of(Member)));

    const auto call = [&]<std::size_t... I>(std::index_sequence<I...>) -> decltype(auto)
    {
      return self->[:Member:](
          *reinterpret_cast<typename[:meta::remove_cvref(type_of(parameters[I])):]*>(args[I + 1])...);
    };

    constexpr auto return_type = call_return_type_of(Member);
    if constexpr(return_type == ^^void)
    {
      call(std::make_index_sequence<N>());
    }
    else
    {
      using return_value_type = [:return_type:];
      return_value_type value = call(std::make_index_sequence<N>());
      if(args[0])
      {
        *reinterpret_cast<return_value_type*>(args[0]) = std::move(value);
      }
    }
  }

  static void qt_static_metacall(QObject* o, QMetaObject::Call c, int id, void** a)
  {
    [[maybe_unused]] auto* self = recover(o);

    if(c == QMetaObject::InvokeMetaMethod)
    {
      template for(constexpr auto i : std::views::iota(0uz, strings::method_count))
      {
        if(int(i) == id)
        {
          if constexpr(strings::methods[i].kind == method_kind::notifier)
          {
            QMetaObject::activate(self, &Super::staticMetaObject, int(i), nullptr);
          }
          else
          {
            invoke<strings::methods[i].member, strings::methods[i].arity>(self, a);
          }
          return;
        }
      }
    }
    else if(c == QMetaObject::IndexOfMethod)
    {
      if constexpr(strings::is_object)
      {
        template for(constexpr auto i : std::views::iota(0uz, strings::method_count))
        {
          if constexpr(strings::methods[i].kind == method_kind::signal_member
                       and not strings::methods[i].cloned)
          {
            if(QtMocHelpers::indexOfMethod(a, &[:strings::methods[i].member:], int(i)))
            {
              return;
            }
          }
          else if constexpr(strings::methods[i].kind == method_kind::notifier)
          {
            constexpr auto notifier =
                &Super::template propertyChanged<constant_string{
                    identifier_of(strings::methods[i].member)}>;
            if(index_of_notifier(a, notifier, int(i)))
            {
              return;
            }
          }
        }
      }
    }
    else if(c == QMetaObject::ReadProperty)
    {
      template for(constexpr auto i : std::views::iota(0uz, strings::property_count))
      {
        if constexpr(property_spec_of(strings::properties[i]).read)
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
    }
    else if(c == QMetaObject::WriteProperty)
    {
      template for(constexpr auto i : std::views::iota(0uz, strings::property_count))
      {
        if constexpr(property_spec_of(strings::properties[i]).write)
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

      template for(constexpr auto i : std::views::iota(0uz, strings::method_count))
      {
        if(int(i) == id)
        {
          if constexpr(strings::methods[i].kind != method_kind::notifier)
          {
            static constexpr auto parameters =
                define_static_array(parameters_of(call_function_of(strings::methods[i].member)));
            const auto argument = *reinterpret_cast<int*>(a[1]);
            template for(constexpr auto j : std::views::iota(0uz, parameters.size()))
            {
              if(int(j) == argument)
              {
                using parameter_type = [:meta::remove_cvref(type_of(parameters[j])):];
                *result              = QMetaType::fromType<parameter_type>();
                return;
              }
            }
          }
          return;
        }
      }
    }
  }
};
}
