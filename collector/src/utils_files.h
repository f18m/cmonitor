/*
 * utils_files.h --  a few reusable C++ utility functions for file manipulation
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

#include <chrono>
#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

class CMonitorOutputFrontend;

//------------------------------------------------------------------------------
// File utilities
//------------------------------------------------------------------------------
bool file_or_dir_exists(const char* filename);
bool search_integer(std::string filePath, uint64_t valueToSearch);
bool read_integer(std::string filePath, uint64_t& value);
bool read_cgroupv2_integer_or_max(std::string filePath, uint64_t& value);
bool read_two_integers(std::string filePath, uint64_t& value1, uint64_t& value2);
bool read_integers_with_range_validation(
    const std::string& filename, uint64_t lower_limit, uint64_t upper_limit, std::set<uint64_t>& cpus);
void proc_read_numeric_stats_from(
    CMonitorOutputFrontend* pOutput, const char* statname, const std::set<std::string>& allowedStatsNames);
