#pragma once

#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
// no includes
#endif

#include <reflex/constant.hpp>

REFLEX_EXPORT namespace reflex
{
    template <typename ...Args> struct derive {
        consteval derive(Args const&...) {};
    };

    namespace _derive_detail
    {
        consteval bool has_derive_annotation_for(std::meta::info t, std::meta::info tag)
        {
            const auto derive_annotations =
                meta::annotations_of_with(t, ^^derive) | std::ranges::to<std::vector>();
            if (not is_type(tag))
            {
                tag = type_of(tag);
            }
            tag = decay(tag);
            if(not derive_annotations.empty())
            {
                for (const auto& annotation : derive_annotations)
                {
                    auto at = type_of(annotation);
                    for (const auto& arg : template_arguments_of(at))
                    {
                        if (decay(arg) == tag)
                        {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        consteval bool has_derive_annotation_for(std::meta::info t, auto tag)
        {
            return has_derive_annotation_for(t, ^^decltype(tag));
        }
    }

    template <auto Tag>
    using derive_t = std::remove_cvref_t<decltype(Tag)>;

    template <typename T, auto Tag>
    constexpr bool derives_v = _derive_detail::has_derive_annotation_for(^^T, Tag);

    consteval bool derives(std::meta::info t, auto tag)
    {
        return std::meta::extract<bool>(substitute(^^derives_v, {t, std::meta::reflect_constant(tag)}));
    }

    template <typename T, typename Tag>
    concept derives_c = derives_v<T, Tag{}>;
}