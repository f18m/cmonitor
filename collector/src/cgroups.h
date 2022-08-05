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
#include "fast_file_reader.h"
#include "system.h"
#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#ifdef PROMETHEUS_SUPPORT
#include "prometheus_kpi.h"
#endif

// ----------------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------------

#define MIN_ELAPSED_SECS (0.1)
#define MAX_LOGICAL_CPU (256)
#define CGROUP_COLLECTOR_BUFF_SIZE (8192)

enum CGroupDetected {
    CG_NONE = 0, // force newline
    CG_VERSION1 = 1, // force newline
    CG_VERSION2 = 2 // force newline
};

std::string CGroupDetected2string(CGroupDetected k);

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

#ifdef PROMETHEUS_SUPPORT
/* structure for prometheus output : CPU utilization as reported by cpuacct cgroup */
static const prometheus_kpi_descriptor g_prometheus_kpi_cgroup_cpu[] = {
    // cgroup : cpu
    { "cgroup_cpuacct_stats_user", prometheus::MetricType::Gauge, "CPU time consumed by tasks in user mode" },
    { "cgroup_cpuacct_stats_sys", prometheus::MetricType::Gauge,
        "CPU time consumed by tasks in system (kernel) mode" },
    { "cgroup_cpuacct_stats_nr_periods", prometheus::MetricType::Gauge,
        "Number of periods that any thread in the cgroup was runnable" },
    { "cgroup_cpuacct_stats_nr_throttled", prometheus::MetricType::Gauge,
        "Number of runnable periods in which the application used its entire quota and was throttled" },
    { "cgroup_cpuacct_stats_throttled_time", prometheus::MetricType::Gauge,
        "Sum total amount of time individual threads within the cgroup were throttled" },
};

/* structure for prometheus output : Memory utilization as reported by cpuacct cgroup */
static const prometheus_kpi_descriptor g_prometheus_kpi_cgroup_memory[] = {
    // cgroup : memory
    { "cgroup_memory_stats_stat_active_anon", prometheus::MetricType::Counter,
        "Number of bytes of anonymous and swap cache memory on active LRU list" },
    { "cgroup_memory_stats_stat_inactive_anon", prometheus::MetricType::Counter,
        "Number of bytes of anonymous and swap cache memory on inactive LRU list" },
    { "cgroup_memory_stats_stat_active_file", prometheus::MetricType::Counter,
        "Number of bytes of file-backed memory on active LRU list" },
    { "cgroup_memory_stats_stat_inactive_file", prometheus::MetricType::Counter,
        "Number of bytes of file-backed memory on inactive LRU list" },
    { "cgroup_memory_stats_stat_cache", prometheus::MetricType::Counter, "Number of bytes of page cache memory" },
    { "cgroup_memory_stats_stat_mapped_file", prometheus::MetricType::Counter,
        "Number of bytes of mapped file (includes tmpfs/shmem)" },
    { "cgroup_memory_stats_stat_pgfault", prometheus::MetricType::Counter, "Total number of page faults incurred" },
    { "cgroup_memory_stats_stat_pgmajfault", prometheus::MetricType::Counter, "Number of major page faults incurred" },
    { "cgroup_memory_stats_stat_pgpgin", prometheus::MetricType::Counter,
        "Number of charging events to the memory cgroup" },
    { "cgroup_memory_stats_stat_pgpgout", prometheus::MetricType::Counter,
        "Number of uncharging events to the memory cgroup" },
    { "cgroup_memory_stats_stat_rss", prometheus::MetricType::Counter,
        "Number of bytes of anonymous and swap cache memory (includes transparent hugepages)" },
    { "cgroup_memory_stats_stat_rss_huge", prometheus::MetricType::Counter,
        "Number of bytes of anonymous transparent hugepages" },
    { "cgroup_memory_stats_stat_swap", prometheus::MetricType::Counter, "Number of bytes of swap usage" },
    { "cgroup_memory_stats_stat_unevictable", prometheus::MetricType::Counter,
        "Number of bytes of memory that cannot be reclaimed (mlocked etc)" },
    { "cgroup_memory_stats_events_failcnt", prometheus::MetricType::Counter,
        "Number of times that a usage counter hit its limit" },
};

/* structure for prometheus output : Network utilization as reported by BY-NETWORK-INTERFACE */
static const prometheus_kpi_descriptor g_prometheus_kpi_cgroup_network[] = {
    // cgroup : network
    { "cgroup_network_ibytes", prometheus::MetricType::Gauge,
        "Total number of bytes of data received by the interface" },
    { "cgroup_network_ipackets", prometheus::MetricType::Gauge,
        "Total number of packets of data received by the interface" },
    { "cgroup_network_ierrs", prometheus::MetricType::Gauge,
        "Total number of receive errors detected by the device driver" },
    { "cgroup_network_idrop", prometheus::MetricType::Gauge, "Total number of packets dropped by the device driver" },
    { "cgroup_network_ififo", prometheus::MetricType::Gauge, "Total number of FIFO buffer errors" },
    { "cgroup_network_iframe", prometheus::MetricType::Gauge, "Total number of packet framing errors" },
    { "cgroup_network_obytes", prometheus::MetricType::Gauge,
        "Total number of bytes of data transmitted by the interface" },
    { "cgroup_network_opackets", prometheus::MetricType::Gauge,
        "Total number of packets of data transmitted by the interface" },
    { "cgroup_network_oerrs", prometheus::MetricType::Gauge,
        "Total number of transmitted errors detected by the device driver" },
    { "cgroup_network_odrop", prometheus::MetricType::Gauge, "Total number of packets dropped by the interface" },
    { "cgroup_network_ofifo", prometheus::MetricType::Gauge, "Total number of FIFO buffer errors" },
    { "cgroup_network_ocolls", prometheus::MetricType::Gauge,
        "Total number of collisions detected on the interface" },
    { "cgroup_network_ocarrier", prometheus::MetricType::Gauge,
        "Total number of carrier losses detected by the device driver" },

};

/* structure for prometheus output : PROCESS/THREAD statistics */
static const prometheus_kpi_descriptor g_prometheus_kpi_cgroup_processes[] = {
    // cgroup : task/threads
    { "cgroup_tasks_last", prometheus::MetricType::Gauge, "CPU number last executed on" },
    { "cgroup_tasks_usr", prometheus::MetricType::Gauge,
        "Amount of time that this process has been scheduled in user mode, measured in clock ticks" },
    { "cgroup_tasks_sys", prometheus::MetricType::Gauge,
        "Amount of time that this process has been scheduled in kernel mode, measured in clock ticks" },
    { "cgroup_tasks_usr_total_secs", prometheus::MetricType::Counter,
        "Total amount of time that this process has been scheduled in kernel mode, measured in clock ticks" },
    { "cgroup_tasks_sys_total_secs", prometheus::MetricType::Counter,
        "Total amount of time that this process has been scheduled in kernel mode, measured in clock ticks" },
    { "cgroup_tasks_size_kb", prometheus::MetricType::Counter, "Total program size" },
    { "cgroup_tasks_resident_kb", prometheus::MetricType::Counter, "Resident set size" },
    { "cgroup_tasks_restext_kb", prometheus::MetricType::Counter, "Resident text" },
    { "cgroup_tasks_resdata_kb", prometheus::MetricType::Counter, "Resident data" },
    { "cgroup_tasks_share_kb", prometheus::MetricType::Counter, "Shared pages" },
    { "cgroup_tasks_rss_limit_bytes", prometheus::MetricType::Counter,
        "Current soft limit in bytes on the rss of the process" },
    { "cgroup_tasks_minor_fault", prometheus::MetricType::Gauge,
        "The number of minor faults the process has made which have not required loading a memory page from disk" },
    { "cgroup_tasks_major_fault", prometheus::MetricType::Gauge,
        "The number of major faults the process has made which have required loading a memory page from disk" },
    { "cgroup_tasks_virtual_bytes", prometheus::MetricType::Counter, "Virtual memory size in bytes" },
    { "cgroup_tasks_rss_bytes", prometheus::MetricType::Counter,
        "Resident Set Size: number of pages the process has in real memory" },
    { "cgroup_tasks_swap_pages", prometheus::MetricType::Counter, "Number of pages swapped " },
    { "cgroup_tasks_child_swap_pages", prometheus::MetricType::Counter, "Cumulative nswap for child processes" },
    { "cgroup_tasks_realtime_priority", prometheus::MetricType::Counter, "Real-time scheduling priority" },
    { "cgroup_tasks_sched_policy", prometheus::MetricType::Counter, "Scheduling policy" },
    { "cgroup_tasks_delayacct_blkio_secs", prometheus::MetricType::Counter,
        "Aggregated block I/O delays, measured in clock ticks" },
    { "cgroup_tasks_rchar", prometheus::MetricType::Gauge,
        "The number of bytes which this task has caused to be read from storage" },
    { "cgroup_tasks_wchar", prometheus::MetricType::Gauge,
        "The number of bytes which this task has caused, or shall cause to be written to disk" },
    { "cgroup_tasks_read_bytes", prometheus::MetricType::Gauge, "Bytes read" },
    { "cgroup_tasks_write_bytes", prometheus::MetricType::Gauge, "Bytes written" },
    { "cgroup_tasks_total_read", prometheus::MetricType::Counter, "Total bytes read" },
    { "cgroup_tasks_total_write", prometheus::MetricType::Counter, "Total bytes written" },
};
#endif

/* structure to save CPU utilization as reported by cpuacct cgroup */
typedef struct {
    uint64_t counter_nsec_user_mode;
    uint64_t counter_nsec_sys_mode;
} cpuacct_utilisation_t;

typedef struct {
    uint64_t nr_periods;
    uint64_t nr_throttled;
    uint64_t throttled_time_nsec;
} cpuacct_throttling_t;

typedef std::map<std::string /* controller type */, std::string /* path */> cgroup_paths_map_t;

typedef struct {
    uint64_t v1_failcnt;
    key_value_map_t v2_events;
} memory_events_t;

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
        memset(&m_cpuacct_prev_values_for_throttling, 0, sizeof(cpuacct_throttling_t));
        m_memory_prev_values.v1_failcnt = 0;
    }

    ~CMonitorCgroups() {}

    // main setup
    // NOTE: arguments _for_test are used only during unit testing
    void init(bool include_threads, // force newline
        const std::string& cgroup_prefix_for_test = "", // force newline
        const std::string& proc_prefix_for_test = "", // force newline
        uint64_t my_own_pid_for_test = UINT64_MAX);
    void get_list_monitored_files(std::set<std::string>& list);

    // one-shot configuration info
    void output_config();

    // collect & output cgroup stats
    void sample_cpuacct(double elapsed_sec);
    void sample_memory(
        const std::set<std::string>& allowedStatsNames_v1, const std::set<std::string>& allowedStatsNames_v2);

    void sample_process_list(); // call before sample_network_interfaces() and sample_processes()
    void sample_network_interfaces(double elapsed_sec, OutputFields output_opts);
    void sample_processes(double elapsed_sec, OutputFields output_opts);

    // misc helpers
    bool cgroup_still_exists();
    std::set<uint64_t> get_cgroup_cpus() const { return m_cgroup_cpus; }
    CGroupDetected get_detected_cgroup_version() const { return m_nCGroupsFound; }

private:
    // cgroups config
    bool get_cgroup_paths_for_this_pid(cgroup_paths_map_t& cgroup_pathsOUT);
    bool are_cgroups_v2_enabled(std::string& cgroup_pathOUT);
    bool get_cgroup_v1_abs_path_prefix_for_this_pid(const std::string& cgroup_type, std::string& cgroup_pathOUT);
    bool detect_cgroup_ver_and_paths_from_myself(
        const std::string& cgroup_prefix_for_test, uint64_t my_own_pid_for_test);
    bool detect_my_own_cgroup();
    bool detect_user_provided_cgroup();
    bool search_my_pid_in_cgroups(); // sets m_cgroup_processes_path
    bool search_processes_cgroup_path(); // sets m_cgroup_processes_path
    void v1_read_limits();
    void v2_read_limits();
    void init_cpuacct(const std::string& cgroup_prefix_for_test);
    void init_memory(const std::string& cgroup_prefix_for_test);
    void init_network(const std::string& cgroup_prefix_for_test);
    void init_processes(const std::string& cgroup_prefix_for_test);

    // cgroup processes
    bool get_process_infos(
        pid_t pid, bool include_threads, procsinfo_t* pout, OutputFields output_opts, bool output_tgid);
    bool collect_pids(const std::string& file, std::vector<pid_t>& pids); // utility of cgroup_proc_tasks()
    bool collect_pids(FastFileReader& reader, std::vector<pid_t>& pids); // utility of cgroup_proc_tasks()

    // cpuacct controller
    bool read_cpuacct_line(FastFileReader& reader, std::vector<uint64_t>& valuesINT /* OUT */);
    bool sample_cpuacct_v1_counters_by_cpu(bool print, double elapsed_sec, cpuacct_utilisation_t& total_cpu_usage);
    bool sample_cpuacct_v2_counters(bool print, double elapsed_sec, cpuacct_utilisation_t& total_cpu_usage);

    // cpuset controller
    bool is_allowed_cpu(int cpu);
    bool read_cpuset_cpus(std::string kernelPath, std::set<uint64_t>& cpus);

    // memory controller
    size_t sample_flat_keyed_file(FastFileReader& reader, const std::set<std::string>& allowedStatsNames,
        const std::string& label_prefix, key_value_map_t& out);

private:
    // main switch that indicates if init() was successful or not
    CGroupDetected m_nCGroupsFound = CG_NONE;
    pid_t m_my_pid = 0;

    //------------------------------------------------------------------------------
    // paths of cgroups controllers to monitor (either our own cgroup or another one):
    //------------------------------------------------------------------------------
    std::string m_cgroup_systemd_name; // contains the "name" of the cgroup
    std::string m_cgroup_memory_kernel_path; // contains the abs path to the folder with memory controller files
    std::string m_cgroup_cpuacct_kernel_path; // contains the abs path to the folder with cpuacct controller files
    std::string m_cgroup_cpuset_kernel_path; // contains the abs path to the folder with cpuset controller files
    std::string m_cgroup_processes_path; // contains the abs path to the folder which contains either the "tasks"
                                         // (v1) or "cgroups.procs|threads" (v2) files
    std::string m_proc_prefix; // used only during unit testing to insert an arbitrary prefix in front of "/proc"
    std::string m_proc_self_cgroup; // defaults to "/proc/self/cgroup" but is changed during unit testing
    std::string m_proc_self_mounts; // defaults to "/proc/self/mounts" but is changed during unit testing

    //------------------------------------------------------------------------------
    // counters of how many times each cgroup_proc_*() main API has been invoked
    //------------------------------------------------------------------------------
    unsigned int m_num_memory_samples_collected = 0;
    unsigned int m_num_cpuacct_samples_collected = 0;
    unsigned int m_num_tasks_samples_collected = 0;
    unsigned int m_num_network_samples_collected = 0;

    //------------------------------------------------------------------------------
    // limits read from the cgroups controllers:
    //------------------------------------------------------------------------------
    uint64_t m_cgroup_memory_limit_bytes = 0; // if UINT64_MAX indicates no memory limit is present
    std::set<uint64_t> m_cgroup_cpus;
    uint64_t m_cgroup_cpuacct_period_us = 0;
    uint64_t m_cgroup_cpuacct_quota_us = 0; // if UINT64_MAX indicates there's no cpu limit

    //------------------------------------------------------------------------------
    // cpuacct controller
    //------------------------------------------------------------------------------
    std::string m_cpuacct_controller_name;
    FastFileReader m_cgroup_cpuacct_v1_reader_sys_stat; // if has split user/system time
    FastFileReader m_cgroup_cpuacct_v1_reader_user_stat; // if has split user/system time
    FastFileReader m_cgroup_cpuacct_v1_reader_combined_stat; // if has COMBINED user/system time
    FastFileReader m_cgroup_cpuacct_v1_reader_total_cpu_stat;
    FastFileReader m_cgroup_cpuacct_v2_reader_total_cpu_stat;
    bool m_cgroup_cpuacct_v1_supports_split_user_and_system_time = false;
    unsigned int m_num_cpus_cpuacct_cgroup = 0;

    // previous values sampled from "cpuacct" cgroup
    cpuacct_utilisation_t m_cpuacct_prev_values[MAX_LOGICAL_CPU];
    cpuacct_utilisation_t m_cpuacct_prev_values_for_total_cpu;
    cpuacct_throttling_t m_cpuacct_prev_values_for_throttling;

    //------------------------------------------------------------------------------
    // memory controller
    //------------------------------------------------------------------------------
    FastFileReader m_cgroup_memory_v2_current;
    FastFileReader m_cgroup_memory_v1v2_stat;
    FastFileReader m_cgroup_memory_v1_failcnt;
    FastFileReader m_cgroup_memory_v2_events;
    memory_events_t m_memory_prev_values;

    //------------------------------------------------------------------------------
    // shared variables between cgroup network/process tracker
    //------------------------------------------------------------------------------
    FastFileReader m_cgroup_processes_reader_pids;
    std::vector<pid_t> m_cgroup_all_pids; // this is the continuosly-updated list of PIDs/TIDs inside cgroup

    //------------------------------------------------------------------------------
    // cgroup network
    //------------------------------------------------------------------------------
    // previous values for network interfaces inside cgroup
    netinfo_map_t m_previous_netinfo;

    //------------------------------------------------------------------------------
    // cgroup processes tracking
    //------------------------------------------------------------------------------
    bool m_cgroup_processes_include_threads = false;
    std::map<pid_t, procsinfo_t> m_pid_databases[2];
    unsigned int m_pid_database_current_index = 0; // will be alternatively 0 and 1

    // it's possible, even if unlikely, for 2 PIDs to have identical process score...
    // that's why we use std::multimap instead of a std::map
    std::multimap<uint64_t /* process score */, proc_topper_t> m_topper_procs;
};
