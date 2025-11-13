#pragma once

#include <reflex/meta.hpp>

#include <reflex/qt/detail/meta_strings.hpp>

#include <reflex/qt/gadget.hpp>
#include <reflex/qt/object.hpp>

#undef signals
#undef slots

namespace reflex::qt
{
namespace detail
{
template <typename Super> struct gadget_impl
{
  static constexpr bool is_object = meta::is_subclass_of(^^Super, ^^object);
  static constexpr bool is_gadget = not is_object;
  using object_type               = [:[] consteval
                       {
                         if constexpr(is_object)
                         {
                           return ^^object<Super, typename Super::parent_type>;
                         }
                         else
                         {
                           return ^^gadget<Super>;
                         }
                       }():];
  using tag                       = object_type::tag;

  template <typename Tag> static constexpr auto qt_create_metaobjectdata()
  {
    static constexpr auto ms = meta_strings<Tag, Super>{};
    return ms.create_meta_objectdata();
  }

  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectContent = qt_create_metaobjectdata<MetaObjectTagType>();
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectStaticContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.staticData;
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectRelocatingContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.relocatingData;

  static constexpr QMetaObject metaObject()
  {
    return {{[]
             {
               if constexpr(is_object)
               {
                 return QMetaObject::SuperData::link<Super::parent_type::staticMetaObject>();
               }
               else
               {
                 return nullptr;
               }
             }(),
             qt_staticMetaObjectStaticContent<tag>.stringdata,
             qt_staticMetaObjectStaticContent<tag>.data,
             qt_static_metacall,
             nullptr,
             qt_staticMetaObjectRelocatingContent<tag>.metaTypes,
             nullptr}};
  }

  static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
  {
    auto* _t = reinterpret_cast<Super*>(_o);

    constexpr auto ms     = meta_strings<tag, Super>{};
    constexpr auto sigs   = ms.signals;
    constexpr auto invocs = ms.invocables;
    constexpr auto slts   = ms.slots;
    constexpr auto props  = ms.properties;

    const auto do_invoke = []<meta::info R, size_t N>(Super* self, void** args)
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
            }(std::make_index_sequence<N>());
      }
      else
      {
        using Ret = [:return_type:];
        Ret ret = [&]<size_t ...I>(std::index_sequence<I...>) {//
              return self->[:R:](get_arg.template operator()<I>()...);
            }(std::make_index_sequence<N>());
        if(args[0])
        {
          *reinterpret_cast<Ret*>(args[0]) = std::move(ret);
        }
      }
    };

    if(_c == QMetaObject::InvokeMetaMethod)
    {
      if constexpr(is_gadget)
      {
        std::abort();
      }
      else
      {
        {
          size_t local_signal_index = 0;
          template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
          {
            static constexpr auto s = sigs[ii];
            using CurrentSignalT    = [:type_of(s):];
            template for(constexpr auto jj : std::views::iota(size_t(0), CurrentSignalT::defaulted_args_count_ + 1))
            {
              if(local_signal_index == _id)
              {
                static constexpr auto parameters = define_static_array(Super::template parameters_types_of<s>()
                                                                       | std::views::transform(meta::remove_reference));
                const auto            get_arg    = []<size_t I>(void** args)
                {
                  constexpr auto p = parameters[I];
                  using T          = typename[:p:];
                  return *reinterpret_cast<T*>(args[I + 1]);
                };

                [&]<size_t ...I>(std::index_sequence<I...>, Super* self, void**args) {//
                self->template trigger<s>(get_arg.template operator()<I>(args)...);
              }(std::make_index_sequence<parameters.size() - jj>(), _t, _a);
                return;
              }
              ++local_signal_index;
            }
          }
          _id -= local_signal_index;
        }

        template for(constexpr auto ii : std::views::iota(size_t(0), props.size()))
        {
          if(ii == _id)
          {
            static constexpr auto p = props[ii];
            static constexpr auto s = ^^Super::template propertyChanged<p>;
            do_invoke.template    operator()<s, 0>(_t, _a);
            return;
          }
        }
        _id -= ms.property_count;

        {
          size_t local_slot_index = 0;
          template for(constexpr auto ii : std::views::iota(size_t(0), slts.size()))
          {
            static constexpr auto s           = slts[ii];
            static constexpr auto parameters  = define_static_array(parameters_of(s));
            constexpr size_t      n_overloads = 1 + std::ranges::count_if(parameters, meta::has_default_argument);
            template for(constexpr auto jj : std::views::iota(size_t(0), n_overloads))
            {
              if(local_slot_index == _id)
              {
                do_invoke.template operator()<s, parameters.size() - jj>(_t, _a);
                return;
              }
              ++local_slot_index;
            }
          }
          _id -= local_slot_index;
        }
        {
          size_t local_invoc_index = 0;
          template for(constexpr auto ii : std::views::iota(size_t(0), invocs.size()))
          {
            static constexpr auto s           = invocs[ii];
            static constexpr auto parameters  = define_static_array(parameters_of(s));
            constexpr size_t      n_overloads = 1 + std::ranges::count_if(parameters, meta::has_default_argument);
            template for(constexpr auto jj : std::views::iota(size_t(0), n_overloads))
            {
              if(local_invoc_index == _id)
              {
                do_invoke.template operator()<s, parameters.size() - jj>(_t, _a);
                return;
              }
              ++local_invoc_index;
            }
          }
        }
      }
    }
    if(_c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      if constexpr(is_gadget)
      {
        std::abort();
      }
      else
      {
        const auto     arg_num      = *reinterpret_cast<int*>(_a[1]);
        auto*          result       = reinterpret_cast<QMetaType*>(_a[0]);
        constexpr auto custom_types = ms.custom_types;

        template for(constexpr auto ii : std::views::iota(size_t(0), sigs.size()))
        {
          if(ii == _id)
          {
            static constexpr auto s = sigs[ii];

            static constexpr auto parameters = define_static_array(Super::template parameters_types_of<s>()
                                                                   | std::views::transform(meta::remove_reference));

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
        _id -= ms.signal_count;

        *result = QMetaType();
      }
    }
    if(_c == QMetaObject::IndexOfMethod)
    {
      if constexpr(is_gadget)
      {
        std::abort();
      }
      else
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
          static constexpr auto s = ^^Super::template propertyChanged<p>;
          using signature_type    = [:meta::signature_of<s, ^^Super>():];
          if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s:], ii + ms.signal_count))
          {
            return;
          }

          static constexpr auto s2 = ^^Super::template propertyChanged<constant{identifier_of(p)}>;
          using signature_type2    = [:meta::signature_of<s2, ^^Super>():];
          if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s2:], ii + ms.signal_count))
          {
            return;
          }
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
};
} // namespace detail

template <typename Super> void gadget<Super>::qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
{
  return detail::gadget_impl<Super>::qt_static_metacall(_o, _c, _id, _a);
}
template <typename T> constinit const QMetaObject gadget<T>::staticMetaObject = detail::gadget_impl<T>::metaObject();

} // namespace reflex::qt