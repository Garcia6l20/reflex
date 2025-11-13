#pragma once

#include <reflex/meta.hpp>

#include <reflex/qt/detail/metatype.hpp>

#undef signals
#undef slots

namespace reflex::qt::detail
{
template <typename Tag, typename Super> struct meta_strings
{
  static constexpr bool is_object = meta::is_subclass_of(^^Super, ^^qt::object);
  static constexpr bool is_gadget = not is_object;

  static constexpr auto signals = [] consteval
  {
    if constexpr(is_gadget)
    {
      return std::span<meta::info>();
    }
    else
    {
      return define_static_array(                                               //
          nonstatic_data_members_of(^^Super, meta::access_context::unchecked()) //
          | std::views::filter([&](auto M)
                               { return meta::is_template_instance_of(type_of(M), ^^detail::signal_decl); }));
    }
  }();

  static constexpr auto signal_count = [] consteval
  {
    size_t count = 0;
    template for(constexpr auto s : signals)
    {
      using CurrentSignalT = [:type_of(s):];
      count += CurrentSignalT::defaulted_args_count_ + 1;
    }
    return count;
  }();

  static constexpr auto slots = [] consteval
  {
    if constexpr(is_gadget)
    {
      return std::span<meta::info>();
    }
    else
    {
      return define_static_array(                        //
          meta::member_functions_annotated_with(^^Super, //
                                                ^^detail::slot,
                                                meta::access_context::unchecked()));
    }
  }();

  static constexpr auto slot_count = [] consteval
  {
    size_t count = 0;
    template for(constexpr auto s : slots)
    {
      constexpr size_t n_overloads = 1 + std::ranges::count_if(parameters_of(s), meta::has_default_argument);
      count += n_overloads;
    }
    return count;
  }();

  static constexpr auto invocables = [] consteval
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::invocable,
                                              meta::access_context::unchecked()));
  }();

  static constexpr auto invocable_count = [] consteval
  {
    size_t count = 0;
    template for(constexpr auto s : invocables)
    {
      constexpr size_t n_overloads = 1 + std::ranges::count_if(parameters_of(s), meta::has_default_argument);
      count += n_overloads;
    }
    return count;
  }();

  static constexpr auto properties = [] consteval
  {
    return define_static_array(                              //
        meta::nonstatic_data_members_annotated_with(^^Super, //
                                                    ^^detail::property,
                                                    meta::access_context::unchecked()));
  }();

  static constexpr auto property_count = properties.size();

  static constexpr auto timer_events = [] consteval
  {
    return define_static_array(                        //
        meta::member_functions_annotated_with(^^Super, //
                                              ^^detail::timer_event,
                                              meta::access_context::unchecked()));
  }();

  static constexpr auto custom_types = [] consteval
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

    for(auto R : signals)
    {
      for(auto A : template_arguments_of(type_of(R)) | std::views::drop(1)) // first arg is ^^Super
      {
        try_push(A);
      }
    }

    for(auto R : properties)
    {
      try_push(type_of(R));
    }

    for(auto R : slots)
    {
      for(auto P : parameters_of(R))
      {
        try_push(type_of(P));
      }
    }

    for(auto R : invocables)
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

  static constexpr auto classinfo_strings = [] consteval
  {
    std::vector<constant_string> strings;

    constexpr auto ci = meta::member_named(^^Super, "ClassInfos", meta::access_context::unchecked());
    if constexpr(ci != meta::null)
    {
      __serializeClassInfos<ci>(strings);
    }

    return define_static_array(strings);
  }();

  static constexpr auto strings = [] consteval
  {
    std::vector<constant_string> strings;

    strings.push_back(identifier_of(^^Super));

    template for(constexpr auto s : classinfo_strings)
    {
      strings.push_back(s);
    }

    strings.push_back(""); // add an empty entry

    template for(constexpr auto s : signals)
    {
      strings.push_back(identifier_of(s));
    }

    if constexpr(is_object)
    {
      template for(constexpr auto p : properties)
      {
        strings.push_back(std::string{identifier_of(p)} + "Changed");
      }
    }

    template for(constexpr auto s : slots)
    {
      strings.push_back(identifier_of(s));
    }

    template for(constexpr auto i : invocables)
    {
      strings.push_back(identifier_of(i));
    }

    template for(constexpr auto p : properties)
    {
      strings.push_back(identifier_of(p));
    }

    template for(constexpr auto t : custom_types)
    {
      auto id =
          display_string_of(t) | std::views::filter([](auto c) { return c != ' '; }) | std::ranges::to<std::string>();
      strings.push_back(id);
    }
    return define_static_array(strings);
  }();

  static constexpr auto create_meta_objectdata()
  {
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
        if constexpr(is_object)
        {
          return std::tuple_cat(make_data_impl.template operator()<signals[I], QtMocConstants::MethodSignal>()...);
        }
        else
        {
          return std::tuple();
        }
      }(std::make_index_sequence<signals.size()>());

      const auto properties_notification_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        if constexpr(is_object)
        {
          return std::tuple_cat(make_data_impl.template operator()<^^Super::template propertyChanged<properties[I]>,
                                                                   QtMocConstants::MethodSignal>()...);
        }
        else
        {
          return std::tuple();
        }
      }(std::make_index_sequence<properties.size()>());

      const auto slots_data = [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        if constexpr(is_object)
        {
          return std::tuple_cat(make_data_impl.template operator()<slots[I], QtMocConstants::MethodSlot>()...);
        }
        else
        {
          return std::tuple();
        }
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
    uint mo_flags = QMC::MetaObjectFlag{};
    if constexpr(is_gadget)
    {
      // gadgets requires this flag
      mo_flags |= QMC::PropertyAccessInStaticMetaCall;
    }
    return QtMocHelpers::metaObjectData<Super, Tag>(mo_flags,
                                                    qt_stringData,
                                                    qt_methods,
                                                    qt_properties,
                                                    qt_enums,
                                                    qt_constructors,
                                                    qt_classinfo);
  }
};
} // namespace reflex::qt::detail
