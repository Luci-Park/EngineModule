/**
 * @file QueryFilters.hpp
 * @author sumin.park
 * @brief Query filter tags (With, Without, Changed, Added) and their classification trait.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <type_traits>

namespace engine
{
    // type-level markers only; never instantiated, never given members
    template <typename T>
    struct With
    {
    };

    template <typename T>
    struct Without
    {
    };

    template <typename T>
    struct Changed
    {
    };

    template <typename T>
    struct Added
    {
    };

    template <typename T>
    struct IsFilter : std::false_type
    {
    };

    template <typename T>
    struct IsFilter<With<T>> : std::true_type
    {
    };

    template <typename T>
    struct IsFilter<Without<T>> : std::true_type
    {
    };

    template <typename T>
    struct IsFilter<Changed<T>> : std::true_type
    {
    };

    template <typename T>
    struct IsFilter<Added<T>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool IS_FILTER_V = IsFilter<T>::value;
}
