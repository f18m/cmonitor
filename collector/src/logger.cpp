/*
 * logger.cpp: routines for logging messages
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

#include "cmonitor.h"

//------------------------------------------------------------------------------
// Logger functions
//------------------------------------------------------------------------------

void CMonitorLoggerUtils::init_error_output_file(const std::string& filenamePrefix)
{
    if (filenamePrefix == "stdout") {
        // open stderr as FILE*:
        if ((m_outputErr = fdopen(STDERR_FILENO, "w")) == 0) {
            perror("opening stderr for write");
            exit(13);
        }
    } else if (filenamePrefix == "none") {
        // avoid opening an error file:
        m_outputErr = nullptr;
    } else {

        m_strErrorFileName = filenamePrefix;
        if (filenamePrefix.size() > 5 && filenamePrefix.substr(filenamePrefix.size() - 5) == ".json")
            m_strErrorFileName = filenamePrefix.substr(0, filenamePrefix.size() - 5) + ".err";
        else
            m_strErrorFileName += ".err";

        // prepare output error file but don't open it yet
        printf("Errors (if any) will be logged into the file '%s'\n", m_strErrorFileName.c_str());

        // however if it already exists, remove it:
        if (file_or_dir_exists(m_strErrorFileName.c_str()))
            unlink(m_strErrorFileName.c_str());
    }

    fflush(NULL);
}

void CMonitorLoggerUtils::LogDebug(const char* line, ...)
{
    char currLogLine[256];

    if (!g_cfg.m_bDebug)
        return;

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    // in debug mode stdout is still open, so we can printf:
    printf("%s", currLogLine);
    size_t lastCh = strlen(currLogLine) - 1;
    if (currLogLine[lastCh] != '\n')
        printf("\n");
}

void CMonitorLoggerUtils::LogError(const char* line, ...)
{
    char currLogLine[256];

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    if (!m_outputErr && !m_strErrorFileName.empty()) {
        // apparently this is the first error happening: time to open the logfile for errors:
        if ((m_outputErr = fopen(m_strErrorFileName.c_str(), "w")) == 0) {
            exit(14);
        }
    }

    if (m_outputErr) {
        // errors always go in their dedicated file
        fprintf(m_outputErr, "ERROR: %s\n", currLogLine);
    }
}

void CMonitorLoggerUtils::LogErrorWithErrno(const char* line, ...)
{
    char currLogLine[256];

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    if (!m_outputErr && !m_strErrorFileName.empty()) {
        // apparently this is the first error happening: time to open the logfile for errors:
        if ((m_outputErr = fopen(m_strErrorFileName.c_str(), "w")) == 0) {
            exit(14);
        }
    }

    if (m_outputErr) {
        // errors always go in their dedicated file
        fprintf(m_outputErr, "ERROR: %s (errno=%d, %s)\n", currLogLine, errno, strerror(errno));
    }
}
