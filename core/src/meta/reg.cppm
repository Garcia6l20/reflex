export module reflex.core:tu;

import :const_assert;
import :meta;
import :exception;
import std;

export namespace reflex::meta::reg
{
/** @brief Marks a type or template as a registration provider.
 *
 * A provider is recursively inspected by registry_base. By default the registry
 * scans the provider itself; with subclass_lookup enabled it scans the bases of
 * the provider instead, which is useful for pack-expansion wrappers.
 */
struct type_provider
{
  bool subclass_lookup = false;
};

/** @brief Shared namespace scanner for compile-time type registries.
 *
 * Derived must expose matches(std::meta::info) and return either the reflected
 * type to register or meta::null when the candidate does not match.
 */
template <typename Derived, auto namespace_info> struct registry_base
{
private:
  static consteval std::meta::info get_provider(std::meta::info type)
  {
    if(not is_type(type))
    {
      return null;
    }
    if(not is_class_type(type) and not is_template(type))
    {
      return null;
    }
    if constexpr(not requires { annotations_of(type); })
    {
      return null;
    }

    for(auto reflected_annotation : annotations_of(type))
    {
      if(decay(type_of(reflected_annotation)) == ^^type_provider)
      {
        return constant_of(reflected_annotation);
      }
    }
    return null;
  }

  static consteval void append_unique(std::vector<std::meta::info>& infos, std::meta::info type)
  {
    if(std::ranges::contains(infos, type))
    {
      return;
    }
    infos.push_back(type);
  }

  static consteval void
      append_unique(std::vector<std::meta::info>& infos, std::span<const std::meta::info> types)
  {
    for(auto type : types)
    {
      append_unique(infos, type);
    }
  }

  static consteval std::span<const std::meta::info>
      reflected_types(std::meta::info scope = namespace_info)
  {
    std::vector<std::meta::info> infos;

    for(auto member : members_of(scope, std::meta::access_context::unchecked()))
    {
      if(is_alias_type(member))
      {
        member = dealias(member);
      }

      if(not is_type(member))
      {
        continue;
      }

      auto provider_r = get_provider(member);
      if(provider_r != null)
      {
        const auto& provider = extract<type_provider const&>(provider_r);
        if(provider.subclass_lookup)
        {
          for(auto base : bases_of(member, std::meta::access_context::unchecked()))
          {
            append_unique(infos, reflected_types(type_of(base)));
          }
        }
        else
        {
          append_unique(infos, reflected_types(member));
        }
      }

      member = Derived::matches(member);
      if(member == null)
      {
        continue;
      }

      append_unique(infos, member);
    }

    return std::define_static_array(infos);
  }

public:
  static consteval int size()
  {
    return static_cast<int>(reflected_types().size());
  }

  static consteval auto all()
  {
    return reflected_types();
  }

  static consteval bool contains(std::meta::info i)
  {
    return std::ranges::contains(reflected_types(), i);
  }

  static consteval auto at(std::size_t index)
  {
    return reflected_types()[index];
  }
};

/** @brief Registers every type in a namespace carrying a matching annotation. */
template <auto namespace_info, auto annotation>
struct annotated_type_registry
    : registry_base<annotated_type_registry<namespace_info, annotation>, namespace_info>
{
private:
  static constexpr std::meta::info ann = annotation;

  static consteval bool matches_annotation(std::meta::info reflected_annotation)
  {
    if(is_template(ann))
    {
      return is_template_instance_of(type_of(reflected_annotation), ann);
    }
    if(is_type(ann))
    {
      return decay(type_of(reflected_annotation)) == decay(ann);
    }
    return constant_of(reflected_annotation) == constant_of(ann);
  }

public:
  static consteval auto matches(std::meta::info type)
  {
    for(auto reflected_annotation : annotations_of(type))
    {
      if(matches_annotation(reflected_annotation))
      {
        return type;
      }
    }
    return null;
  }
};

/** @brief Registers types derived from a marker base in a namespace.
 *
 * Plain subclasses register themselves. The nested register_ helper registers
 * the template arguments instead by routing discovery through type providers.
 */
template <auto namespace_info, auto base_type>
struct derived_type_registry
    : registry_base<derived_type_registry<namespace_info, base_type>, namespace_info>
{
private:
  static constexpr std::meta::info base = base_type;

  template <typename T> struct __register_type : typename[:base_type:]
  {
    using type = T;
  };

  template <typename T> struct[[= type_provider{}]] __register_type_wrapper
  {
    using type = __register_type<T>;
  };

public:
  static consteval auto matches(std::meta::info type)
  {
    if(is_class_type(type) and is_subclass_of(type, base, std::meta::access_context::unchecked()))
    {
      if(has_template_arguments(type) and template_of(type) == ^^__register_type)
      {
        return template_arguments_of(type)[0];
      }
      else
      {
        return type;
      }
    }
    return null;
  }

  /** @brief Provider wrapper that registers Ts... through the derived registry. */
  template <typename... Ts>
  struct[[= type_provider{.subclass_lookup = true}]] register_ : __register_type_wrapper<Ts>...
  {};
};

} // namespace reflex::meta::reg