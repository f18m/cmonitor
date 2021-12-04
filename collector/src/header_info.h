/*
 * header_info.h -- code generating information sampled only at the start, one-shot
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

#include "cmonitor.h"
#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

//------------------------------------------------------------------------------
// The CMonitorHeaderInfo object
//------------------------------------------------------------------------------

class CMonitorHeaderInfo : public CMonitorAppHelper {
public:
    CMonitorHeaderInfo(CMonitorCollectorAppConfig* pCfg, CMonitorOutputFrontend* pOutput)
        : CMonitorAppHelper(pCfg, pOutput)
    {
    }

    //------------------------------------------------------------------------------
    // JSON header functions
    //------------------------------------------------------------------------------

    void header_identity();
    void header_cmonitor_info(
        int argc, char** argv, long sampling_interval_msec, long num_samples, unsigned int collect_flags);
    void header_etc_os_release();
    void header_cpuinfo();
    void header_version();
    void header_lscpu();
    void header_lshw();
    void header_meminfo();
    void header_custom_metadata();

private:
    void file_read_one_stat(const char* file, const char* name);
};