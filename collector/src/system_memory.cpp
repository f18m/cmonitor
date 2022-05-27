/*
 * system_memory.cpp - code for collecting SYSTEM-level memory-statistics (i.e. not cgroup-aware)
 * Developer: Nigel Griffiths.
 * (C) Copyright 2018 Nigel Griffiths

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
#include "system.h"
#include "utils_string.h"

/*
read /proc/meminfo
which has format
    STATNAME: <value> kB
or
    STATNAME: <value>
*/
bool CMonitorSystem::read_meminfo_stats(FastFileReader& reader, const std::set<std::string>& allowedStatsNames,
    CMonitorOutputFrontend* pOutput, numeric_parser_stats_t& out_stats)
{
    /*
        NOTE: this is a specialized variant of FastFileReader::read_numeric_stats()
    */

    size_t nread = 0, ndiscarded = 0;
    if (!reader.open_or_rewind()) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", reader.get_file().c_str());
        return nread;
    }

    pOutput->psection_start("proc_meminfo");

    std::string label;
    uint64_t value = 0;
    const char* pline = reader.get_next_line();
    while (pline) {

        std::string line(pline);
        bool is_kb = false;
        if (line.size() > 3 && line[line.size() - 2] == 'k' && line[line.size() - 1] == 'B') {
            is_kb = true;
            line.resize(line.size() - 3);
        }
        // CMonitorLogger::instance()->LogDebug("read_meminfo_stats: [%s] is_kb=%d", line.c_str(), is_kb);

        std::string value_str;
        if (split_string_on_first_separator(line, ':', label, value_str)) {

            // skip all the spaces after the colon
            unsigned int i = 0;
            for (; i < value_str.size() && isspace(value_str[i]); i++)
                ;

            if (string2int(&value_str[i], value)) {
                // adjust kB -> bytes if needed
                if (is_kb)
                    value *= 1000;

                // apply KPI filter
                if (allowedStatsNames.empty() /* all stats must be put in output */
                    || allowedStatsNames.find(label) != allowedStatsNames.end()) {
                    pOutput->plong(label.c_str(), value);
                    nread++;
                } else
                    ndiscarded++;
            }
        }

        pline = reader.get_next_line();
    }

    pOutput->psection_end();

    CMonitorLogger::instance()->LogDebug(
        "From %s read=%zu discarded=%zu kpis", reader.get_file().c_str(), nread, ndiscarded);

    return nread;
}

void CMonitorSystem::sample_memory(const std::set<std::string>& charted_stats_from_meminfo)
{
    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_MEMORY) == 0)
        return;

    if (m_pOutput->is_prometheus_enabled()) {
        size_t size = sizeof(prometheus_kpi_proc_meminfo) / sizeof(prometheus_kpi_proc_meminfo[0]);
        m_pOutput->init_prometheus_kpi(prometheus_kpi_proc_meminfo, size);
    }

    DEBUGLOG_FUNCTION_START();

    key_value_map_t out;
    numeric_parser_stats_t out_stats;
    read_meminfo_stats(m_meminfo, charted_stats_from_meminfo, m_pOutput, out_stats);

    if (m_pCfg->m_nOutputFields == PF_ALL) {
        key_value_map_t out;
        numeric_parser_stats_t out_stats;
        m_vmstat.read_numeric_stats(std::set<std::string>(), out, out_stats);

        if (m_pOutput->is_prometheus_enabled()) {
            size_t size = sizeof(prometheus_kpi_proc_vmstat) / sizeof(prometheus_kpi_proc_vmstat[0]);
            m_pOutput->init_prometheus_kpi(prometheus_kpi_proc_vmstat, size);
        }

        m_pOutput->psection_start("proc_vmstat");
        for (auto entry : out)
            m_pOutput->plong(entry.first.c_str(), entry.second);
        m_pOutput->psection_end();
    }
}