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
#include "utils_files.h"
#include "utils_string.h"
#include <assert.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

// ----------------------------------------------------------------------------------
// CMonitorCgroups - internal helpers
// ----------------------------------------------------------------------------------

bool read_integer(FastFileReader& reader, uint64_t& value)
{
    if (!reader.open_or_rewind()) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", reader.get_file().c_str());
        return false; // file does not exist or not readable
    }

    // read a single integer from the file
    value = 0;
    return (sscanf(reader.get_next_line(), "%lu", &value) == 1);
}

size_t CMonitorCgroups::sample_flat_keyed_file(FastFileReader& reader, const std::set<std::string>& allowedStatsNames,
    const std::string& label_prefix, key_value_map_t& out)
{
    size_t nread = 0, ndiscarded = 0;
    if (!reader.open_or_rewind()) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", reader.get_file().c_str());
        return nread;
    }

    std::string label;
    uint64_t value = 0;
    const char* pline = reader.get_next_line();
    while (pline) {

        std::string line;
        if (m_nCGroupsFound == CG_VERSION1) {
            if (strncmp(pline, "total_", 6) != 0) {
                pline = reader.get_next_line();
                continue; // skip NON-totals: collect only cgroup-total values
            }

            // forget about the total_ prefix to make cgroups v1 stat names more similar to those of cgroups v2
            line = std::string(&pline[6]);
        } else
            line = std::string(pline);

        if (line.back() == '\n')
            line.pop_back();

        if (split_label_value(line, ' ', label, value)) {
            // add label prefix before filtering
            label = label_prefix + label;

            // apply KPI filter
            if (allowedStatsNames.empty() /* all stats must be put in output */
                || allowedStatsNames.find(label) != allowedStatsNames.end()) {
                out[label] = value;
                nread++;
            } else
                ndiscarded++;
        }

        pline = reader.get_next_line();
    }

    CMonitorLogger::instance()->LogDebug(
        "For memory controller %s read=%zu discarded=%zu kpis", reader.get_file().c_str(), nread, ndiscarded);

    return nread;
}

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCgroups::init_memory(const std::string& cgroup_prefix_for_test)
{
    // when unit testing, we ask the FastFileReader to actually be not-so-fast and reopen each time the file;
    // that's because during unit testing the actual inode of the statistic file changes on every sample.
    // Of course this does not happen in normal mode
    bool reopen_each_time = !cgroup_prefix_for_test.empty();

    m_cgroup_memory_v1v2_stat.set_file(m_cgroup_memory_kernel_path + "/memory.stat", reopen_each_time);
    if (!m_cgroup_memory_v1v2_stat.open_or_rewind()) {
        m_pCfg->m_nCollectFlags &= ~PK_CGROUP_MEMORY;
        CMonitorLogger::instance()->LogError(
            "Could not read the memory statistics file '%s'. Disabling monitoring of memory cgroup.\n",
            m_cgroup_memory_v1v2_stat.get_file().c_str());
        return;
    }

    switch (m_nCGroupsFound) {
    case CG_VERSION1:
        m_cgroup_memory_v1_failcnt.set_file(m_cgroup_memory_kernel_path + "/memory.failcnt", reopen_each_time);
        // even if reading this file fails later on, we keep monitoring the memory controller
        break;

    case CG_VERSION2:
        m_cgroup_memory_v2_current.set_file(m_cgroup_memory_kernel_path + "/memory.current", reopen_each_time);
        if (!m_cgroup_memory_v2_current.open_or_rewind()) {
            m_pCfg->m_nCollectFlags &= ~PK_CGROUP_MEMORY;
            CMonitorLogger::instance()->LogError(
                "Could not read the memory statistics file '%s'. Disabling monitoring of memory cgroup.\n",
                m_cgroup_memory_v2_current.get_file().c_str());
            return;
        }

        m_cgroup_memory_v2_events.set_file(m_cgroup_memory_kernel_path + "/memory.events", reopen_each_time);
        break;

    case CG_NONE:
        assert(0);
        return;
    }

    CMonitorLogger::instance()->LogDebug("Successfully initialized memory cgroup monitoring.\n");
}

void CMonitorCgroups::sample_memory(
    const std::set<std::string>& allowedStatsNames_v1, const std::set<std::string>& allowedStatsNames_v2)
{
    uint64_t value;

    if (m_nCGroupsFound == CG_NONE)
        return;

    bool print = (m_num_memory_samples_collected > 0);
    m_num_memory_samples_collected++;

    DEBUGLOG_FUNCTION_START();

    // See
    //   https://lwn.net/Articles/529927/
    //   https://www.kernel.org/doc/Documentation/cgroup-v1/memory.txt
    //   https://www.kernel.org/doc/Documentation/cgroup-v2.txt

    m_pOutput->psection_start("cgroup_memory_stats");

    if (m_nCGroupsFound == CG_VERSION2)
        // list as first value the main "current" KPI
        if (read_integer(m_cgroup_memory_v2_current, value))
            m_pOutput->plong("stat.current", value);

    // dump main memory statistics file
    const std::set<std::string>& allowedStatsNames
        = (m_nCGroupsFound == CG_VERSION1) ? allowedStatsNames_v1 : allowedStatsNames_v2;
    key_value_map_t statsValues;

    sample_flat_keyed_file(m_cgroup_memory_v1v2_stat, allowedStatsNames, "stat.", statsValues);
    for (auto entry : statsValues)
        m_pOutput->plong(entry.first.c_str(), entry.second);

    switch (m_nCGroupsFound) {
    case CG_VERSION1:
        if (read_integer(m_cgroup_memory_v1_failcnt, value)) {
            if (print)
                m_pOutput->plong("events.failcnt", value - m_memory_prev_values.v1_failcnt);

            // save new values for next sample:
            m_memory_prev_values.v1_failcnt = value;
        }
        break;

    case CG_VERSION2: {
        key_value_map_t newEventsValues;
        if (sample_flat_keyed_file(m_cgroup_memory_v2_events, allowedStatsNames, "events.", newEventsValues)) {
            if (print) {
                for (auto entry : newEventsValues) {
                    auto prevValue = m_memory_prev_values.v2_events.find(entry.first);
                    if (prevValue != m_memory_prev_values.v2_events.end())
                        m_pOutput->plong(entry.first.c_str(), entry.second - prevValue->second);
                }

                // save new values for next sample:
                m_memory_prev_values.v2_events = newEventsValues;
            }
        }
    } break;

    case CG_NONE:
        break;
    }

    m_pOutput->psection_end();
}
