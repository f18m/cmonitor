/*
 * cmonitor.h -- main objects for the cmonitor_collector application
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
    g_logger.LogDebug("%s() called at line %d of file %s\n", __func__, __LINE__, __FILE__);

#define PROCESS_DEBUGGING_ADDRESSES_SIGNALS (0)

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define SPECIAL_NUMSAMPLES_UNTIL_CGROUP_ALIVE (UINT64_MAX)

enum PerformanceKpiFamily {
    PK_INVALID = 0,

    PK_CPU = 2,
    PK_DISK = 4,
    PK_MEMORY = 8,
    PK_NETWORK = 16,
    // PK_PROCESSES = 32,

    PK_CGROUP_CPU_ACCT = 128,
    PK_CGROUP_MEMORY = 256,
    PK_CGROUP_BLKIO = 512,
    PK_CGROUP_PROCESSES = 1024,

    PK_MAX,

    PK_ALL_BAREMETAL = PK_CPU | PK_DISK | PK_MEMORY | PK_NETWORK,
    PK_ALL_CGROUP = PK_CGROUP_CPU_ACCT | PK_CGROUP_MEMORY | PK_CGROUP_BLKIO | PK_CGROUP_PROCESSES,

    PK_ALL = PK_CPU | PK_DISK | PK_MEMORY | PK_NETWORK // force newline
        | PK_CGROUP_CPU_ACCT | PK_CGROUP_MEMORY | PK_CGROUP_BLKIO | PK_CGROUP_PROCESSES
};

PerformanceKpiFamily string2PerformanceKpiFamily(const std::string&);
std::string performanceKpiFamily2string(PerformanceKpiFamily k);

enum OutputFields {
    PF_NONE, // force newline
    PF_ALL, // force newline
    PF_USED_BY_CHART_SCRIPT_ONLY // force newline
};

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

/*
 * Structure to store CPU usage specs as reported by Linux kernel
 * NOTE: all fields specify amount of time, measured in units of USER_HZ
         (1/100ths of a second on most architectures); this means that if the
         _delta_ CPU value reported is 60 in mode X, then that mode took 60% of the CPU!
         IOW there is no need to do any math to produce a percentage, just taking
         the delta of the absolute, monotonic-increasing value and divide by the time
*/
typedef struct cpu_specs_s {
    long long user;
    long long nice;
    long long sys;
    long long idle;
    long long iowait;
    long long hardirq;
    long long softirq;
    long long steal;
    long long guest;
    long long guestnice;
} cpu_specs_t;

typedef struct procsinfo_s {
    /* Process owner */
    uid_t uid;
    char username[64];
    /* Process details; see http://man7.org/linux/man-pages/man5/proc.5.html */
    int pi_pid;
    char pi_comm[64]; // The filename of the executable
    char pi_state;
    int pi_ppid; // The PID of the parent of this process.
    int pi_tgid; // The Thread group ID of this process
    int pi_pgrp; // The process group ID of the process
    int pi_session; // The session ID of the process.
    int pi_tty_nr; //  The controlling terminal of the process
    int pi_tty_pgrp; // The ID of the foreground process group of the controlling terminal
    unsigned long pi_flags; // The kernel flags word of the process
    unsigned long pi_minflt; // The number of minor faults the process has made which have not required loading a memory
                             // page from disk.
    unsigned long pi_child_min_flt;
    unsigned long pi_majflt; // The number of major faults the process has made which have required loading a memory
                             // page from disk.
    unsigned long pi_child_maj_flt;
    unsigned long pi_utime; // Amount of time that this process has been scheduled in user mode, in clock ticks
    unsigned long pi_stime; // Amount of time that this process has been scheduled in kernel mode, in clock ticks
    long pi_child_utime; // Amount of time that this process's waited-for children have been scheduled in user mode
    long pi_child_stime; // Amount of time that this process's waited-for children have been scheduled in kernel mode
    long pi_priority;
    long pi_nice; // The nice value
    long pi_num_threads; // Number of threads in this process
    unsigned long pi_start_time;
    unsigned long pi_vsize; // Virtual memory size in bytes.
    long pi_rss; /* - 3 */
    unsigned long pi_rsslimit;
    unsigned long pi_start_code;
    unsigned long pi_end_code;
    unsigned long pi_start_stack;
    unsigned long pi_esp;
    unsigned long pi_eip;
    /* The signal information here is obsolete. See "man proc" */
    unsigned long pi_signal_pending;
    unsigned long pi_signal_blocked;
    unsigned long pi_signal_ignore;
    unsigned long pi_signal_catch;
    unsigned long pi_wchan;
    unsigned long pi_swap_pages;
    unsigned long pi_child_swap_pages;
    int pi_signal_exit;
    int pi_last_cpu;
    unsigned long pi_realtime_priority;
    unsigned long pi_sched_policy;
    unsigned long long pi_delayacct_blkio_ticks;
    /* Process stats for memory */
    unsigned long statm_size; /* total program size, measured in pages */
    unsigned long statm_resident; /* resident set size, measured in pages */
    unsigned long statm_share; /* shared pages */
    unsigned long statm_trs; /* text (code) */
    unsigned long statm_drs; /* data/stack */
    unsigned long statm_lrs; /* library */
    unsigned long statm_dt; /* dirty pages */
    /* Process stats for disks */
    unsigned long long io_rchar; // includes things such as terminal I/O and is
                                 // unaffected by whether or not actual physical disk I/O
                                 // was required (the read might have been satisfied from pagecache).
    unsigned long long io_wchar; // The number of bytes which this task has caused, or
                                 // shall cause to be written to disk
    unsigned long long io_read_bytes; // Attempt to count the number of bytes which this process
                                      // really did cause to be fetched from the storage layer.
    unsigned long long io_write_bytes; // Attempt to count the number of bytes which this process
                                       // caused to be sent to the storage layer.
} procsinfo_t;

typedef struct proc_topper_s {
    const procsinfo_t* current;
    const procsinfo_t* prev;
} proc_topper_t;

//------------------------------------------------------------------------------
// Command-Line Globals
// (Configuration from command-line)
//------------------------------------------------------------------------------

class CMonitorCollectorAppConfig {
public:
    CMonitorCollectorAppConfig() {}

    // configuration for this process:
    bool m_bAllowMultipleInstances = false; // --allow-multiple-instances
    bool m_bDebug = false; // --debug
    bool m_bForeground = false; // --foreground

    // local data saving opts
    std::string m_strOutputDir; // --output-directory
    std::string m_strOutputFilenamePrefix; // --output-filename

    // remove streaming opts
    std::string m_strRemoteAddress; // --remote-ip
    std::string m_strRemoteSecret; // --remote-secret
    uint64_t m_nRemotePort = 0; // --remote-port

    // data collecting options
    uint64_t m_nSamples = 0; // --num-samples
    uint64_t m_nSamplingInterval = 60; // --sampling-interval
    unsigned int m_nCollectFlags = PK_ALL; // --collect: a combination of PerformanceKpiFamily values
    OutputFields m_nOutputFields = PF_USED_BY_CHART_SCRIPT_ONLY; // --deep-collect
    std::string m_strCGroupName; // --cgroup-name
};

// app-wide config settings:
extern CMonitorCollectorAppConfig g_cfg;

//------------------------------------------------------------------------------
// Logging functions for this app
//------------------------------------------------------------------------------

class CMonitorLoggerUtils {
public:
    void init_error_output_file(const std::string& filenamePrefix);

    void LogDebug(const char* line, ...);
    void LogError(const char* line, ...);

private:
    std::string m_strErrorFileName;

    // output:
    FILE* m_outputErr = nullptr;
};

// app-wide logger:
extern CMonitorLoggerUtils g_logger;

//------------------------------------------------------------------------------
// The App object
//------------------------------------------------------------------------------

class CMonitorCollectorApp {
public:
    CMonitorCollectorApp() {}

    void init_defaults();
    void parse_args(int argc, char** argv);
    int run(int argc, char** argv);

private:
    void print_help();
    void check_pid_file();
    std::string get_hostname();
    void get_timestamps(std::string& localTime, std::string& utcTime);
    void file_read_one_stat(const char* file, const char* name);
    void proc_read_numeric_stats_from(const char* statname, const std::set<std::string>& allowedStatsNames);
    void psample_date_time(long loop);
    double get_timestamp_sec();

    //------------------------------------------------------------------------------
    // JSON header functions
    //------------------------------------------------------------------------------

    void header_identity();
    void header_cmonitor_info(
        int argc, char** argv, long sampling_interval_sec, long num_samples, unsigned int collect_flags);
    void header_etc_os_release();
    void header_cpuinfo();
    void header_version();
    void header_lscpu();
    void header_lshw();
    void header_meminfo();

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
    void cgroup_proc_tasks(double elapsed_sec, OutputFields output_opts);
    bool cgroup_collect_pids(std::vector<pid_t>& pids); // utility of cgroup_proc_tasks()

    //------------------------------------------------------------------------------
    // Functions to collect /proc stats
    //------------------------------------------------------------------------------

    void proc_stat(double elapsed, bool onlyCgroupAllowedCpus, OutputFields output_opts);
    void proc_stat_cpu_total(const char* cpu_data, double elapsed_sec, OutputFields output_opts, cpu_specs_t& total_cpu,
        int max_cpu_count); // utility of proc_stat()
    int proc_stat_cpu_index(const char* cpu_data, double elapsed_sec, OutputFields output_opts,
        cpu_specs_t* logical_cpu, bool onlyCgroupAllowedCpus);

    void proc_diskstats(double elapsed, OutputFields output_opts);
    void proc_net_dev(double elapsed, OutputFields output_opts);
    void proc_loadavg();
    void proc_filesystems();
    void proc_uptime();

private:
    //------------------------------------------------------------------------------
    // Misc globals
    //------------------------------------------------------------------------------
    std::string m_strHostname; // full hostname for this machine
    std::string m_strShortHostname; // short hostname for this machine

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

    //------------------------------------------------------------------------------
    // Process tracking
    //------------------------------------------------------------------------------
    std::map<pid_t, procsinfo_t> m_pid_databases[2];
    unsigned int m_pid_database_current_index = 0; // will be alternatively 0 and 1
    std::map<uint64_t /* process score */, proc_topper_t> m_topper;
};

//------------------------------------------------------------------------------
// String/File utilities
//------------------------------------------------------------------------------

unsigned int replace_string(std::string& str, const std::string& from, const std::string& to, bool allOccurrences);
std::string to_lower(const std::string& orig_str);
std::string trim_string(const std::string& s);
void strip_spaces(char* s);
bool string2int(const char* s, uint64_t& result);
bool file_or_dir_exists(const char* filename);
template <typename T> std::string stl_container2string(const T& par, const std::string& delim);
std::vector<std::string> split_string_in_array(const std::string& str, char splitter);
bool parse_string_with_multiple_ranges(const std::string& data, std::vector<int>& result);
bool parse_string_with_multiple_ranges(const std::string& data, std::set<int>& result);
bool search_integer(std::string filePath, uint64_t valueToSearch);
bool read_integer(std::string filePath, uint64_t& value);
bool read_integers_with_range_validation(
    const std::string& filename, uint64_t lower_limit, uint64_t upper_limit, std::set<uint64_t>& cpus);
