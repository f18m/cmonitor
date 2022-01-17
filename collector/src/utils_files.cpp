/*
 * utils.cpp - a few reusable C++ utility functions for string/file
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

#include "utils_files.h"
#include "logger.h"
#include "output_frontend.h"
#include "utils_string.h"
#include <fmt/format.h>
#include <limits.h>
#include <netdb.h>
#include <sstream>
#include <sys/stat.h>

// ----------------------------------------------------------------------------------
// Utility functions
// ----------------------------------------------------------------------------------

bool file_or_dir_exists(const char* filename)
{
    struct stat buffer;
    int exist = stat(filename, &buffer);
    if (exist == 0)
        return true;
    else
        // -1
        return false;
}

bool read_integer(std::string filePath, uint64_t& value)
{
    FILE* stream = fopen(filePath.c_str(), "r");
    if (!stream) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", filePath.c_str());
        return false; // file does not exist or not readable
    }

    // read a single integer from the file
    value = 0;
    bool bRet = fscanf(stream, "%lu", &value) == 1;
    fclose(stream);

    return bRet;
}

bool read_cgroupv2_integer_or_max(std::string filePath, uint64_t& value)
{
    FILE* stream = fopen(filePath.c_str(), "r");
    if (!stream) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", filePath.c_str());
        return false; // file does not exist or not readable
    }

    // read a single integer from the file
    char buffer[64];
    size_t result = fread(buffer, 1, 64, stream);
    if (result == 0 || result >= 64)
        return false; // failed reading even 1 char
    fclose(stream);

    if (buffer[result - 1] != '\n')
        return false; // IMPORTANT: cgroups v2 use always new-line separated values
    buffer[result - 1] = '\0'; // remove last newline

    if (strcmp(buffer, "max") == 0) {
        // consider operation successful and return UINT64_MAX
        value = UINT64_MAX;
        return true;
    }

    return string2int(buffer, value);
}

bool read_two_integers(std::string filePath, uint64_t& value1, uint64_t& value2)
{
    FILE* stream = fopen(filePath.c_str(), "r");
    if (!stream) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", filePath.c_str());
        return false; // file does not exist or not readable
    }

    // read a single integer from the file
    value1 = value2 = 0;
    bool bRet = fscanf(stream, "%lu %lu", &value1, &value2) == 2;
    fclose(stream);

    return bRet;
}

bool read_integers_with_range_validation(
    const std::string& filename, uint64_t lower_limit, uint64_t upper_limit, std::set<uint64_t>& cpus)
{
    // this function reads integers from a file expressed as
    //  - plain numbers written in base 10
    //  - ranges: two numbers separed by "-"
    // separated by commas.

    FILE* stream = fopen(filename.c_str(), "r");
    if (!stream)
        return false; // file does not exist, try next path

    char buffer[256] = "";
    bool bRet = fscanf(stream, "%255s", buffer) == 1;
    fclose(stream);

    if (!bRet)
        return false;

    if (!parse_string_with_multiple_ranges(buffer, cpus))
        return false; // invalid content format??

    std::set<uint64_t>::iterator cpuit = cpus.begin();
    while (cpuit != cpus.end()) {
        if (*cpuit >= lower_limit && *cpuit < upper_limit)
            cpuit++; // OK; the CPU index is valid
        else
            cpuit = cpus.erase(cpuit); // INVALID CPU index: remove it
    }

    return true;
}

bool search_integer(std::string filePath, uint64_t valueToSearch)
{
    FILE* stream = fopen(filePath.c_str(), "r");
    if (!stream) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", filePath.c_str());
        return false; // file does not exist or not readable
    }

#define MAX_LINE_LEN 1024
    char line[MAX_LINE_LEN];
    uint64_t value;
    while (fgets(line, MAX_LINE_LEN, stream) != NULL) {
        // CMonitorLogger::instance()->LogDebug("Searching for %d into [%s]", valueToSearch, line);
        if (sscanf(line, "%lu", &value) == 1) {
            if (value == valueToSearch)
                return true; // found!
        }
    }
    fclose(stream);

    return false; // not found
}
