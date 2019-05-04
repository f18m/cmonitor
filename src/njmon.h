#pragma once

#include <set>
#include <string.h>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------

namespace influxdb_cpp {
struct server_info;
}

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
    NjmonCollectorApp()
        : m_influxdb_server(nullptr)
    {
    }

    void init_defaults();
    void parse_args(int argc, char** argv);
    int run(int argc, char** argv);

private:
    void print_help();
    void check_pid_file();
    std::string get_hostname();
    void get_timestamps(std::string& localTime, std::string& utcTime);
    void file_read_one_stat(const char* file, const char* name);
    void read_data_number(const char* statname);

    //------------------------------------------------------------------------------
    // Logging functions for this app
    //------------------------------------------------------------------------------

    void LogDebug(const char* line, ...);
    void LogError(const char* line, ...);

    //------------------------------------------------------------------------------
    // JSON low-level functions
    //------------------------------------------------------------------------------

    void prawc(const char c);
    void praw(const char* string);
    void pstart();
    void pfinish();
    void psample();
    void psampleend(bool comma_needed);
    void pindent();
    void psection(const char* section);
    void psub(const char* resource);
    void psubend();
    void psectionend();
    void phex(const char* name, long long value);
    void plong(const char* name, long long value);
    void pdouble(const char* name, double value);
    void pstring(const char* name, const char* value);
    void pstats();

    void premove_ending_comma_if_any();
    void pbuffer_check();

    void push(); // writes on file, stdout or socket
    void psample_date_time(long loop);

    //------------------------------------------------------------------------------
    // JSON header functions
    //------------------------------------------------------------------------------

    void header_identity();
    void header_njmon_info(
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
    void cgroup_proc_memory();
    void cgroup_proc_cpuacct(double elapsed_sec, bool print);

    //------------------------------------------------------------------------------
    // Functions to collect /proc stats
    //------------------------------------------------------------------------------

    void proc_stat(double elapsed, bool onlyCgroupAllowedCpus, bool print);
    void proc_diskstats(double elapsed, int print);
    void proc_net_dev(double elapsed, int print);
    void proc_loadavg();
    void proc_filesystems();
    void proc_uptime();

    //------------------------------------------------------------------------------
    // Remote connection functions
    //------------------------------------------------------------------------------

    void remote_create_influxdb_connection(const std::string& hostname, unsigned int port);
    void remote_push();

private:
    std::string m_strHostname;
    std::string m_strShortHostname;
    std::string m_strErrorFileName;
    bool m_bCGroupsFound = false;

    influxdb_cpp::server_info* m_influxdb_server; //("127.0.0.1", 8086, "db", "usr", "pwd");

    // output:

    FILE* m_outputJson = nullptr;
    FILE* m_outputErr = nullptr;
    //    int m_outputSocketFd = 0; /*default is stdout, only changed if we are using a remote socket */
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
bool read_integer(std::string filePath, uint64_t& value);
bool read_integers_with_range_validation(
    const std::string& filename, int lower_limit, int upper_limit, std::set<int>& cpus);
