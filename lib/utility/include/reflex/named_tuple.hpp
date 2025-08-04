#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/to_tuple.hpp>
#include <reflex/utility.hpp>

#include <ranges>

namespace reflex
{

template <class T, constant_string Name> struct named_type
{
  static constexpr auto name() -> std::string_view
  {
    return Name;
  }
  using type = T;
};

template <typename Impl> struct named_tuple : Impl
{
  using impl_type = Impl;

  template <constant_string name> static consteval bool has() noexcept
  {
    template for(constexpr auto item :
                 define_static_array(nonstatic_data_members_of(^^Impl, meta::access_context::unchecked())))
    {
      if constexpr(name == identifier_of(item))
      {
        return true;
      }
    }
    return false;
  }

  template <constant_string name> static consteval meta::info _get() noexcept
  {
    template for(constexpr auto item :
                 define_static_array(nonstatic_data_members_of(^^Impl, meta::access_context::unchecked())))
    {
      if constexpr(name.get() == identifier_of(item))
      {
        return item;
      }
    }
    return meta::info{};
  }

  template <constant_string name, typename Self> constexpr decltype(auto) get(this Self& self) noexcept
  {
    if constexpr(has<name>())
    {
      constexpr auto item = std::remove_cvref_t<Self>::template _get<name>();
      return self.[:item:];
    }
    else
    {
      static_assert(always_false<Self>, "not found");
    }
  }

  template <typename T, typename Self> constexpr bool has(this Self& self, std::string_view name)
  {
    template for(constexpr auto item :
                 define_static_array(nonstatic_data_members_of(^^Impl, meta::access_context::unchecked())))
    {
      if(name == identifier_of(item))
      {
        if constexpr(type_of(item) == ^^T)
        {
          return true;
        }
        else
        {
          return false;
        }
      }
    }
    return false;
  }

  template <typename T, typename Self> constexpr auto& get(this Self& self, std::string_view name)
  {
    template for(constexpr auto item :
                 define_static_array(nonstatic_data_members_of(^^Impl, meta::access_context::unchecked())))
    {
      if(name == identifier_of(item))
      {
        if constexpr(type_of(item) == ^^T)
        {
          return self.[:item:];
        }
        else
        {
          throw std::runtime_error("bad type");
        }
      }
    }
    throw std::runtime_error("not found");
  }
};

template <constant_string name, typename NamedTuple>
  requires(template_of(^^NamedTuple) == ^^named_tuple)
constexpr decltype(auto) get(NamedTuple t) noexcept
{
  return t.template get<name>();
}

template <class... Items> consteval auto make_named_tuple_type(std::meta::info type)
{
  std::vector<std::meta::info> members;
  constexpr std::array         items = {(^^Items)...};
  template for(constexpr auto item : items)
  {
    using ItemT = [:item:];
    members.push_back(data_member_spec(dealias(^^typename ItemT::type), {.name = ItemT::name()}));
  }
  return define_aggregate(type, members);
}

template <class... Items, class... Values> consteval auto make_named_tuple(Values... values)
{
  struct tuple_t;
  consteval
  {
    make_named_tuple_type<Items...>(^^tuple_t);
  }

  return named_tuple<tuple_t>{values...};
}

template <constant_string... Names, class... Values> consteval auto make_named_tuple(Values... values)
{
  return make_named_tuple<named_type<Values, Names>...>(values...);
}

template <class Object> consteval auto make_named_object_tuple_type(std::meta::info type)
{
  std::vector<std::meta::info> members;
  constexpr auto               ctx = meta::access_context::current();
  template for(constexpr auto item : define_static_array(nonstatic_data_members_of(^^Object, ctx)))
  {
    members.push_back(data_member_spec(type_of(item), {.name = identifier_of(item)}));
  }
  return define_aggregate(type, members);
}

template <typename Object> constexpr auto make_named_tuple(Object const& obj)
{
  struct tuple_t;
  consteval
  {
    make_named_object_tuple_type<Object>(^^tuple_t);
  }

  named_tuple<tuple_t> tup;

  constexpr auto ctx         = meta::access_context::current();
  constexpr auto obj_members = define_static_array(nonstatic_data_members_of(^^Object, ctx));
  constexpr auto tup_members = define_static_array(nonstatic_data_members_of(^^tuple_t, ctx));
  static_assert(obj_members.size() == tup_members.size());
  template for(constexpr auto ii : std::views::iota(std::size_t(0), obj_members.size()))
  {
    constexpr auto t_mem = tup_members[ii];
    constexpr auto o_mem = obj_members[ii];
    tup.[:t_mem:]        = obj.[:o_mem:];
  }
  return tup;
}

} // namespace reflex