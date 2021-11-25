/*
 * fast_file_reader.cpp -- a class to quickly read over and over
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

#include "fast_file_reader.h"
#include <fcntl.h> // open()
#include <unistd.h> // read()

/* static */ char FastFileReader::m_buff[FAST_FILE_READER_MAX_FILE_SIZE];

/*
    PERFORMANCE NOTE:
    Please check open_fopen_ifstream_benchmark.cpp to see how this solution (open()+lseek()+full file read)
    compares for speed with other solutions...
*/

bool FastFileReader::open_or_rewind()
{
    // On error, -1 is returned and errno is set to indicate the error.
    m_start_next_line_to_process = NULL;
    m_num_lines = 0;
    if (m_fd != -1) {
        off_t ret = lseek(m_fd, 0, SEEK_SET);
        if (ret == -1)
            return false;
    } else {
        m_fd = open(m_filepath.c_str(), O_RDONLY);
        if (m_fd == -1)
            return false;
    }
    return read_whole_file();
}

bool FastFileReader::read_whole_file()
{
    ssize_t nread = read(m_fd, m_buff, FAST_FILE_READER_MAX_FILE_SIZE);
    if (nread <= 0 || nread >= (ssize_t)FAST_FILE_READER_MAX_FILE_SIZE)
        return false; // we expect a non-zero value less than the "m_buff" size

    m_buff[nread] = '\0'; // add NUL termination
    m_start_next_line_to_process = m_buff;
    m_end_next_line_to_process = NULL;
    return true;
}

const char* FastFileReader::get_next_line()
{
    if (m_start_next_line_to_process == NULL)
        return NULL; // we already reached EOF, wait for rewind operation to reset the cursor
    if (m_end_next_line_to_process != NULL) {
        // this is not the first time this function gets called after open/rewind...
        // so we already know where the last-returned line ends... advance our cursor:
        m_start_next_line_to_process = m_end_next_line_to_process + 1;
        m_num_lines++;
    }

    if (m_start_next_line_to_process >= m_buff + FAST_FILE_READER_MAX_FILE_SIZE) {
        m_start_next_line_to_process = NULL;
        return NULL;
    }

    // find first newline
    m_end_next_line_to_process = strchr(m_start_next_line_to_process, '\n');
    if (m_end_next_line_to_process == NULL) // no more newlines
    {
        m_start_next_line_to_process = NULL;
        return NULL;
    }

    // successfully identified the start/end of the next line to process:
    *m_end_next_line_to_process = '\0'; // replace the newline with NUL terminator
    return m_start_next_line_to_process;
}
