#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/hash.hpp>
#include <reflex/serde/annotations.hpp>

#include <bit>
#include <cstring>
#endif

REFLEX_EXPORT namespace reflex::serde
{
  namespace detail
  {
    // First eight bytes of a name, little-end first, zero padded. Bounded: it
    // never reads past the name, so it is safe on a view into a document buffer
    // that ends at a page boundary.
    constexpr std::uint64_t name_word(std::string_view s) noexcept
    {
      if !consteval
      {
        if constexpr(std::endian::native == std::endian::little)
        {
          std::uint64_t w = 0;
          std::memcpy(&w, s.data(), s.size() < 8 ? s.size() : 8);
          return w;
        }
      }
      std::uint64_t w = 0;
      for(std::size_t i = 0; i < s.size() and i < 8; ++i)
      {
        w |= static_cast<std::uint64_t>(static_cast<unsigned char>(s[i])) << (8 * i);
      }
      return w;
    }

    // Below this many members a straight chain of comparisons wins: the one-off
    // setup a hash or a prefix word needs costs more than the comparisons it
    // saves. Measured on GCC 16.1.1 at -O3, where the chain overtakes both
    // between 20 and 24 members.
    inline constexpr std::size_t wide_object_threshold = 24;
  } // namespace detail

  template <typename T> struct object_visitor;

  template <aggregate_c T> struct object_visitor<T>
  {
    static constexpr auto __access_context = std::meta::access_context::current();

    static consteval auto __members()
    {
      return define_static_array(
          nonstatic_data_members_of(remove_reference(^^T), __access_context));
    }

    // A name past eight bytes does not fit a machine word, which is what
    // decides between the two wide-object strategies.
    static consteval bool __any_long_name()
    {
      for(auto member : nonstatic_data_members_of(remove_reference(^^T), __access_context))
      {
        if(std::string_view{identifier_of(member)}.size() > 8
           or std::string_view{serialized_name(member)}.size() > 8)
        {
          return true;
        }
      }
      return false;
    }

    template <typename Fn, decays_to_c<T> Agg>
    static inline constexpr decltype(auto) operator()(
        [[maybe_unused]] Fn&&             fn,
        [[maybe_unused]] std::string_view key,
        [[maybe_unused]] Agg&&            agg)
    {
      constexpr std::size_t count = __members().size();

      if constexpr(count < detail::wide_object_threshold)
      {
        template for(constexpr auto& member : __members())
        {
          constexpr std::string_view id   = identifier_of(member);
          constexpr std::string_view name = serialized_name(member);
          // A member only carries a second name when it is renamed or cased, so
          // most of the time the two comparisons are the same one done twice.
          if constexpr(id == name)
          {
            if(key == name)
            {
              return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
            }
          }
          else
          {
            if(key == name or key == id)
            {
              return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
            }
          }
        }
      }
      else if constexpr(__any_long_name())
      {
        // Names run past a word, so a full comparison per member is expensive.
        // Reject on length and first word, and compare in full only on a hit.
        const std::uint64_t kw = detail::name_word(key);
        template for(constexpr auto& member : __members())
        {
          constexpr std::string_view id   = identifier_of(member);
          constexpr std::string_view name = serialized_name(member);
          if(key.size() == name.size() and kw == detail::name_word(name) and key == name)
          {
            return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
          }
          if constexpr(id != name)
          {
            if(key.size() == id.size() and kw == detail::name_word(id) and key == id)
            {
              return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
            }
          }
        }
      }
      else
      {
        // Every name fits a word, so a comparison is already cheap and only the
        // number of them hurts. Hash once and reject on an integer.
        const std::size_t kh = reflex::hash_bytes(key.data(), key.size());
        template for(constexpr auto& member : __members())
        {
          constexpr std::string_view id   = identifier_of(member);
          constexpr std::string_view name = serialized_name(member);
          if(kh == reflex::hash_bytes(name.data(), name.size()) and key == name)
          {
            return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
          }
          if constexpr(id != name)
          {
            if(kh == reflex::hash_bytes(id.data(), id.size()) and key == id)
            {
              return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
            }
          }
        }
      }
      throw std::runtime_error("Key not found in object");
    }
  };

  template <typename T>
  concept object_visitable_c = not meta::is_template_instance_of(decay(^^T), ^^std::array)
                           and is_complete_type(^^object_visitor<std::decay_t<T>>);

  template <object_visitable_c T, typename Fn>
  constexpr decltype(auto) object_visit(std::span<std::string_view> keys, T && value, Fn && fn)
  {
    auto first = keys.front();
    auto rest  = keys.subspan(1);
    if(rest.empty())
    {
      return object_visitor<std::decay_t<T>>{}(std::forward<Fn>(fn), first, std::forward<T>(value));
    }
    else
    {
      return object_visitor<std::decay_t<T>>{}(
          [&]<typename N>(N&& nested) {
            using U = std::decay_t<N>;
            if constexpr(object_visitable_c<U>)
            {
              return object_visit(
                  rest, std::forward<decltype(nested)>(nested), std::forward<Fn>(fn));
            }
            else
            {
              return std::forward<Fn>(fn)(std::forward<decltype(value)>(value));
            }
          },
          first, std::forward<T>(value));
    }
  }

  // Visit one member named by the whole key. A dot in the key is part of the
  // name, not a path separator, which is what a caller holding a key read out
  // of a document wants: there a dot is an ordinary character, and treating it
  // as a path lets a document reach into a member it never named.
  template <object_visitable_c T, typename Fn>
  constexpr decltype(auto) object_visit_flat(std::string_view key, T && value, Fn && fn)
  {
    return object_visitor<std::decay_t<T>>{}(std::forward<Fn>(fn), key, std::forward<T>(value));
  }

  // Longest dotted path object_visit will split. Keys reach this function
  // straight from a document, so the segment count is input-controlled and the
  // copy below has to be bounded.
  inline constexpr std::size_t max_key_depth = 32;

  template <object_visitable_c T, typename Fn>
  constexpr decltype(auto) object_visit(std::string_view key, T && value, Fn && fn)
  {
    // Most keys carry no dot at all, and the split machinery below costs about
    // as much as the member scan it feeds. Skip straight to the visitor when
    // there is nothing to split.
    if(key.find('.') == std::string_view::npos)
    {
      return object_visit_flat(key, std::forward<T>(value), std::forward<Fn>(fn));
    }

    const auto to_sv = [](auto&& r) { return std::string_view(r.begin(), r.end()); };
    auto       rng   = key | std::views::split('.') | std::views::transform(to_sv);

    std::array<std::string_view, max_key_depth> keys;
    std::size_t                                 key_count = 0;
    for(auto segment : rng)
    {
      if(key_count == max_key_depth)
      {
        throw std::runtime_error("Object key has more than 32 dotted segments");
      }
      keys[key_count++] = segment;
    }
    if(key_count == 0)
    {
      // empty key, treat as single key with empty string
      keys[0]   = key;
      key_count = 1;
    }
    return object_visit(
        std::span(keys.data(), key_count), std::forward<T>(value), std::forward<Fn>(fn));
  }

} // namespace reflex::serde
