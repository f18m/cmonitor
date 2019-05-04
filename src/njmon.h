#pragma once

#include <set>
#include <string.h>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------

#define DEBUGLOG_FUNCTION_START() LogDebug("%s() called at line %d of file %s\n", __func__, __LINE__, __FILE__);

//------------------------------------------------------------------------------
// Command-Line Globals
//------------------------------------------------------------------------------

enum PerformanceKpiFamily {
    PK_INVALID = 0,

    PK_CGROUPS = 1, // this activates the collection of cgroup-specific KPIs for other families:

    PK_CPU = 2,
    PK_DISK = 4,
    PK_MEMORY = 8,
    PK_NETWORK = 16,

    PK_MAX,
    PK_ALL = PK_CGROUPS | PK_DISK | PK_CPU | PK_MEMORY | PK_NETWORK
};

PerformanceKpiFamily string2PerformanceKpiFamily(const std::string&);
std::string string2PerformanceKpiFamily(PerformanceKpiFamily k);

class NjmonCollectorAppConfig {
public:
    NjmonCollectorAppConfig() {}

    // configuration from command-line:
    bool m_bAllowMultipleInstances = false; // --allow-multiple-instances
    bool m_bDebug = false; // --debug
    bool m_bForeground = false; // --foreground

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
extern NjmonCollectorAppConfig g_cfg;

//------------------------------------------------------------------------------
// The App object
//------------------------------------------------------------------------------

class NjmonCollectorApp {
public:
    NjmonCollectorApp() {}

    void init_defaults();
    void parse_args(int argc, char** argv);
    int run(int argc, char** argv);

private:
    void print_help();
    void make_pid_file();
    void check_pid_file();
    std::string get_hostname();
    void get_timestamps(std::string& localTime, std::string& utcTime);
    void date_time(long loop);
    void identity();
    void njmon_info(int argc, char** argv, long sampling_interval_sec, long num_samples, unsigned int collect_flags);
    void file_read_one_stat(const char* file, const char* name);

    // Global logging function for this app
    void LogDebug(const char* line, ...);
    void LogError(const char* line, ...);

    //------------------------------------------------------------------------------
    // JSON functions
    //------------------------------------------------------------------------------

    void prawc(const char c);
    void praw(const char* string);
    void pstart();
    void pfinish();
    void psample();
    void psampleend(bool comma_needed);
    void indent();
    void psection(const char* section);
    void psub(const char* resource);
    void psubend();
    void psectionend();
    void phex(const char* name, long long value);
    void plong(const char* name, long long value);
    void pdouble(const char* name, double value);
    void pstats();
    void pstring(const char* name, const char* value);
    void push();
    void remove_ending_comma_if_any();
    void buffer_check();

    //------------------------------------------------------------------------------
    // CGroup functions
    //------------------------------------------------------------------------------

    void cgroup_init();
    void cgroup_config();
    bool cgroup_is_allowed_cpu(int cpu);
    void cgroup_proc_memory();
    void cgroup_proc_cpuacct(double elapsed_sec, bool print);

    //------------------------------------------------------------------------------
    // Collect functions
    //------------------------------------------------------------------------------

    void proc_stat(double elapsed, bool onlyCgroupAllowedCpus, bool print);
    void proc_diskstats(double elapsed, int print);
    void proc_net_dev(double elapsed, int print);
    void proc_cpuinfo();
    void etc_os_release();
    void read_data_number(const char* statname);
    void proc_loadavg();
    void proc_filesystems();
    void proc_version();
    void lscpu();
    void lshw();
    void strip_spaces(char* s);
    void proc_uptime();

private:
    // other globals:

    std::string m_strHostname;
    std::string m_strShortHostname;

    std::string m_strErrorFileName;

    bool m_bCGroupsFound = false;

    // output:

    FILE* m_outputJson = nullptr;
    FILE* m_outputErr = nullptr;
    int m_outputSocketFd = 0; /*default is stdout, only changed if we are using a remote socket */
};

// Utilities
unsigned int replace_string(std::string& str, const std::string& from, const std::string& to, bool allOccurrences);
std::string to_lower(const std::string& orig_str);
std::string trim_string(const std::string& s);
bool string2int(const std::string& str, int& result);
bool string2int(const std::string& str, uint64_t& result);
bool file_exists(const char* filename);
template <typename T> std::string stl_container2string(const T& par, const std::string& delim);
std::vector<std::string> split_string_in_array(const std::string& str, char splitter);
bool parse_string_with_multiple_ranges(const std::string& data, std::vector<int>& result);
bool parse_string_with_multiple_ranges(const std::string& data, std::set<int>& result);
bool read_integer(std::string filePath, uint64_t& value);
bool read_integers_with_range_validation(
    const std::string& filename, int lower_limit, int upper_limit, std::set<int>& cpus);
