/*
 * system.h -- code for collecting SYSTEM-level statistics (i.e. not cgroup-aware)
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
// Types
//------------------------------------------------------------------------------

typedef struct {
    uint64_t if_ibytes;
    uint64_t if_ipackets;
    uint64_t if_ierrs;
    uint64_t if_idrop;
    uint64_t if_ififo;
    uint64_t if_iframe;
    uint64_t if_obytes;
    uint64_t if_opackets;
    uint64_t if_oerrs;
    uint64_t if_odrop;
    uint64_t if_ofifo;
    uint64_t if_ocolls;
    uint64_t if_ocarrier;
} netinfo_t;

typedef std::map<std::string /* interface name */, netinfo_t> netinfo_map_t;

//------------------------------------------------------------------------------
// CMonitorSystem
//------------------------------------------------------------------------------

class CMonitorSystem : public CMonitorAppHelper {
public:
    CMonitorSystem(CMonitorCollectorAppConfig* pCfg, CMonitorOutputFrontend* pOutput)
        : CMonitorAppHelper(pCfg, pOutput)
    {
    }

    void set_monitored_cpus(const std::set<uint64_t>& cpus) { m_monitored_cpus = cpus; }

    //------------------------------------------------------------------------------
    // Functions to collect /proc stats (baremetal), invoked by main app
    //------------------------------------------------------------------------------

    void proc_stat(double elapsed, OutputFields output_opts);
    void proc_diskstats(double elapsed, OutputFields output_opts);
    void proc_net_dev(double elapsed, OutputFields output_opts);
    void proc_loadavg();
    void proc_filesystems();
    void proc_uptime();

    //------------------------------------------------------------------------------
    // Utility shared with CMonitorCgroups
    //------------------------------------------------------------------------------

    static bool read_net_dev(
        const std::string& filename, const std::set<std::string>& net_iface_whitelist, netinfo_map_t& out_infos);
    static bool output_net_dev_stats(CMonitorOutputFrontend* m_pOutput, double elapsed_sec,
        const netinfo_map_t& new_stats, const netinfo_map_t& prev_stats, OutputFields output_opts);

private:
    bool is_monitored_cpu(int cpu)
    {
        if (m_monitored_cpus.empty())
            return true; // allowed
        return m_monitored_cpus.find(cpu) != m_monitored_cpus.end();
    }

    int proc_stat_cpu_index(
        const char* cpu_data, double elapsed_sec, OutputFields output_opts, cpu_specs_t* logical_cpu);
    void proc_stat_cpu_total(const char* cpu_data, double elapsed_sec, OutputFields output_opts, cpu_specs_t& total_cpu,
        int max_cpu_count); // utility of proc_stat()

private:
    std::set<uint64_t> m_monitored_cpus;

    std::set<std::string> m_network_interfaces_up;
    netinfo_map_t m_previous_netinfo;
};
