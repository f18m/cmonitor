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

#include "utils.h"
#include "logger.h"
#include "output_frontend.h"
#include <fmt/format.h>
#include <limits.h>
#include <netdb.h>
#include <sstream>
#include <sys/stat.h>

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

bool string2int(const char* s, uint64_t& result)
{
    char* end;
    if (s[0] == '\0' || isspace(s[0]))
        return false;

    /*
    from the manpage:
    Since 0 can legitimately be returned on both success and failure, the calling program should set errno to 0 before
    the call, and then determine if an error occurred by checking whether errno has a nonzero value after the call.
    */
    errno = 0;
    unsigned long l = strtoul(s, &end, 10);
    if (errno != 0)
        return false;

    if (*end != '\0')
        return false;
    result = l;
    return true;
}

bool string2double(const char* s, double& result)
{
    char* end;
    if (s[0] == '\0' || isspace(s[0]))
        return false;

    /*
    from the manpage:
    Since 0 can legitimately be returned on both success and failure, the calling program should set errno to 0 before
    the call, and then determine if an error occurred by checking whether errno has a nonzero value after the call.
    */
    errno = 0;
    double l = strtod(s, &end);
    if (errno != 0)
        return false;

    if (*end != '\0')
        return false;
    result = l;
    return true;
}

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

template std::string stl_container2string(const std::set<uint64_t>& par, const std::string& delim);
template std::string stl_container2string(const std::vector<int>& par, const std::string& delim);

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

bool split_string_on_first_separator(const std::string& str, char separator, std::string& before, std::string& after)
{
    size_t sep_position = str.find(separator);
    if (sep_position == std::string::npos)
        return false;
    before = str.substr(0, sep_position);
    after = str.substr(sep_position + 1);
    return true;
}

bool split_label_value(const std::string& str, char separator, std::string& label, uint64_t& value)
{
    std::string value_str;
    return split_string_on_first_separator(str, separator, label, value_str) && string2int(value_str.c_str(), value);
}

bool parse_string_with_multiple_ranges(const std::string& data, std::vector<uint64_t>& result)
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
            uint64_t res;
            if (!string2int(range[0].c_str(), res))
                return false;

            result.push_back(res);
        } else if (range.size() == 2) {
            uint64_t start;
            if (!string2int(range[0].c_str(), start))
                return false;
            uint64_t stop;
            if (!string2int(range[1].c_str(), stop))
                return false;

            // expand the range in the output vector
            for (uint64_t i = start; i <= stop; i++)
                result.push_back(i);
        } else {
            return false;
        }
    }

    return true;
}

bool parse_string_with_multiple_ranges(const std::string& data, std::set<uint64_t>& result)
{
    std::vector<uint64_t> tmpResult;
    if (!parse_string_with_multiple_ranges(data, tmpResult))
        return false;

    for (uint64_t i : tmpResult)
        result.insert(i);
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

std::string get_hostname()
{
    DEBUGLOG_FUNCTION_START();

    char hostname[1024];
    hostname[1023] = '\0';
    if (gethostname(hostname, sizeof(hostname) - 1) != 0)
        return "unknown-hostname";

    struct hostent* h;
    h = gethostbyname(hostname);
    if (!h)
        return std::string(hostname);

    return std::string(h->h_name);
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

void format_timestamp(const std::chrono::time_point<std::chrono::system_clock>& now_ts, std::string& utcTime)
{
    // and of course the string representation of the wall-clock sample, with millisec accuracy:
    std::time_t sampling_time_in_secs = std::chrono::system_clock::to_time_t(now_ts);
    size_t millisec_since_epoch
        = std::chrono::duration_cast<std::chrono::milliseconds>(now_ts.time_since_epoch()).count() % 1000;

    // IMPORTANT:
#if 0
    utcTime
        = fmt::format("{:%04Y-%02m-%02dT%H:%M:%S}.{:03d}", fmt::gmtime(sampling_time_in_secs), millisec_since_epoch);
    /*
        this syntax is available only since libfmt >= 6.x.y. Ubuntu 18.04 (bionic) ships libfmt 4.x.y so we actually
       rollout our own variation:
    */
#else
    const std::tm* ptm = gmtime(&sampling_time_in_secs);
    utcTime = fmt::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}", // fn
        1900 + ptm->tm_year, ptm->tm_mon + 1, ptm->tm_mday, // fn
        ptm->tm_hour, ptm->tm_min, ptm->tm_sec, millisec_since_epoch);
#endif
}

bool get_timestamp(double* ts_for_delta_computation, std::string& utcTime)
{
    struct timespec tv;
    if (clock_gettime(CLOCK_MONOTONIC, &tv) != 0) {
        *ts_for_delta_computation = 0;
        return false;
    }

    // produce at the same time the timestamp which will be used for DELTA computations...
    *ts_for_delta_computation = (double)tv.tv_sec + (double)tv.tv_nsec * 1.0e-9;

    // ...and the wall-clock timestamp to associate to the new sample of statistics:
    auto now_ts = std::chrono::system_clock::now();

    // and of course the string representation of the wall-clock sample, with millisec accuracy:
    format_timestamp(now_ts, utcTime);
    return true;
}
