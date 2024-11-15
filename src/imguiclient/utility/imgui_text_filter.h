/**
 * @file
 *
 * @brief ImGui utility
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once
#include "imgui_text_filter.h"
#include <imgui.h>
#include <string_view>

struct ImGuiTextFilterEx : public ImGuiTextFilter
{
    bool Draw(const char *key, const char *label = "Filter (inc,-exc)", float width = 0.0f);
    bool PassFilter(std::string_view view) const;
};

namespace unassemblize::gui
{
template<typename Type>
struct TextFilterDescriptor
{
    using FilterType = Type;

    TextFilterDescriptor(const char *key) : key(key) {}

    void reset()
    {
        filtered.clear();
        filteredOnce = false;
    }

    const char *const key;
    ImGuiTextFilterEx filter;
    ImVector<FilterType> filtered;
    bool filteredOnce = false;
};

/*
 * Container: The stable source container.
 * Type: The source container element pointer type.
 * Predicate: The callback for the filter.
 *   Arguments: const ImGuiTextFilterEx &filter, const Container::value_type &value
 *   Return: bool
 */
template<typename Container, typename Type = typename Container::value_type *, typename Predicate>
void UpdateFilter(ImVector<Type> &filtered, const ImGuiTextFilterEx &filter, const Container &source, Predicate condition)
{
    if (filter.IsActive())
    {
        const size_t size = source.size();
        filtered.clear();
        filtered.reserve(size);
        for (size_t i = 0; i < size; ++i)
            if (condition(filter, source[i]))
                filtered.push_back(&source[i]);
    }
    else
    {
        const size_t size = source.size();
        filtered.resize(size);
        for (size_t i = 0; i < size; ++i)
            filtered[i] = &source[i];
    }
}

template<typename Container, typename Descriptor, typename Predicate>
void UpdateFilter(Descriptor &descriptor, const Container &source, Predicate condition)
{
    if (descriptor.filter.Draw(descriptor.key) || !descriptor.filteredOnce)
    {
        UpdateFilter(descriptor.filtered, descriptor.filter, source, condition);
        descriptor.filteredOnce = true;
    }
}

} // namespace unassemblize::gui
