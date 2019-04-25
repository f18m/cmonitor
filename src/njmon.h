#pragma once

#include <string.h>
#include <string>

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------

#define VERSION_STRING "22-3"

#define DEBUGLOG_FUNCTION_START()                                                                                      \
    if (g_cfg.m_bDebug)                                                                                                \
        fprintf(stderr, "%s called line %d\n", __func__, __LINE__);

//------------------------------------------------------------------------------
// Command-Line Globals
//------------------------------------------------------------------------------

enum PerformanceKpiFamily {
    PK_CGROUPS = 1,

    PK_DISK = 2,
    PK_CPU = 4,
    PK_MEMORY = 8,
    PK_NETWORK = 16,

    PK_ALL = PK_CGROUPS | PK_DISK | PK_CPU | PK_MEMORY | PK_NETWORK
};

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
    NjmonCollectorApp() { memset(&timer, 0, sizeof(timer)); }

    void init_defaults();
    void parse_args(int argc, char** argv);
    int run(int argc, char** argv);

private:
    void print_help();
    void make_pid_file();
    void check_pid_file();
    std::string get_hostname();
    void get_time();
    void get_localtime();
    void get_utc();
    void date_time(long loop);
    void identity();
    void njmon_info(int argc, char** argv, long sampling_interval_sec, long num_samples);
    void file_read_one_stat(const char* file, const char* name);

    //------------------------------------------------------------------------------
    // JSON functions
    //------------------------------------------------------------------------------

    void praw(const char* string);
    void pstart();
    void pfinish();
    void psample();
    void psampleend(int comma_needed);
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
    int cgroup_is_allowed_cpu(int cpu);
    void cgroup_proc_memory();
    void cgroup_proc_cpuacct(double elapsed_sec);

    //------------------------------------------------------------------------------
    // Collect functions
    //------------------------------------------------------------------------------

    void proc_stat(double elapsed, int print);
    void proc_diskstats(double elapsed, int print);
    void proc_net_dev(double elapsed, int print);
    void proc_cpuinfo();
    void etc_os_release();
    void read_data_number(const char* statname);
    void proc_loadavg();
    void proc_filesystems();
    void proc_version();
    void lscpu();
    void strip_spaces(char* s);
    void proc_uptime();

private:
    // other globals:

    std::string m_strHostname;
    std::string m_strShortHostname;

    bool m_bCGroupsFound = false;

    time_t timer; /* used to work out the time details*/
    struct tm* tim = nullptr; /* used to work out the local hour/min/second */

    // output:

    FILE* m_outputJson = nullptr;
    FILE* m_outputErr = nullptr;
    int m_outputSocketFd = 0; /*default is stdout, only changed if we are using a remote socket */
};

// Global logging function for this app
void LogDebug(const char* line, ...);
void LogError(const char* line, ...);

// Utilities
unsigned int ReplaceString(std::string& str, const std::string& from, const std::string& to, bool allOccurrences);
