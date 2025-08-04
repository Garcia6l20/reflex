#pragma once

#include <reflex/meta.hpp>
#include <reflex/to_tuple.hpp>
#include <reflex/utility.hpp>
#include <reflex/views/permutations.hpp>

#include <any>
#include <exception>
#include <expected>
#include <map>
#include <memory>
#include <mutex>
#include <typeindex>
#include <typeinfo>

namespace reflex::di
{
namespace detail
{
enum class dependency_type
{
  invalid,
  unique,
  shared,
};

consteval auto get_dependency_object_type(meta::info type) -> meta::info
{
  auto decayed_type = remove_reference(type);
  if(has_template_arguments(decayed_type))
  {
    if(template_of(decayed_type) == ^^std::shared_ptr or template_of(decayed_type) == ^^std::unique_ptr)
    {
      return dealias(meta::member_named(decayed_type, "element_type"));
    }
    else if(template_of(decayed_type) == ^^std::reference_wrapper)
    {
      return dealias(meta::member_named(decayed_type, "type"));
    }
  }
  else
  {
    return decayed_type;
  }
  std::unreachable();
}

consteval auto get_dependency_type(meta::info type) -> dependency_type
{
  auto decayed_type = remove_reference(type);
  if(has_template_arguments(decayed_type))
  {
    if(template_of(decayed_type) == ^^std::shared_ptr)
    {
      return dependency_type::shared;
    }
    else if(template_of(decayed_type) == ^^std::unique_ptr)
    {
      return dependency_type::unique;
    }
    else if(template_of(decayed_type) == ^^std::reference_wrapper)
    {
      return dependency_type::shared;
    }
    else
    {
      return dependency_type::invalid;
    }
  }
  else
  {
    if(is_lvalue_reference_type(type))
    {
      return dependency_type::shared;
    }
    else
    {
      return dependency_type::unique;
    }
  }
  std::unreachable();
}

template <meta::info NS> consteval auto load_registry_namespace() -> std::vector<meta::info>
{
  std::vector<meta::info> registry;
  template for(constexpr auto m : define_static_array(members_of(NS, meta::access_context::current())))
  {
    constexpr auto value = [:m:];
    registry.push_back(value);
  }
  return registry;
}

template <meta::info NS> struct instance_resolver
{
  consteval bool contains(meta::info tmpl) const
  {
    return std::ranges::find_if(map_, [&](auto const& it) { return it.first == tmpl or it.second == tmpl; }) !=
           map_.end();
  }

  consteval meta::info get(meta::info tmpl)
  {
    if(not is_template(tmpl))
    {
      return tmpl;
    }
    else
    {
      auto it = std::ranges::find_if(map_, [&](auto const& it) { return it.first == tmpl; });
      if(it != map_.end())
      {
        return it->second;
      }
      else
      {
        template for(constexpr auto args_count : std::views::iota(size_t(0), registry_.size()))
        {
          template for(constexpr auto p : define_static_array(registry_ | views::permutations<args_count>))
          {
            auto p2 = define_static_array([&]<size_t... II>(std::index_sequence<II...>)
            { //
              return std::vector<meta::info>({get<p[II]>()...});
            }(std::make_index_sequence<args_count>()));
            if(can_substitute(tmpl, p2))
            {
              auto item = substitute(tmpl, p2);
              map_.push_back(std::make_pair(tmpl, item));
              return item;
            }
          }
        }
      }
    }
    return meta::null;
  }

  template <meta::info tmpl> consteval meta::info get()
  {
    return get(tmpl);
  }

  static constexpr auto registry_ = define_static_array(di::detail::load_registry_namespace<NS>());
  std::vector<std::pair<meta::info, meta::info>> map_;
};

template <meta::info tmpl, meta::info NS> consteval meta::info resolve_template_instance()
{
  instance_resolver<NS> resolver;
  return resolver.template get<tmpl>();
}

template <meta::info obj, meta::info reg_ns> consteval auto dependencies_of() -> std::vector<meta::info>
{
  instance_resolver<reg_ns> resolver;
  auto                      ctx = meta::access_context::current();
  std::vector<meta::info>   deps;
  auto ctors = members_of(resolver.template get<obj>(), ctx) | std::views::filter(meta::is_constructor);
  if(not ctors.empty())
  {
    for(auto p : parameters_of(ctors.front()))
    {
      auto ptype        = type_of(p);
      auto decayed_type = remove_const(remove_reference(ptype));
      if(has_template_arguments(decayed_type))
      {
        if(template_of(decayed_type) == ^^std::shared_ptr or template_of(decayed_type) == ^^std::unique_ptr)
        {
          deps.push_back(decayed_type);
        }
        else if(template_of(decayed_type) == ^^std::reference_wrapper)
        {
          deps.push_back(decayed_type);
        }
        else
        {
          auto resolved = resolver.get(decayed_type);
          if(resolved != meta::null)
          {
            if(is_lvalue_reference_type(ptype))
            {
              deps.push_back(substitute(^^std::reference_wrapper,
                                        {
                                            resolved}));
            }
            else
            {
              deps.push_back(resolved);
            }
          }
          else
          {
            std::unreachable(); // unexpected type
          }
        }
      }
      else
      {
        if(is_lvalue_reference_type(ptype))
        {
          deps.push_back(substitute(^^std::reference_wrapper,
                                    {
                                        remove_reference(ptype)}));
        }
        else
        {
          deps.push_back(ptype);
        }
      }
    }
  }
  return deps;
}

namespace empty_registry
{
}
} // namespace detail

struct config
{
  meta::info registry = ^^detail::empty_registry;
  bool       debug    = false;
};

template <config cfg = {}> class injector
{
public:
  template <typename T> std::shared_ptr<T> make_shared()
  {
    return make_ptr<^^T, ^^std::shared_ptr>();
  }

  template <typename T> std::unique_ptr<T> make_unique()
  {
    return make_ptr<^^T, ^^std::unique_ptr>();
  }

  template <typename T> T make_value()
  {
    if constexpr(cfg.debug)
    {
      std::println("🧬 creating unique (value) instance of {}", display_string_of(^^T));
    }
    return std::apply([this]<typename... Args>(Args&&... args) { return T(std::forward<Args>(args)...); },
                      get_constructor_tuple<T>());
  }

  template <meta::info R> auto make_shared()
  {
    if constexpr(is_template(R))
    {
      return make_template<R>([this]<typename T> { return make_shared<T>(); });
    }
    else
    {
      using T = [:R:];
      return make_shared<T>();
    }
  }

  template <meta::info R> auto make_unique()
  {
    if constexpr(is_template(R))
    {
      return make_template<R>([this]<typename T> { return make_unique<T>(); });
    }
    else
    {
      using T = [:R:];
      return make_unique<T>();
    }
  }

  template <meta::info R> auto make_value()
  {
    if constexpr(is_template(R))
    {
      return make_template<R>([this]<typename T> { return make_value<T>(); });
    }
    else
    {
      using T = [:R:];
      return make_value<T>();
    }
  }

private:
  template <meta::info R, meta::info PtrTmpl> auto make_ptr()
  {
    constexpr auto ptr_type = substitute(PtrTmpl, {R});
    using T                 = [:R:];
    using PtrT              = [:ptr_type:];

    if constexpr(PtrTmpl == ^^std::shared_ptr)
    {
      const std::type_index tid = typeid(T);
      if(not shared_objects_.contains(tid))
      {
        if constexpr(cfg.debug)
        {
          std::println("✨ creating shared instance of {}", display_string_of(R));
        }
        shared_objects_[tid] =
            std::apply([this]<typename... Args>(Args&&... args) { return PtrT(new T(std::forward<Args>(args)...)); },
                       get_constructor_tuple<T>());
      }
      else
      {
        if constexpr(cfg.debug)
        {
          std::println("♻️  reusing shared instance of {}", display_string_of(R));
        }
      }
      return std::any_cast<PtrT>(shared_objects_[tid]);
    }
    else
    {
      if constexpr(cfg.debug)
      {
        std::println("🧬 creating unique instance of {}", display_string_of(R));
      }
      return std::apply([this]<typename... Args>(Args&&... args) { return PtrT(new T(std::forward<Args>(args)...)); },
                        get_constructor_tuple<T>());
    }
  }

  template <meta::info R, typename Make> auto make_template(Make const& make)
  {
    constexpr auto instance_type = detail::resolve_template_instance<R, cfg.registry>();
    if constexpr(instance_type == meta::null)
    {
      static_assert(always_false<decltype(*this)>,
                    "cannot resolve suitable template instance, check registry namespace");
      return;
    }
    else
    {
      if constexpr(cfg.debug)
      {
        std::println("🛠️  using template instance {}", display_string_of(instance_type));
      }
      using T = [:instance_type:];
      return make.template operator()<T>();
    }
  }

  template <meta::info type> static consteval auto get_implementation_type() -> meta::info
  {
    constexpr auto decayed_type = remove_reference(type);
    if constexpr(has_template_arguments(decayed_type))
    {
      if constexpr(template_of(decayed_type) == ^^std::shared_ptr or template_of(decayed_type) == ^^std::unique_ptr)
      {
        return get_implementation_type<meta::member_named(decayed_type, "element_type")>();
      }
      else if constexpr(template_of(decayed_type) == ^^std::reference_wrapper)
      {
        return get_implementation_type<meta::member_named(decayed_type, "type")>();
      }
      // fallthrough
    }

    if constexpr(is_abstract_type(decayed_type))
    {
      constexpr auto reg = define_static_array(detail::load_registry_namespace<cfg.registry>());
      template for(constexpr auto p : reg)
      {
        if constexpr(is_base_of_type(decayed_type, p))
        {
          return p;
        }
      }
      std::unreachable(); // unresolved abstract class
    }
    else
    {
      if constexpr(is_lvalue_reference_type(type))
      {
        return remove_reference(type);
      }
      else
      {
        return type;
      }
    }
  }

  template <typename T> auto get_constructor_tuple()
  {
    constexpr auto tuple_type = meta::tuple_for(detail::dependencies_of<^^T, cfg.registry>());
    using tuple_t             = [:tuple_type:];

    const auto get_arg = [&]<size_t I, typename ArgT>()
    {
      constexpr auto type         = ^^ArgT;
      constexpr auto decayed_type = remove_reference(type);
      constexpr auto value_type   = get_implementation_type<type>();
      using ValueT                = [:value_type:];
      if constexpr(has_template_arguments(decayed_type))
      {
        if constexpr(template_of(decayed_type) == ^^std::shared_ptr)
        {
          return make_shared<ValueT>();
        }
        else if constexpr(template_of(decayed_type) == ^^std::unique_ptr)
        {
          return make_unique<ValueT>();
        }
        else if constexpr(template_of(decayed_type) == ^^std::reference_wrapper)
        {
          return std::ref(*make_shared<ValueT>());
        }
      }
      else
      {
        if constexpr(is_lvalue_reference_type(type))
        {
          return std::ref(*make_shared<ValueT>());
        }
        else
        {
          return make_value<ValueT>();
        }
      }
      throw std::runtime_error(std::format("unhandled type: {}", display_string_of(type)));
    };

    return [&]<size_t... I>(std::index_sequence<I...>)
    {
      return std::make_tuple(get_arg.template operator()<I, std::tuple_element_t<I, tuple_t>>()...);
    }(std::make_index_sequence<std::tuple_size_v<tuple_t>>());
  }

  std::mutex                          mux_;
  std::map<std::type_index, std::any> shared_objects_;
};

} // namespace reflex::di
