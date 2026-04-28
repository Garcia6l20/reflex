#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/ctp/core.hpp>
#include <reflex/ctp/serialize.hpp>
#endif

REFLEX_EXPORT namespace reflex::ctp {
    template <>
    struct reflect<std::string> {
        using target_type = std::string_view;

        static consteval auto serialize(serializer& ser, std::string const& str) -> void {
            ser.push(std::meta::reflect_constant_string(str));
        }

        static consteval auto deserialize(std::meta::info r) -> std::string_view {
            return std::string_view(extract<char const*>(r), extent(type_of(r)) - 1);
        }
    };

    template <class T>
    struct reflect<std::vector<T>> {
        using target_type = std::span<target<T const> const>;

        static consteval auto serialize(serializer& s, std::vector<T> const& v) -> void {
            s.push(reflect_constant_array(v));
        }

        static consteval auto deserialize(std::meta::info r) -> target_type {
            const auto e = extent(type_of(r));
            if (e == 0) {
                return target_type{};
            } else {
                return std::span(extract<target<T> const*>(r), e);
            }
        }
    };

    template <class T>
    struct reflect<std::optional<T>> {
        using target_type = std::optional<target<T>>;

        static consteval auto serialize(serializer& s, std::optional<T> const& o) -> void {
            if (o) {
                s.push_constant(*o);
            }
        }

        static consteval auto deserialize_constants() -> target_type { return {}; }
        static consteval auto deserialize_constants(target_or_ref<T> const& v) -> target_type {
            return v;
        }
    };

    template <class... Ts>
    struct reflect<std::tuple<Ts...>> {
        using target_type = std::tuple<target_or_ref<Ts>...>;

        static consteval auto serialize(serializer& s, std::tuple<Ts...> const& t) -> void {
            auto& [...elems] = t;
            (s.push_constant_or_object(^^Ts, elems), ...);
        }

        static consteval auto deserialize_constants(target_or_ref<Ts> const&... vs) -> target_type {
            return target_type(vs...);
        }
    };

    template <class... Ts>
    struct reflect<std::variant<Ts...>> {
        using target_type = std::variant<target<Ts>...>;

        static consteval auto serialize(serializer& s, std::variant<Ts...> const& v) -> void {
            s.push_constant(v.index());
            // visit should work, but can't because of LWG4197
            template for (constexpr std::size_t I : std::views::iota(0zu, sizeof...(Ts))) {
                if (I == v.index()) {
                    s.push_constant(std::get<I>(v));
                    return;
                }
            }
        }

        template <std::meta::info I, std::meta::info R>
        static consteval auto deserialize() -> target_type {
            return target_type(std::in_place_index<([:I:])>, [:R:]);
        }
    };

    template <class T>
    struct reflect<std::reference_wrapper<T>> {
        using target_type = std::reference_wrapper<T>;

        static consteval auto serialize(serializer& s, std::reference_wrapper<T> r) -> void {
            s.push_object(r.get());
        }

        static consteval auto deserialize_constants(T& r) -> target_type {
            return r;
        }
    };

    template <>
    struct reflect<std::string_view> {
        using target_type = std::string_view;

        static consteval auto serialize(serializer& s, std::string_view sv) -> void {
            s.push_constant(sv.data());
            s.push_constant(sv.size());
        }

        static consteval auto deserialize_constants(char const* data, std::size_t size) -> std::string_view {
            return std::string_view(data, size);
        }
    };

    template <class T, std::size_t N>
    struct reflect<std::span<T, N>> {
        using target_type = std::span<T, N>;

        static consteval auto serialize(serializer& s, std::span<T, N> sp) -> void {
            s.push_constant(sp.data());
            s.push_constant(sp.size());
        }

        static consteval auto deserialize_constants(T const* data, std::size_t size) -> target_type {
            return target_type(data, size);
        }
    };
}
