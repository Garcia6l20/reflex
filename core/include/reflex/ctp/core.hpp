#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <meta>
#include <ranges>
#endif

REFLEX_EXPORT namespace reflex::ctp {

// The customization point to opt-in to having your type be usable as a
// ctp::Param.
//
// Reflect<T> must provide a target_type, a serialize() with the
// following signature, and one of three forms of deserialization function:
//
//
//  struct {
//      using target_type = ????;
//      static consteval auto serialize(Serializer&, T const&) -> void;
//
//      // Option 1. Take all of the serialized infos as constant template parameters
//      template <std::meta::info... Is>
//      static consteval auto deserialize() -> target_type;
//
//      // Option 2: Take all of the serialized infos as function parameters
//      static consteval auto deserialize(std::meta::info...) -> target_type;
//
//      // Option 3: Take the splices of all of the serialized infos as function
//      // parameters
//      static consteval auto deserialize_constants(auto&&...) -> target_type;
//
//  };

template <class T>
struct reflect;

namespace impl {
    template <class T> using custom_target = reflect<T>::target_type;
}

// The target type for a given T. For structural types, the target is always
// T in order to be consistent with regular C++20 template parameters.
template <class T>
using target = [: std::meta::is_structural_type(^^T) ? ^^T : substitute(^^impl::custom_target, {^^T}) :];

// For those cases where references need to be preserved,
// target_or_ref<T&> is T& but target_or_ref<T> is target<T>
template <class T>
using target_or_ref = [: std::meta::is_lvalue_reference_type(^^T) ? ^^T : substitute(^^target, {^^T}) :];


namespace impl {
    // This is the singular (private) object that will be
    // constructed from the serialization-deserialization round trip.
    template <class T, std::meta::info... Is>
    inline constexpr target<T> the_object = []{
        if constexpr (requires { reflect<T>::template deserialize<Is...>(); }) {
            return reflect<T>::template deserialize<Is...>();
        } else if constexpr (requires { reflect<T>::deserialize(Is...); }) {
            return reflect<T>::deserialize(Is...);
        } else {
            return reflect<T>::deserialize_constants([:Is:]...);
        }
    }();

    // This is the singular (private) object that will be used for reflect_constant_array
    template <class T, std::meta::info... Is>
    inline constexpr target<T> the_array[] = {[:Is:]...};

    // The default/simple approach to serialization, using Serializer
    template <class T>
    consteval auto default_serialize(T const& v) -> std::meta::info;
}

inline constexpr auto normalize = 
    []<class T>([[maybe_unused]] T& v) -> void {
        if constexpr (requires { std::is_string_literal(v); }) {
#ifdef __clang__
            if (char const* root = std::string_literal_from(v)) {
                char const* global = std::define_static_string(std::string_view(root));
                v = global + (v - root);
            }
#elifdef __GNUC__
            // no std::string_literal_from yet on GCC so we define a new object for now
            v = std::define_static_string(std::string_view(v));
#endif
        }
    };

// Extension of std::meta::reflect_constant that is customizable via Reflect<T>.
// For scalar types, returns a reflection representing a value.
// For class types, returns a reflection representing an object.
inline constexpr auto reflect_constant =
    []<class T>(T v){
        if constexpr (is_structural_type(^^T)) {
            normalize(v);
            return std::meta::reflect_constant(v);
        } else {
            // For non-structural types, customize via Reflect<T>::serialize
            return impl::default_serialize(v);
        }
    };

// Extension of std::meta::reflect_constant_array. Returns a reflection representing
// an array of target<T>, where T is the value type of the range.
inline constexpr auto reflect_constant_array =
    []<std::ranges::input_range R>(R&& r){
        std::vector<std::meta::info> elems = {^^std::ranges::range_value_t<R>};
        for (auto&& e : r) {
            elems.push_back(reflect_constant(reflect_constant(e)));
        }
        return substitute(^^impl::the_array, elems);
    };

// Extension of std::define_static_object, except based on ctp::reflect_constant
inline constexpr auto define_static_object =
    []<class T>(T const& v) -> target<T> const& {
        // ctp::reflect_constant gives us a reflection representing an object
        // UNLESS T is a scalar type, in which case we have to do something else
        if constexpr (is_class_type(^^T)) {
            return extract<target<T> const&>(reflect_constant(v));
        } else {
            // should be std::define_static_object but not implemented in clang
            return std::define_static_array(std::span(std::addressof(v), 1))[0];
        }
    };

}
