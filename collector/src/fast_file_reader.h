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

// file /proc/stat is pretty large, 8k is not enough, so we use 16k:
#define FAST_FILE_READER_MAX_FILE_SIZE 16384

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
*/
//------------------------------------------------------------------------------

class FastFileReader {
public:
    FastFileReader(const std::string& filepath)
    {
        m_filepath = filepath;
        m_fd = -1;
        m_start_next_line_to_process = NULL;
        m_num_lines = 0;
    }
    ~FastFileReader()
    {
        if (m_fd != -1)
            close(m_fd);
    }

    bool open_or_rewind();

    // returns NULL if EOF is reached
    const char* get_next_line();

private:
    bool read_whole_file();

private:
    std::string m_filepath;
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
