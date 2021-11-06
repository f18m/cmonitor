/*
 * logger.h -- routines for logging messages
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

#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------

#define DEBUGLOG_FUNCTION_START()                                                                                      \
    CMonitorLogger::instance()->LogDebug("%s() called at line %d of file %s\n", __func__, __LINE__, __FILE__);

//------------------------------------------------------------------------------
// Logging functions for this app
//------------------------------------------------------------------------------

class CMonitorLogger {
public:

    static CMonitorLogger* instance()
    {
        if (!ms_pInstance)
            ms_pInstance = new CMonitorLogger();
        return ms_pInstance;
    }

    void init_error_output_file(const std::string& filenamePrefix);
    void enable_debug()
    {
        m_bDebugEnabled = true;
    }

    void LogDebug(const char* line, ...) __attribute__((format(printf, 2, 3)));
    void LogError(const char* line, ...) __attribute__((format(printf, 2, 3)));
    void LogErrorWithErrno(const char* line, ...) __attribute__((format(printf, 2, 3)));

private:
    static CMonitorLogger* ms_pInstance;
    std::string m_strErrorFileName;
    bool m_bDebugEnabled = false;

    // output:
    FILE* m_outputErr = nullptr;
};
