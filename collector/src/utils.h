/*
 * utils.h --  a few reusable C++ utility functions for string/file
 *             manipulation
 * Developer: Francesco Montorsi.
 * (C) Copyright 2018 Francesco Montorsi

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------

#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

class CMonitorOutputFrontend;

//------------------------------------------------------------------------------
// String/File utilities
//------------------------------------------------------------------------------

unsigned int replace_string(std::string& str, const std::string& from, const std::string& to, bool allOccurrences);
std::string to_lower(const std::string& orig_str);
std::string trim_string(const std::string& s);
void strip_spaces(char* s);
bool string2int(const char* s, uint64_t& result);
bool string2double(const char* s, double& result);
bool file_or_dir_exists(const char* filename);
template <typename T> std::string stl_container2string(const T& par, const std::string& delim);
std::vector<std::string> split_string_in_array(const std::string& str, char splitter);
bool split_string_on_first_separator(const std::string& str, char separator, std::string& before, std::string& after);
bool split_label_value(const std::string& str, char separator, std::string& label, uint64_t& value);
bool parse_string_with_multiple_ranges(const std::string& data, std::vector<int>& result);
bool parse_string_with_multiple_ranges(const std::string& data, std::set<int>& result);
bool search_integer(std::string filePath, uint64_t valueToSearch);
bool read_integer(std::string filePath, uint64_t& value);
bool read_two_integers(std::string filePath, uint64_t& value1, uint64_t& value2);
bool read_integers_with_range_validation(
    const std::string& filename, uint64_t lower_limit, uint64_t upper_limit, std::set<uint64_t>& cpus);
void proc_read_numeric_stats_from(
    CMonitorOutputFrontend* pOutput, const char* statname, const std::set<std::string>& allowedStatsNames);
std::string get_hostname();
