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

#include "logger.h"
#include "output_frontend.h"
#include "utils_files.h"
#include "utils_string.h"
#include <fmt/format.h>
#include <limits.h>
#include <netdb.h>
#include <sstream>
#include <sys/stat.h>

// ----------------------------------------------------------------------------------
// Utility functions
// ----------------------------------------------------------------------------------

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
