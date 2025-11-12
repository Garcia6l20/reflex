#pragma once

#include <reflex/qt/object.hpp>

#undef signals
#undef slots

namespace reflex::qt
{

namespace detail
{
template <typename Super, typename Parent> struct meta_impl
{
  using object_type = object<Super, Parent>;
  using tag         = object_type::tag;

  template <typename Tag> static constexpr auto qt_create_metaobjectdata()
  {
    static constexpr auto classinfo_strings = object_type::template __classInfosStrings<Tag>;
    static constexpr auto strings           = object_type::template __strings<Tag>;
    static constexpr auto signals           = object_type::template __signals<Tag>;
    static constexpr auto slots             = object_type::template __slots<Tag>;
    static constexpr auto invocables        = object_type::template __invocables<Tag>;
    static constexpr auto properties        = object_type::template __properties<Tag>;

    namespace QMC = QtMocConstants;

    // FIXME: we should be able to pass  __strings<Tag>[I]->data() directly to StringRefStorage
    constexpr auto string_wrappers = [&]
    {
      return [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::make_tuple(detail::string_storage_wrapper<strings[I]->size()>{strings[I]->data()}...);
      }(std::make_index_sequence<strings.size()>());
    }();

    auto qt_stringData = [&] constexpr
    {
      return [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return QtMocHelpers::StringRefStorage{std::get<I>(string_wrappers).data...};
        // FIXME: should be:
        // return QtMocHelpers::StringRefStorage{__strings<Tag>[I]->data()...};
      }(std::make_index_sequence<strings.size()>());
    }();

    static constexpr size_t classinfos_size = classinfo_strings.size();
    size_t                  strings_index   = 2 + classinfos_size;

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
            using SignalT = [:type_of(Fn):];
            return ^^SignalT::fn;
          }
        }();
        constexpr auto func_type = []
        {
          if constexpr(is_function(Fn))
          {
            return Fn;
          }
          else
          {
            return type_of(Fn);
          }
        }();
        constexpr auto method_type = type_of(method);
        using RetT                 = [:return_type_of(method_type):];

        constexpr auto parameters = define_static_array(parameters_of(method));

        auto make_parameters_data = [&]<size_t N, typename DataT> constexpr
        {
          auto make_parameter = [&]<meta::info p>
          {
            constexpr auto param_type = type_of(p);
            constexpr auto tid        = []
            {
              constexpr auto static_tid = uint(detail::static_meta_type_id_of(param_type));
              if constexpr(static_tid == uint(detail::custom_type))
              {
                constexpr auto offset = []
                {
                  constexpr auto id = []
                  {
                    constexpr auto DR = remove_const(remove_reference(param_type));
                    return constant_string{display_string_of(DR)
                                           | std::views::filter([](auto c) { return c != ' '; })
                                           | std::ranges::to<std::string>()};
                    //
                    // return meta::static_identifier_wrapper_of<DR>()
                    //     .template remove_if<[](auto c) { return c == ' '; }>();
                  }();
                  template for(constexpr auto ii : std::views::iota(size_t(0), strings.size()))
                  {
                    constexpr auto s = strings[ii];
                    if(*id == *s)
                    {
                      return ii;
                    }
                  }
                  std::unreachable();
                }();
                return static_tid | offset;
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
          }(std::make_index_sequence<N>());
        };

        const auto make_data = [&, string_index = strings_index++]<size_t N>
        {
          constexpr auto can_invoke = [&]
          {
            // NOTE: is_invocable_type dont work for defaulted function arguments (but works for callable types [ie.:
            // signal_decl])
            if constexpr(is_invocable_type(type_of(Fn),
                                           parameters | std::views::take(N) | std::views::transform(meta::type_of)))
            {
              return true;
            }
            else if constexpr(is_function(Fn))
            {
              // as workaround we check if last argument is defaulted
              return has_default_argument(parameters[N]);
            }
            else
            {
              return false;
            }
          }();
          if constexpr(can_invoke)
          {
            using SignatureT = typename[:meta::signature_of<method, N>():];
            using DataT      = QtMocHelpers::FunctionData<SignatureT, Type>;

            uint flags = is_public(Fn)    ? QMC::AccessPublic
                       : is_protected(Fn) ? QMC::AccessProtected
                                          : QMC::AccessPrivate;
            if constexpr(N < parameters.size())
            {
              flags = flags | uint(QMC::MethodCloned);
            }
            return std::make_tuple(DataT( //
                /* nameIndex = */ string_index,
                /* tagIndex = */ 1,
                flags,
                detail::static_meta_type_id_of(^^RetT),
                make_parameters_data.template operator()<N, DataT>()));
          }
          else
          {
            return std::tuple();
          }
        };

        return [&]<size_t... I>(std::index_sequence<I...>)
        {
          return std::tuple_cat(make_data.template operator()<parameters.size() - I>()...);
        }(std::make_index_sequence<parameters.size() + 1>());
      };

      const auto signals_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::tuple_cat(make_data_impl.template operator()<signals[I], QtMocConstants::MethodSignal>()...);
      }(std::make_index_sequence<signals.size()>());

      const auto properties_notification_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::tuple_cat(make_data_impl.template operator()<^^Super::template propertyChanged<properties[I]>,
                                                                 QtMocConstants::MethodSignal>()...);
      }(std::make_index_sequence<properties.size()>());

      const auto slots_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::tuple_cat(make_data_impl.template operator()<slots[I], QtMocConstants::MethodSlot>()...);
      }(std::make_index_sequence<slots.size()>());

      const auto invocables_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return std::tuple_cat(make_data_impl.template operator()<invocables[I], QtMocConstants::MethodMethod>()...);
      }(std::make_index_sequence<invocables.size()>());

      return std::apply([]<typename... Args>(Args... args)
                        { return QtMocHelpers::UintData<Args...>{std::move(args)...}; },
                        std::tuple_cat(signals_data, properties_notification_data, slots_data, invocables_data));
    }();

    size_t property_index = 0;
    size_t notify_index   = [&]
    {
      size_t count = 0;
      template for(constexpr auto s : signals)
      {
        using CurrentSignalT = [:type_of(s):];
        count += CurrentSignalT::defaulted_args_count_ + 1;
      }
      return count;
    }();
    auto qt_properties = [&]
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
        return QtMocHelpers::UintData{make_data_impl.template operator()<properties[I]>()...};
      }(std::make_index_sequence<properties.size()>());
    }();
    QtMocHelpers::UintData   qt_enums{};
    QtMocHelpers::UintData   qt_constructors{};
    QtMocHelpers::ClassInfos qt_classinfo = []
    {
      return []<size_t... I>(std::index_sequence<I...>)
      {
        return QtMocHelpers::ClassInfos<classinfos_size / 2>({std::array<uint, 2>{I * 2 + 1, I * 2 + 2}...});
      }(std::make_index_sequence<classinfos_size / 2>());
    }();
    return QtMocHelpers::metaObjectData<Super, tag>(QMC::MetaObjectFlag{},
                                                    qt_stringData,
                                                    qt_methods,
                                                    qt_properties,
                                                    qt_enums,
                                                    qt_constructors,
                                                    qt_classinfo);
  }

  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectContent = qt_create_metaobjectdata<MetaObjectTagType>();
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectStaticContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.staticData;
  template <typename MetaObjectTagType>
  static constexpr inline auto qt_staticMetaObjectRelocatingContent =
      qt_staticMetaObjectContent<MetaObjectTagType>.relocatingData;

  static inline consteval QMetaObject metaObject()
  {
    return {{QMetaObject::SuperData::link<Parent::staticMetaObject>(),
             qt_staticMetaObjectStaticContent<tag>.stringdata,
             qt_staticMetaObjectStaticContent<tag>.data,
             object_type::qt_static_metacall,
             nullptr,
             qt_staticMetaObjectRelocatingContent<tag>.metaTypes,
             nullptr}};
  }

  static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
  {
    auto* _t = static_cast<Super*>(_o);

    constexpr auto sigs   = Super::template __signals<tag>;
    constexpr auto invocs = Super::template __invocables<tag>;
    constexpr auto slts   = Super::template __slots<tag>;
    constexpr auto props  = Super::template __properties<tag>;

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
      _id -= Super::template __property_count<tag>;

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
    if(_c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      const auto     arg_num      = *reinterpret_cast<int*>(_a[1]);
      auto*          result       = reinterpret_cast<QMetaType*>(_a[0]);
      constexpr auto custom_types = Super::template __custom_types<tag>;

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
      _id -= Super::template __signal_count<tag>;

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
        static constexpr auto s = ^^Super::template propertyChanged<p>;
        using signature_type    = [:meta::signature_of<s, ^^Super>():];
        if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s:], ii + Super::template __signal_count<tag>))
        {
          return;
        }

        static constexpr auto s2 = ^^Super::template propertyChanged<constant{identifier_of(p)}>;
        using signature_type2    = [:meta::signature_of<s2, ^^Super>():];
        if(QtMocHelpers::indexOfMethod<signature_type>(_a, &[:s2:], ii + Super::template __signal_count<tag>))
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

  static inline int qt_metacall(object_type* obj, QMetaObject::Call _c, int _id, void** _a)
  {
    static constexpr auto signal_slot_count = [&]
    {
      size_t count = 0;
      count += Super::template __signal_count<tag>;
      count += Super::template __properties<tag>.size(); /* no args for property notifiers */
      count += Super::template __slot_count<tag>;
      count += Super::template __invocable_count<tag>;
      return count;
    }();

    _id = static_cast<Parent*>(obj)->Parent::qt_metacall(_c, _id, _a);
    if(_id < 0)
      return _id;

    if(_c == QMetaObject::InvokeMetaMethod)
    {
      if(_id <= signal_slot_count)
        Super::qt_static_metacall(obj, _c, _id, _a);
      _id -= signal_slot_count;
    }
    if(_c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      if(_id <= signal_slot_count)
        *reinterpret_cast<QMetaType*>(_a[0]) = QMetaType();
      _id -= signal_slot_count;
    }
    if(_c
       == QMetaObject::ReadProperty
       || _c
       == QMetaObject::WriteProperty
       || _c
       == QMetaObject::ResetProperty
       || _c
       == QMetaObject::BindableProperty
       || _c
       == QMetaObject::RegisterPropertyMetaType)
    {
      Super::qt_static_metacall(obj, _c, _id, _a);
      _id -= 1;
    }
    return _id;
  }
};
} // namespace detail

template <typename Super, typename Parent> void* object<Super, Parent>::qt_metacast(const char* _clname)
{
  if(!_clname)
    return nullptr;
  if(!strcmp(_clname, detail::meta_impl<Super, Parent>::template qt_staticMetaObjectStaticContent<tag>.strings))
    return static_cast<void*>(this);
  return QObject::qt_metacast(_clname);
}
template <typename Super, typename Parent>
int object<Super, Parent>::qt_metacall(QMetaObject::Call _c, int _id, void** _a)
{
  return detail::meta_impl<Super, Parent>::qt_metacall(this, _c, _id, _a);
}

template <typename Super, typename Parent>
void object<Super, Parent>::qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
{
  return detail::meta_impl<Super, Parent>::qt_static_metacall(_o, _c, _id, _a);
}

template <typename Super, typename Parent>
constinit const QMetaObject object<Super, Parent>::staticMetaObject = detail::meta_impl<Super, Parent>::metaObject();

} // namespace reflex::qt