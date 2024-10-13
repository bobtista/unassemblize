/**
 * @file
 *
 * @brief Class to extract relevant symbols from PDB files
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include <string>

namespace util
{
std::string to_utf8(const wchar_t *utf16);
std::string to_utf8(const std::wstring &utf16);
std::wstring to_utf16(const char *utf8);
std::wstring to_utf16(const std::string &utf8);
void remove_characters(std::string &s, const std::string &chars);
std::string get_remove_file_ext(const std::string &file_name);
std::string get_file_path(const std::string &file_path);
std::string get_file_ext(const std::string &file_name);

} // namespace util
