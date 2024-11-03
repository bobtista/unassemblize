/**
 * @file
 *
 * @brief Common types
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include <array>
#include <assert.h>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace unassemblize
{
using Address64T = uint64_t;
using Address32T = uint32_t;
using IndexT = uint32_t;

using StringToIndexMapT = std::unordered_map<std::string, IndexT>;

template<typename ElementType, typename SizeType, SizeType Size>
struct SizedArray
{
    static constexpr SizeType MaxSize = Size;

    std::array<ElementType, MaxSize> elements;
    std::remove_const_t<SizeType> size = 0;
};

template<typename TargetType, typename SourceType>
TargetType down_cast(SourceType value)
{
    static_assert(sizeof(TargetType) < sizeof(SourceType), "Expects smaller target type");

    assert(value < (SourceType(1) << sizeof(TargetType) * 8));

    return static_cast<TargetType>(value);
}

} // namespace unassemblize
