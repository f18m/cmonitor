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
#include "system.h"
#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

// ----------------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------------

#define MIN_ELAPSED_SECS (0.1)
#define MAX_LOGICAL_CPU (256)
#define CGROUP_COLLECTOR_BUFF_SIZE (8192)

enum CGroupDetected {
    CG_NONE, // force newline
    CG_VERSION1, // force newline
    CG_VERSION2 // force newline
};

std::string CGroupDetected2string(CGroupDetected k);

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

/* structure to save CPU utilization as reported by cpuacct cgroup */
typedef struct {
    uint64_t counter_nsec_user_mode;
    uint64_t counter_nsec_sys_mode;
} cpuacct_utilisation_t;

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
        memset(&m_cpuacct_prev_values[0], 0, MAX_LOGICAL_CPU * sizeof(cpuacct_utilisation_t));
        memset(&m_cpuacct_prev_values_for_total_cpu, 0, sizeof(cpuacct_utilisation_t));
    }

    ~CMonitorCgroups()
    {
        if (m_fp_cpuacct_stats)
            fclose(m_fp_cpuacct_stats);
    }

    // main setup
    // NOTE: arguments are used only during unit testing
    void init(const std::string& cgroup_prefix_for_test = "", const std::string& proc_prefix_for_test = "");

    // one-shot configuration info
    void output_config();

    // collect & output cgroup stats
    void sample_cpuacct(double elapsed_sec);
    void sample_memory(
        const std::set<std::string>& allowedStatsNames_v1, const std::set<std::string>& allowedStatsNames_v2);
    void sample_network_interfaces(double elapsed_sec, OutputFields output_opts);
    void sample_processes(double elapsed_sec, OutputFields output_opts, bool include_threads);

    // misc helpers
    bool cgroup_still_exists();
    std::set<uint64_t> get_cgroup_cpus() const { return m_cgroup_cpus; }

private:
    // cgroups config
    bool init_check_for_our_pid();
    void v1_read_limits();
    void v2_read_limits();
    void init_cpuacct();

    // cgroup processes
    bool proc_procsinfo(
        pid_t pid, bool include_threads, procsinfo_t* pout, OutputFields output_opts, bool output_tgid);
    bool collect_pids(const std::string& file, std::vector<pid_t>& pids); // utility of cgroup_proc_tasks()

    // cpuacct and cpuset cgroups
    bool read_cpuacct_line(const std::string& path, std::vector<uint64_t>& valuesINT /* OUT */);
    bool proc_cpuacct_v1_counters_by_cpu(bool print, double elapsed_sec, cpuacct_utilisation_t& total_cpu_usage);
    bool proc_cpuacct_v2_counters(bool print, double elapsed_sec, cpuacct_utilisation_t& total_cpu_usage);
    bool is_allowed_cpu(int cpu);
    bool read_from_system_cpu_for_current_cgroup(std::string kernelPath, std::set<uint64_t>& cpus);

    // memory cgroups
    size_t proc_memory_dump_flat_keyed(
        const std::string& path, const std::set<std::string>& allowedStatsNames, const std::string& label_prefix);

private:
    // main switch that indicates if init() was successful or not
    CGroupDetected m_nCGroupsFound = CG_NONE;

    // paths of cgroups for the cgroup to monitor (either our own cgroup or another one):
    std::string m_cgroup_systemd_name;
    std::string m_cgroup_memory_kernel_path;
    std::string m_cgroup_cpuacct_kernel_path;
    std::string m_cgroup_cpuset_kernel_path;
    std::string m_proc_prefix; // used only during unit testing to insert an arbitrary prefix in front of "/proc"

    // limits read from the cgroups that apply to this process:
    uint64_t m_cgroup_memory_limit_bytes = 0;
    std::set<uint64_t> m_cgroup_cpus;
    uint64_t m_cgroup_cpuacct_period_us = 0;
    uint64_t m_cgroup_cpuacct_quota_us = 0;

    // configuration/status read from "cpuacct" cgroup
    unsigned int m_num_cpus_cpuacct_cgroup = 0;

    // previous values sampled from "cpuacct" cgroup
    cpuacct_utilisation_t m_cpuacct_prev_values[MAX_LOGICAL_CPU];
    cpuacct_utilisation_t m_cpuacct_prev_values_for_total_cpu;

    // previous values for network interfaces inside cgroup
    netinfo_map_t m_previous_netinfo;

    // counters of how many times each cgroup_proc_*() main API has been invoked
    unsigned int m_num_memory_samples_collected = 0;
    unsigned int m_num_cpuacct_samples_collected = 0;
    unsigned int m_num_tasks_samples_collected = 0;
    unsigned int m_num_network_samples_collected = 0;

    // handles to stat files
    FILE* m_fp_cpuacct_stats = nullptr;

    // Process tracking
    std::map<pid_t, procsinfo_t> m_pid_databases[2];
    unsigned int m_pid_database_current_index = 0; // will be alternatively 0 and 1

    // it's possible, even if unlikely, for 2 PIDs to have identical process score...
    // that's why we use std::multimap instead of a std::map
    std::multimap<uint64_t /* process score */, proc_topper_t> m_topper_procs;

    // buffer used for reading stats files or for other processing
    char m_buff[8192];
};
