#pragma once

#include "output_frontend.h"
#include <array>
#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------

#define DEBUGLOG_FUNCTION_START()                                                                                      \
    g_logger.LogDebug("%s() called at line %d of file %s\n", __func__, __LINE__, __FILE__);

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

enum PerformanceKpiFamily {
    PK_INVALID = 0,

    PK_CGROUPS = 1, // this activates the collection of cgroup-specific KPIs for other families:

    PK_CPU = 2,
    PK_DISK = 4,
    PK_MEMORY = 8,
    PK_NETWORK = 16,
    PK_PROCESSES = 32,

    PK_MAX,
    PK_ALL = PK_CGROUPS | PK_DISK | PK_CPU | PK_MEMORY | PK_NETWORK | PK_PROCESSES
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

typedef struct procsinfo_s {
    /* Process owner */
    uid_t uid;
    char username[64];
    /* Process details; see http://man7.org/linux/man-pages/man5/proc.5.html */
    int pi_pid;
    char pi_comm[64]; // The filename of the executable
    char pi_state;
    int pi_ppid; // The PID of the parent of this process.
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
    unsigned long long read_io; /* storage read bytes */
    unsigned long long write_io; /* storage write bytes */
} procsinfo_t;

typedef struct proc_topper_s {
    const procsinfo_t* current;
    const procsinfo_t* prev;
} proc_topper_t;

//------------------------------------------------------------------------------
// Command-Line Globals
//------------------------------------------------------------------------------

class CMonitorCollectorAppConfig {
public:
    CMonitorCollectorAppConfig() {}

    // configuration from command-line:
    bool m_bAllowMultipleInstances = false; // --allow-multiple-instances
    bool m_bDebug = false; // --debug
    bool m_bForeground = false; // --foreground
    OutputFields m_nOutputFields = PF_USED_BY_CHART_SCRIPT_ONLY;

    std::string m_strOutputDir; // --output-directory
    std::string m_strOutputFilenamePrefix; // --output-filename
    std::string m_strRemoteAddress; // --remote-ip
    std::string m_strRemoteSecret; // --remote-secret

    unsigned int m_nSamples = 0; // --num-samples
    unsigned int m_nSamplingInterval = 60; // --sampling-interval
    unsigned int m_nRemotePort = 0; // --remote-port

    unsigned int m_nCollectFlags = PK_ALL; // --collect: a combination of PerformanceKpiFamily values
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
    void read_data_number(const char* statname, const std::set<std::string>& allowedStatsNames);
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

    //------------------------------------------------------------------------------
    // CGroup functions
    //------------------------------------------------------------------------------

    void cgroup_init();
    void cgroup_config();
    bool cgroup_is_allowed_cpu(int cpu);
    void cgroup_proc_memory(const std::set<std::string>& allowedStatsNames);
    void cgroup_proc_cpuacct(double elapsed_sec, bool print);
    void cgroup_proc_tasks(double elapsed_sec, bool print);
    bool cgroup_collect_pids(std::vector<pid_t>& pids); // utility of cgroup_proc_tasks()

    //------------------------------------------------------------------------------
    // Functions to collect /proc stats
    //------------------------------------------------------------------------------

    void proc_stat(double elapsed, bool onlyCgroupAllowedCpus, OutputFields output_opts);
    void proc_diskstats(double elapsed, OutputFields output_opts);
    void proc_net_dev(double elapsed, OutputFields output_opts);
    void proc_loadavg();
    void proc_filesystems();
    void proc_uptime();

private:
    std::string m_strHostname;
    std::string m_strShortHostname;

    //------------------------------------------------------------------------------
    // CGroups variables
    //------------------------------------------------------------------------------
    bool m_bCGroupsFound = false;

    // paths of cgroups for this process:
    std::string cgroup_systemd_name;
    std::string cgroup_memory_kernel_path;
    std::string cgroup_cpuacct_kernel_path;
    std::string cgroup_cpuset_kernel_path;

    // limits read from the cgroups that apply to this process:
    uint64_t cgroup_memory_limit_bytes = 0;
    std::set<int> cgroup_cpus;

    //------------------------------------------------------------------------------
    // PID cpu/disk tracking
    //------------------------------------------------------------------------------
    std::map<pid_t, procsinfo_t> m_pid_databases[2];
    unsigned int m_pid_database_current_index = 0; // will be alternatively 0 and 1
    std::map<uint64_t /* process score */, proc_topper_t> m_topper;
    // std::map<pid_t, procsinfo_t>* m_pid_database_previous = nullptr;
    // std::map<pid_t, procsinfo_t>* m_pid_database_current = nullptr;
};

//------------------------------------------------------------------------------
// String/File utilities
//------------------------------------------------------------------------------

unsigned int replace_string(std::string& str, const std::string& from, const std::string& to, bool allOccurrences);
std::string to_lower(const std::string& orig_str);
std::string trim_string(const std::string& s);
void strip_spaces(char* s);
bool string2int(const std::string& str, int& result);
bool string2int(const std::string& str, uint64_t& result);
bool file_exists(const char* filename);
template <typename T> std::string stl_container2string(const T& par, const std::string& delim);
std::vector<std::string> split_string_in_array(const std::string& str, char splitter);
bool parse_string_with_multiple_ranges(const std::string& data, std::vector<int>& result);
bool parse_string_with_multiple_ranges(const std::string& data, std::set<int>& result);
bool search_integer(std::string filePath, uint64_t valueToSearch);
bool read_integer(std::string filePath, uint64_t& value);
bool read_integers_with_range_validation(
    const std::string& filename, int lower_limit, int upper_limit, std::set<int>& cpus);
