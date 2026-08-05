#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/serde.hpp>
#include <reflex/serde/json.hpp>

#include <reflex/named_arg.hpp>
#include <reflex/parse.hpp>

#include <functional>
#endif

#include <reflex/jinja/value_ops.hpp>

REFLEX_EXPORT namespace reflex::jinja::expr
{
  namespace detail
  {
  struct type_scan_accumulator
  {
    std::vector<std::meta::info> types;
    std::vector<std::meta::info> scanned_aggregates;

    consteval bool append_unique(std::meta::info type)
    {
      if(std::ranges::contains(types, type))
      {
        return false;
      }
      types.push_back(type);
      return true;
    }

    consteval bool mark_aggregate_scanned(std::meta::info agg)
    {
      if(std::ranges::contains(scanned_aggregates, agg))
      {
        return false;
      }
      scanned_aggregates.push_back(agg);
      return true;
    }

    consteval void scan_aggregate(
        std::meta::info           agg,
        std::meta::access_context ctx = std::meta::access_context::current())
    {
      agg = dealias(agg);
      if(!mark_aggregate_scanned(agg))
      {
        return;
      }

      for(auto m : std::meta::nonstatic_data_members_of(agg, ctx))
      {
        scan_type(type_of(m), ctx);
      }
    }

    // collects the types reachable from a single type: unwraps std::optional, descends into
    // sequence and map element types, recurses into aggregates
    consteval void scan_type(
        std::meta::info           t,
        std::meta::access_context ctx = std::meta::access_context::current())
    {
      t = dealias(t);
      // extract value_type from std::optional
      if(meta::is_template_instance_of(t, ^^std::optional))
      {
        t = dealias(template_arguments_of(t)[0]);
      }
      if(meta::eval_concept(
             ^^seq_c, {
                          t}))
      {
        auto value_type = dealias(meta::member_named(t, "value_type"));
        if(append_unique(value_type) and is_aggregate_type(value_type))
        {
          scan_aggregate(value_type, ctx);
        }
        return;
      }

      if(meta::eval_concept(
             ^^map_c, {
                          t}))
      {
        auto key_type = dealias(meta::member_named(t, "key_type"));
        append_unique(key_type);

        auto mapped_type = dealias(meta::member_named(t, "mapped_type"));
        if(append_unique(mapped_type) and is_aggregate_type(mapped_type))
        {
          scan_aggregate(mapped_type, ctx);
        }
        return;
      }

      if(append_unique(t) and is_aggregate_type(t))
      {
        scan_aggregate(t, ctx);
      }
    }
  };

  // Binds by reference when the value type has a reference alternative for T, by value otherwise
  // (scalars, strings, const elements).
  template <typename ValueT, typename T> constexpr ValueT make_bound_value(T&& v)
  {
    if constexpr(std::same_as<std::remove_cvref_t<T>, ValueT>)
    {
      return ValueT{std::forward<T>(v)};
    }
    else if constexpr(std::is_lvalue_reference_v<T&&> and requires { ValueT{std::ref(v)}; })
    {
      return ValueT{std::ref(v)};
    }
    else
    {
      return ValueT{std::forward<T>(v)};
    }
  }

  // Defined in <reflex/jinja/builtins.hpp>. Null when the name is not a builtin.
  template <typename ContextT>
  const typename ContextT::function_type* find_builtin(std::string_view name);
  } // namespace detail

  // scans the types of all nonstatic data members of an aggregate, including nested aggregates and
  // element types of sequences and maps
  consteval auto scan_object_types(
      std::meta::info agg, std::meta::access_context ctx = std::meta::access_context::current())
      -> std::vector<std::meta::info>
  {
    detail::type_scan_accumulator acc;
    acc.scan_aggregate(agg, ctx);
    return acc.types;
  }

  // scans the types reachable from a type bound as a context variable: members for an aggregate,
  // element types for a bound sequence or map
  consteval auto scan_bound_types(
      std::meta::info bound, std::meta::access_context ctx = std::meta::access_context::current())
      -> std::vector<std::meta::info>
  {
    detail::type_scan_accumulator acc;
    if(is_aggregate_type(dealias(bound)))
    {
      acc.scan_aggregate(bound, ctx);
    }
    else
    {
      acc.scan_type(bound, ctx);
    }
    return acc.types;
  }

  struct loop_info
  {
    int                       index0;
    int                       index;
    bool                      first;
    int                       length;
    bool                      last;
    std::optional<loop_info&> parent;
  };

  // A callable registered with the evaluator.
  template <typename... Ts> struct context
  {
    static constexpr auto _mandatory_types = std::array{
        ^^bool, ^^int, ^^double, decay(^^std::string), add_lvalue_reference(^^loop_info)};

    context() = default;

    context(named_arg<Ts>... args)
      requires(sizeof...(Ts) > 0)
    {
      (set(args.name, std::ref(args.value)), ...);
    }

    using value_type    = typename[:[] consteval {
      auto types = std::vector{std::from_range, _mandatory_types};

      template for(constexpr auto t : {^^Ts...})
      {
        if(!std::ranges::contains(types, t))
        {
          types.push_back(t);
          for(auto nt : scan_bound_types(decay(t)))
          {
            if(is_aggregate_type(nt))
            {
              auto ref_type = add_lvalue_reference(nt);
              if(!std::ranges::contains(types, ref_type))
              {
                types.push_back(ref_type);
              }
            }
            else
            {
              if(!std::ranges::contains(types, decay(nt)))
              {
                types.push_back(decay(nt));
              }
            }
          }
        }
      }
      return substitute(^^poly::var, types);
    }():];
    using object_type   = typename value_type::obj_type;
    using array_type    = typename value_type::arr_type;
    using function_type = std::function<value_type(std::span<const value_type>)>;
    // Non-owning, unlike function_type: a scoped function is installed by the renderer around a
    // region of the template and its callable outlives that region on the render stack, so there
    // is nothing to allocate and nothing to own. {% block %} publishes super() this way.
    using scoped_function_type = std::function_ref<value_type(std::span<const value_type>)>;

    template <typename T> static constexpr bool can_hold() noexcept
    {
      return value_type::template can_hold<T>();
    }

    object_type              global_vars;
    std::vector<object_type> local_vars;

    std::unordered_map<std::string, function_type> funcs;
    // Stack, not a map: an inner region shadows an outer one under the same name and restores it.
    std::vector<std::pair<std::string_view, scoped_function_type>> scoped_funcs;

    // value_type overload, for braced initializer lists which cannot deduce T
    context& set(std::string_view name, value_type v)
    {
      global_vars.insert_or_assign(std::string{name}, std::move(v));
      return *this;
    }

    template <typename T> context& set(std::string_view name, T&& v)
    {
      global_vars.insert_or_assign(
          std::string{name}, detail::make_bound_value<value_type>(std::forward<T>(v)));
      return *this;
    }

    // {% set %}: binds into the innermost active scope, so a set inside a {% for %} body dies
    // with the loop instead of outliving the render.
    context& set_scoped(std::string_view name, value_type v)
    {
      auto& scope = local_vars.empty() ? global_vars : local_vars.back();
      scope.insert_or_assign(std::string{name}, std::move(v));
      return *this;
    }

    context& def(std::string name, function_type fn)
    {
      funcs.insert_or_assign(std::move(name), std::move(fn));
      return *this;
    }

    value_type operator()(std::string_view fname, std::span<const value_type> args) const
    {
      // A user-registered name always wins: a builtin must never shadow a def().
      if(auto it =
             std::ranges::find_if(funcs, [fname](auto const& pair) { return pair.first == fname; });
         it != funcs.end())
      {
        return it->second(args);
      }
      // Innermost first, so a nested {% block %} shadows the super() of its enclosing one.
      for(auto const& [name, fn] : scoped_funcs | std::views::reverse)
      {
        if(name == fname)
        {
          return fn(args);
        }
      }
      if(const auto* fn = detail::find_builtin<context>(fname))
      {
        return (*fn)(args);
      }
      throw runtime_error("Unknown function '{}'", fname);
    }

    decltype(auto) operator[](std::string_view name) const noexcept
    {
      value_type result = poly::null;
      visit(name, [&]<typename V>(V&& v) {
        using U = std::decay_t<V>;
        if constexpr(requires { result = v; })
        {
          result = v;
        }
        else if constexpr(can_hold<U&>())
        {
          result = std::ref(v);
        }
        else if constexpr(seq_c<U> and requires(std::ranges::range_value_t<U>& elem) {
                            value_type{std::ref(elem)};
                          })
        {
          result = v
                 | std::views::transform([](auto&& elem) { return std::ref(elem); })
                 | std::ranges::to<array_type>();
        }
        else if constexpr(meta::is_template_instance_of(dealias(^^U), ^^std::optional))
        {
          if(v.has_value())
          {
            using T = std::decay_t<decltype(v.value())>;
            if constexpr(requires { result = v.value(); })
            {
              result = v.value();
            }
            else if constexpr(requires { result = std::ref(v.value()); })
            {
              result = std::ref(v.value());
            }
            else
            {
              throw runtime_error(
                  "Variable '{}' is not readable ({})", name, display_string_of(dealias(^^T)));
            }
          }
          else
          {
            result = poly::null;
          }
        }
        else
        {
          throw runtime_error(
              "Variable '{}' is not readable ({})", name, display_string_of(dealias(^^U)));
        }
      });
      return result;
    }

    void get(std::string_view name, value_type& result) const
    {
      result = operator[](name);
    }

    template <typename T> std::optional<T&> get(std::string_view name) const
    {
      std::optional<T&> result;
      visit(name, [&result]<typename U>(U&& v) {
        if constexpr(requires { result = v; })
        {
          result = v;
        }
        else
        {
          result = std::nullopt;
        }
      });
      return result;
    }

    // value_type& operator[](std::string_view name) noexcept
    // {
    //   return visit(name, [](auto&& v) -> auto& { return v; });
    // }

    void visit(this auto& self, std::string_view dotted_keys, auto&& fn) noexcept
    {
      bool found = false;
      for(auto&& locals : self.local_vars | std::views::reverse)
      {
        serde::object_visit(dotted_keys, locals, [&found, &fn]<typename T>(T&& v) {
          found = true;
          if constexpr(meta::is_template_instance_of(decay(^^T), ^^std::optional))
          {
            if(v.has_value())
            {
              std::forward<decltype(fn)>(fn)(std::forward<T>(v).value());
            }
            else
            {
              std::forward<decltype(fn)>(fn)(value_type{poly::null});
            }
          }
          else
          {
            std::forward<decltype(fn)>(fn)(std::forward<T>(v));
          }
        });
        if(found)
        {
          return;
        }
      }
      serde::object_visit(dotted_keys, self.global_vars, std::forward<decltype(fn)>(fn));
    }

    struct [[nodiscard("local_scope_guard must be used to avoid dangling local variables")]]
    local_scope_guard
    {
      context&    ctx;
      std::size_t index;
      local_scope_guard(context& c) : ctx{c}, index{ctx.local_vars.size()}
      {
        ctx.local_vars.emplace_back();
      }

      decltype(auto) set(std::string_view name, value_type v)
      {
        ctx.local_vars.at(index).insert_or_assign(std::string{name}, std::move(v));
        return *this;
      }

      template <typename T> decltype(auto) set(std::string_view name, T&& v)
      {
        ctx.local_vars.at(index).insert_or_assign(
            std::string{name}, detail::make_bound_value<value_type>(std::forward<T>(v)));
        return *this;
      }

      ~local_scope_guard()
      {
        ctx.local_vars.pop_back();
      }
    };

    local_scope_guard push_locals()
    {
      return local_scope_guard{*this};
    }

    struct [[nodiscard("the scoped function is popped as soon as the guard dies")]]
    scoped_function_guard
    {
      context* ctx;

      explicit scoped_function_guard(context& c) : ctx{&c}
      {}

      scoped_function_guard(const scoped_function_guard&)            = delete;
      scoped_function_guard& operator=(const scoped_function_guard&) = delete;

      ~scoped_function_guard()
      {
        if(ctx != nullptr)
        {
          ctx->scoped_funcs.pop_back();
        }
      }
    };

    // `fn` must outlive the guard: nothing is copied, see scoped_function_type. `name` has to be a
    // string with static storage, it is held as a view.
    scoped_function_guard push_function(std::string_view name, scoped_function_type fn)
    {
      scoped_funcs.emplace_back(name, fn);
      return scoped_function_guard{*this};
    }

    void dump() const
    {
      std::println("Globals:");
      for(const auto& [k, v] : global_vars)
      {
        std::println("  {}: {}", k, v);
      }
      std::println("Locals:");
      for(const auto& [index, locals] : local_vars | std::views::enumerate)
      {
        std::println("  Scope {}@{}:", index, static_cast<const void*>(&locals));
        for(const auto& [k, v] : locals)
        {
          std::println("    {}: {}", k, v);
        }
      }
    }
  };
} // namespace reflex::jinja::expr