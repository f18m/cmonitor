/*
 * cgroups.h -- code for collecting CGROUP statistics
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
// The CMonitorCgroups object
//------------------------------------------------------------------------------

class CMonitorOutputFrontend;
class CMonitorLoggerUtils;

class CMonitorCgroups : public CMonitorAppHelper {
public:
    CMonitorCgroups(CMonitorCollectorAppConfig* pCfg, CMonitorOutputFrontend* pOutput)
        : CMonitorAppHelper(pCfg, pOutput)
    {
    }

    //------------------------------------------------------------------------------
    // CGroup functions
    //------------------------------------------------------------------------------

    void cgroup_init();
    bool cgroup_init_check_for_our_pid();
    void cgroup_config();
    bool cgroup_is_allowed_cpu(int cpu);
    bool cgroup_still_exists();
    void cgroup_proc_memory(const std::set<std::string>& allowedStatsNames);
    void cgroup_proc_cpuacct(double elapsed_sec, bool print);
    void cgroup_proc_tasks(double elapsed_sec, OutputFields output_opts, bool include_threads);
    bool cgroup_collect_pids(std::vector<pid_t>& pids); // utility of cgroup_proc_tasks()

    std::set<uint64_t> get_cgroup_cpus() const { return m_cgroup_cpus; }

private:
    bool cgroup_proc_procsinfo(pid_t pid, bool include_threads, procsinfo_t* pout, OutputFields output_opts);

private:
    //------------------------------------------------------------------------------
    // CGroups variables
    //------------------------------------------------------------------------------
    bool m_bCGroupsFound = false;

    // paths of cgroups for the cgroup to monitor (either our own cgroup or another one):
    std::string m_cgroup_systemd_name;
    std::string m_cgroup_memory_kernel_path;
    std::string m_cgroup_cpuacct_kernel_path;
    std::string m_cgroup_cpuset_kernel_path;

    // limits read from the cgroups that apply to this process:
    uint64_t m_cgroup_memory_limit_bytes = 0;
    std::set<uint64_t> m_cgroup_cpus;
    uint64_t m_cgroup_cpuacct_period_us = 0;
    uint64_t m_cgroup_cpuacct_quota_us = 0;

    //------------------------------------------------------------------------------
    // Process tracking
    //------------------------------------------------------------------------------
    std::map<pid_t, procsinfo_t> m_pid_databases[2];
    unsigned int m_pid_database_current_index = 0; // will be alternatively 0 and 1

    // it's possible, even if unlikely, for 2 PIDs to have identical process score...
    // that's why we use std::multimap instead of a std::map
    std::multimap<uint64_t /* process score */, proc_topper_t> m_topper;
};
