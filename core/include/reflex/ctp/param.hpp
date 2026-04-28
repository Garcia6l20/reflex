#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/ctp/core.hpp>
#endif

REFLEX_EXPORT namespace reflex::ctp {

// The main user-facing interface: ctp::Param<T> is the way to have a constant
// template parameter of type T(~ish).
template <class T>
struct param {
    using type = target<T>;
    type const& value;

    consteval param(T const& v) : value(define_static_object(v)) { }
    template <typename U> requires std::constructible_from<T, U&&>
    consteval param(U && v) : value(define_static_object(T(std::forward<U>(v)))) { }
    constexpr operator type const&() const { return value; }
    constexpr auto get() const -> type const& { return value; }
    constexpr auto operator*() const -> type const& { return value; }
    constexpr auto operator->() const -> type const* { return std::addressof(value); }

    constexpr bool operator==(param const& other) const requires requires { value == other.value; } { return value == other.value; }
    constexpr auto operator<=>(param const& other) const requires requires { value <=> other.value; } { return value <=> other.value; }

    template <typename U>
    constexpr bool operator==(U const& other) const requires requires { value == other; } { return value == other; }

    template <typename U>
    constexpr auto operator<=>(U const& other) const requires requires { value <=> other; } { return value <=> other; }
};

template <class T> requires (is_structural_type(^^T))
struct param<T> {
    using type = T;
    T value;

    consteval param(T const& v) : value(v) { ctp::normalize(value); }
    constexpr operator type const&() const { return value; }
    constexpr auto get() const -> T const& { return value; }
    constexpr auto operator*() const -> T const& { return value; }
    constexpr auto operator->() const -> T const* { return std::addressof(value); }

    constexpr bool operator==(param const& other) const requires requires { value == other.value; } { return value == other.value; }
    constexpr auto operator<=>(param const& other) const requires requires { value <=> other.value; } { return value <=> other.value; }

    template <typename U>
    constexpr bool operator==(U const& other) const requires requires { value == other; } { return value == other; }

    template <typename U>
    constexpr auto operator<=>(U const& other) const requires requires { value <=> other; } { return value <=> other; }
};

template <class T>
param(T) -> param<T>;

}
