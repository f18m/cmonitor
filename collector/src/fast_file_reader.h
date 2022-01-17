/*
 * fast_file_reader.h -- a class to quickly read over and over
                         the same, small file, with the fastest combination
                         of syscalls
 * Developer: Francesco Montorsi.
 * (C) Copyright 2021 Francesco Montorsi

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

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

// file /proc/stat is pretty large, 8k is not enough, so we use 16k:
#define FAST_FILE_READER_MAX_FILE_SIZE 16384

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

typedef std::map<std::string /* KPI name */, uint64_t /* value */> key_value_map_t;

typedef struct numeric_parser_stats_s {
    size_t num_read = 0;
    size_t num_discarded = 0;
} numeric_parser_stats_t;

//------------------------------------------------------------------------------
// The FastFileReader class
// Usage example:
/*
    class MyClass {
        MyClass() : m_reader("/proc/stat") {}
       ...
    private:
       FastFileReader m_reader;
    }

    void MyClass::my_timer_func()
    {
        m_reader.open_or_rewind();

        const char* p = m_reader.get_next_line();
        while (p)
        {
            // process line pointed at by "p"

            p = m_reader.get_next_line();
        }
    }

    void MyClass::my_timer_func2()
    {
        // some more high-level APIs exist:
        uint64_t val;
        m_reader.read_integer(val);
        m_reader.read_numeric_stats(...);
    }
*/
//------------------------------------------------------------------------------

class FastFileReader {
public:
    FastFileReader(const std::string& filepath = "")
    {
        m_filepath = filepath;
        m_fd = -1;
        m_start_next_line_to_process = NULL;
        m_num_lines = 0;
        m_reopen_each_time = false;
    }
    ~FastFileReader() { close(); }

    // configuration API:

    void set_file(const std::string& filepath, bool reopen_each_time = false)
    {
        close(); // in case a previous one had already been opened
        m_filepath = filepath;

        // reopen each time is used during unit testing:
        m_reopen_each_time = reopen_each_time;
    }
    std::string get_file() const { return m_filepath; }

    // actual file READING:

    bool open_or_rewind();
    void close();

    // returns NULL if EOF is reached
    const char* get_next_line();

    // assume the whole file just contains a single integer and parse it
    bool read_integer(uint64_t& value);

    // assume the whole file contains statistics in the format:
    //   STATNAME  <value>
    // and read all those listed in provided whitelist
    bool read_numeric_stats(
        const std::set<std::string>& allowedStatsNames, key_value_map_t& out, numeric_parser_stats_t& out_stats);

private:
    bool read_whole_file();

private:
    std::string m_filepath;
    bool m_reopen_each_time;
    int m_fd;

    // the cache buffer is static because cmonitor_collector is mono-thread so we don't
    // need FastFileReader to be reentrant (since FastFileReader won't be used by signal
    // handlers of cmonitor_collector!)
    // By making this static:
    // * we don't pay the memory price for this big buffer for each instance of FastFileReader
    // * we keep the cache hot on this memory buffer
    static char m_buff[FAST_FILE_READER_MAX_FILE_SIZE];

    // parser status
    char* m_start_next_line_to_process;
    char* m_end_next_line_to_process;
    unsigned int m_num_lines;
};
