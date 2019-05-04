/*
 * njmon_utils.cpp - a few reusable C++ utility functions for string/file
 * 					 manipulation
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

#include "njmon.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

// ----------------------------------------------------------------------------------
// C++ Helper functions
// ----------------------------------------------------------------------------------

void strip_spaces(char* s)
{
    char* p;
    int spaced = 1;

    p = s;
    for (p = s; *p != 0; p++) {
        if (*p == ':')
            *p = ' ';
        if (*p != ' ') {
            *s = *p;
            s++;
            spaced = 0;
        } else if (spaced) {
            /* do no thing as this is second space */
        } else {
            *s = *p;
            s++;
            spaced = 1;
        }
    }
    *s = 0;
}

std::string to_lower(const std::string& orig_str)
{
    std::string str(orig_str);
    for (auto& c : str)
        c = tolower(c);
    return str;
}

unsigned int replace_string(std::string& str, const std::string& from, const std::string& to, bool allOccurrences)
{
    unsigned int noccurrences = 0;
    size_t start_pos = str.find(from);

    if (allOccurrences) {
        while (start_pos != std::string::npos) {
            noccurrences++;
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();

            size_t newpos = str.substr(start_pos).find(from);
            if (newpos != std::string::npos)
                start_pos += newpos;
            else
                start_pos = newpos;
        }
    } else {
        if (start_pos == std::string::npos)
            return 0;
        str.replace(start_pos, from.length(), to);
    }

    return noccurrences;
}

// General tool to strip spaces from both ends:
std::string trim_string(const std::string& s)
{
    if (s.empty())
        return s;
    std::string::size_type b = s.find_first_not_of(" \t\r\n");
    std::string::size_type e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos)
        return ""; // No non-spaces
    return std::string(s, b, e - b + 1);
}

bool string2int(const std::string& str, int& result)
{
    std::stringstream ss(str);
    if (!(ss >> result)) {
        return false;
    }
    return true;
}

bool string2int(const std::string& str, uint64_t& result)
{
    std::stringstream ss(str);
    if (!(ss >> result)) {
        return false;
    }
    return true;
}

bool file_exists(const char* filename)
{
    struct stat buffer;
    int exist = stat(filename, &buffer);
    if (exist == 0)
        return true;
    else
        // -1
        return false;
}

template <typename T> std::string stl_container2string(const T& par, const std::string& delim)
{
    // Fails to compile here if the function parameter is not an STL container.
    {
        typename T::const_iterator dummy = par.begin();
        (void)dummy;
    }

    if (par.empty())
        return "";

    std::ostringstream oss;
    for (typename T::const_iterator iter = par.begin(); iter != par.end(); ++iter)
        oss << *iter << delim;

    // remove last appended delimiter
    std::string ret = oss.str();
    if (ret.size() > delim.size()) {
        for (unsigned int i = 0; i < delim.size(); i++)
            ret.pop_back();
    }
    return ret;
}

template std::string stl_container2string(const std::set<int>& par, const std::string& delim);

std::vector<std::string> split_string_in_array(const std::string& str, char splitter)
{
    std::vector<std::string> tokens;
    std::string trimmed = trim_string(str);
    std::stringstream ss(trimmed);
    std::string temp;

    while (getline(ss, temp, splitter)) // split into new "lines" based on character
        tokens.push_back(trim_string(temp));

    if (!trimmed.empty() && trimmed[trimmed.size() - 1] == splitter)
        // in this case we must forcefully push an empty token in returned array
        tokens.push_back("");

    return tokens;
}

bool parse_string_with_multiple_ranges(const std::string& data, std::vector<int>& result)
{
    // here we support strings containing a combination of
    //  - plain numbers written in base 10
    //  - ranges: two numbers separed by "-"
    // separated by commas.
    // IMPORTANT: the output vector will be cleared at startup
    // IMPORTANT: the output vector will NOT be sorted
    // IMPORTANT: ranges A-B specified in the string will be present in the output vector as EXPANDED list

    std::vector<std::string> tokens = split_string_in_array(data, ',');

    result.clear();
    for (std::vector<std::string>::const_iterator it = tokens.begin(), end_it = tokens.end(); it != end_it; ++it) {
        const std::string& token = *it;
        std::vector<std::string> range = split_string_in_array(token, '-');
        if (range.size() == 1) {
            int res;
            if (!string2int(range[0], res))
                return false;

            result.push_back(res);
        } else if (range.size() == 2) {
            int start;
            if (!string2int(range[0], start))
                return false;
            int stop;
            if (!string2int(range[1], stop))
                return false;

            // expand the range in the output vector
            for (int i = start; i <= stop; i++)
                result.push_back(i);
        } else {
            return false;
        }
    }

    return true;
}

bool parse_string_with_multiple_ranges(const std::string& data, std::set<int>& result)
{
    std::vector<int> tmpResult;
    if (!parse_string_with_multiple_ranges(data, tmpResult))
        return false;

    for (int i : tmpResult)
        result.insert(i);
    return true;
}

bool read_integer(std::string filePath, uint64_t& value)
{
    FILE* stream = fopen(filePath.c_str(), "r");
    if (!stream)
        return false; // file does not exist or not readable

    value = 0;
    bool bRet = fscanf(stream, "%lu", &value) == 1;
    fclose(stream);

    return bRet;
}

bool read_integers_with_range_validation(
    const std::string& filename, int lower_limit, int upper_limit, std::set<int>& cpus)
{
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

    std::set<int>::iterator cpuit = cpus.begin();
    while (cpuit != cpus.end()) {
        if (*cpuit >= lower_limit && *cpuit < upper_limit)
            cpuit++; // OK; the CPU index is valid
        else
            cpuit = cpus.erase(cpuit); // INVALID CPU index: remove it
    }

    return true;
}
