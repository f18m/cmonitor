/*
 * cgroups_memory.cpp -- code for collecting CGROUP MEMORY statistics
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

#include "cgroups.h"
#include "logger.h"
#include "output_frontend.h"
#include "utils.h"
#include <assert.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

// ----------------------------------------------------------------------------------
// CMonitorCgroups - internal helpers
// ----------------------------------------------------------------------------------

size_t CMonitorCgroups::cgroup_proc_memory_dump_flat_keyed(
    const std::string& path, const std::set<std::string>& allowedStatsNames, const std::string& label_prefix)
{
    size_t nread = 0;
    FILE* fp_memory_stats;
    if ((fp_memory_stats = fopen(path.c_str(), "r")) == NULL)
        return nread;

    std::string label;
    uint64_t value = 0;
    while (fgets(m_buff, 1000, fp_memory_stats) != NULL) {

        char* pstart = m_buff;
        if (m_nCGroupsFound == CG_VERSION1)
        {
            if (strncmp(m_buff, "total_", 6) != 0)
                continue; // skip NON-totals: collect only cgroup-total values
            
            // forget about the total_ prefix to make cgroups v1 stat names more similar to those of cgroups v2
            pstart = &m_buff[6];
        }

        size_t len = strlen(pstart);
        if (pstart[len - 1] == '\n')
            pstart[len - 1] = 0;
#if 0 // in both cgroups v1 and cgroups v2 the "memory" controller will never generate 'special' characters
        for (size_t i = 0; i < len; i++) {
            if (m_buff[i] == '(')
                m_buff[i] = '_';
            if (m_buff[i] == ')')
                m_buff[i] = ' ';
            if (m_buff[i] == ':')
                m_buff[i] = ' ';
        }
#endif

        if (split_label_value(pstart, ' ', label, value)) {
            if (allowedStatsNames.empty() /* all stats must be put in output */
                || allowedStatsNames.find(label) != allowedStatsNames.end()) {
                m_pOutput->plong((label_prefix + label).c_str(), value);
                nread++;
            }
        }
    }

    fclose(fp_memory_stats);

    return nread;
}

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCgroups::cgroup_proc_memory(
    const std::set<std::string>& allowedStatsNames_v1, const std::set<std::string>& allowedStatsNames_v2)
{
    uint64_t value;

    if (m_nCGroupsFound == CG_NONE)
        return;

    // See
    //   https://lwn.net/Articles/529927/
    //   https://www.kernel.org/doc/Documentation/cgroup-v1/memory.txt
    //   https://www.kernel.org/doc/Documentation/cgroup-v2.txt

    m_pOutput->psection_start("cgroup_memory_stats");

    if (m_nCGroupsFound == CG_VERSION2)
        // list as first value the main "current" KPI
        if (read_integer(m_cgroup_memory_kernel_path + "/memory.current", value))
            m_pOutput->plong("stat.current", value);

    // dump main memory statistics file
    const std::set<std::string>& allowedStatsNames
        = (m_nCGroupsFound == CG_VERSION1) ? allowedStatsNames_v1 : allowedStatsNames_v2;
    cgroup_proc_memory_dump_flat_keyed(m_cgroup_memory_kernel_path + "/memory.stat", allowedStatsNames, "stat.");

    switch (m_nCGroupsFound) {
    case CG_VERSION1:
        if (read_integer(m_cgroup_memory_kernel_path + "/memory.failcnt", value))
            m_pOutput->plong("failcnt", value);
        break;

    case CG_VERSION2:
        cgroup_proc_memory_dump_flat_keyed(
            m_cgroup_memory_kernel_path + "/memory.events", allowedStatsNames, "events.");
        break;

    case CG_NONE:
        break;
    }

    m_pOutput->psection_end();
}
