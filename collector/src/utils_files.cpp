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

/*
Reads files in one of the 3 formats supported below:

name number
name: number
name: number kB

*/
void proc_read_numeric_stats_from(
    CMonitorOutputFrontend* pOutput, const char* statname, const std::set<std::string>& allowedStatsNames)
{
    FILE* fp = 0;
    char line[1024];
    char label[512];
    char number[512];
    int i;
    int len;

    DEBUGLOG_FUNCTION_START();
    std::string filename = fmt::format("/proc/{}", statname);
    if ((fp = fopen(filename.c_str(), "r")) == NULL) {
        CMonitorLogger::instance()->LogErrorWithErrno("Failed to open performance file %s", filename.c_str());
        return;
    }

    pOutput->psection_start(fmt::format("proc_{}", statname).c_str());
    while (fgets(line, 1000, fp) != NULL) {
        len = strlen(line);
        bool is_kb = false;
        for (i = 0; i < len; i++) {

            // escape characters that we don't like in JSON output:
            if (line[i] == '(')
                line[i] = '_';
            if (line[i] == ')')
                line[i] = ' ';
            if (line[i] == ':')
                line[i] = ' ';
            if (line[i] == '\n') {
                line[i] = 0;
                if (i > 3 && line[i - 2] == 'k' && line[i - 1] == 'B')
                    is_kb = true;
            }
        }
        sscanf(line, "%s %s", label, number);
        // CMonitorLogger::instance()->LogDebug("read_data_numer(%s) |%s| |%s|=%lld\n", statname, label, numstr,
        // atoll(numstr));

        if (allowedStatsNames.empty() /* all stats must be put in output */
            || allowedStatsNames.find(label) != allowedStatsNames.end()) {
            long long num = atoll(number);
            if (is_kb)
                num *= 1000;

            pOutput->plong(label, num);
        }
    }
    pOutput->psection_end();
    (void)fclose(fp);
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
