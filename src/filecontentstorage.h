/**
 * @file
 *
 * @brief Class to cache file contents for frequent access
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
#include <map>
#include <string>
#include <vector>

namespace unassemblize
{
struct TextFileContent
{
    std::string filename;
    std::vector<std::string> lines;
};

struct TextFileContentPair
{
    std::array<const TextFileContent *, 2> pair;
};

class FileContentStorage
{
    using FileContentMap = std::map<std::string, TextFileContent>;

public:
    enum class LoadResult
    {
        Failed,
        Loaded,
        AlreadyLoaded,
    };

public:
    FileContentStorage();

    const TextFileContent *find_content(const std::string &name) const;
    LoadResult load_content(const std::string &name);
    size_t size() const;
    void clear();

private:
    FileContentMap m_filesMap;
    mutable FileContentMap::const_iterator m_lastFileIt;
};

} // namespace unassemblize
