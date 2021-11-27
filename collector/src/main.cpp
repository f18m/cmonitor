/*
 * main.cpp: core routines for "cmonitor_collector"
 * Developer: Nigel Griffiths, Francesco Montorsi.
 * (C) Copyright 2018 Nigel Griffiths, Francesco Montorsi

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

#include "cgroups.h"
#include "cmonitor.h"
#include "header_info.h"
#include "logger.h"
#include "output_frontend.h"
#include "system.h"
#include "utils.h"
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define PID_FILE "/var/run/cmonitor.pid"

#define ADDITIONAL_HELP_COLUMN_START (40)

//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------

bool g_bExiting = false;

//------------------------------------------------------------------------------
// The App object
//------------------------------------------------------------------------------

class CMonitorCollectorApp {
public:
    CMonitorCollectorApp()
        : m_header_info_generator(&m_cfg, &m_output)
        , m_cgroups_collector(&m_cfg, &m_output)
        , m_system_collector(&m_cfg, &m_output)
    {
    }

    void init_defaults();
    void parse_args(int argc, char** argv);
    int run(int argc, char** argv);

private:
    void print_help();
    void check_pid_file();
    void get_timestamps(std::string& localTime, std::string& utcTime);
    void psample_date_time(long loop);
    double get_timestamp_sec();

private:
    //------------------------------------------------------------------------------
    // Configuration from CLI
    //------------------------------------------------------------------------------
    CMonitorCollectorAppConfig m_cfg;

    //------------------------------------------------------------------------------
    // Output channel
    //------------------------------------------------------------------------------
    CMonitorOutputFrontend m_output;

    //------------------------------------------------------------------------------
    // Stats collectors
    //------------------------------------------------------------------------------
    CMonitorHeaderInfo m_header_info_generator;
    CMonitorCgroups m_cgroups_collector;
    CMonitorSystem m_system_collector;
};

//------------------------------------------------------------------------------
// Command Line Globals
//------------------------------------------------------------------------------

struct option g_long_opts[] = {
    // Data sampling options
    { "sampling-interval", required_argument, 0, 's' }, // force newline
    { "num-samples", required_argument, 0, 'c' }, // force newline
    { "allow-multiple-instances", no_argument, 0, 'k' }, // force newline
    { "foreground", no_argument, 0, 'F' }, // force newline
    { "collect", required_argument, 0, 'C' }, // force newline
    { "deep-collect", no_argument, 0, 'e' }, // force newline
    { "cgroup-name", required_argument, 0, 'g' }, // force newline
    { "score-threshold", required_argument, 0, 't' }, // force newline
    { "custom-metadata", required_argument, 0, 'M' }, // force newline

    // Options to save data locally
    { "output-directory", required_argument, 0, 'm' }, // force newline
    { "output-filename", required_argument, 0, 'f' }, // force newline
    { "output-pretty", no_argument, 0, 'P' }, // force newline

    // Options to stream data remotely
    { "remote-ip", required_argument, 0, 'i' }, // force newline
    { "remote-port", required_argument, 0, 'p' }, // force newline
    { "remote-secret", required_argument, 0, 'X' }, // force newline
    { "remote-dbname", required_argument, 0, 'D' }, // force newline

    // Other options
    { "version", no_argument, 0, 'v' }, // force newline
    { "debug", no_argument, 0, 'd' }, // force newline
    { "help", no_argument, 0, 'h' }, // force newline
    { 0, 0, 0, 0 }
};

struct option_extended {
    const char* section_name;
    struct option* opt_descriptor;
    const char* additional_help;
} const g_opts_extended[] = {
    // Data sampling options
    { "Data sampling options", &g_long_opts[0], "Seconds between samples of data (default 60 seconds)." },
    { "Data sampling options", &g_long_opts[1],
        "Number of samples to collect; special values are:\n" // force newline
        "   '0': means forever (default value)\n" // force newline
        "   'until-cgroup-alive': until the selected cgroup is alive" },
    { "Data sampling options", &g_long_opts[2],
        "Allow multiple simultaneously-running instances of cmonitor_collector on this system." },
    { "Data sampling options", &g_long_opts[3], "Stay in foreground." },
    { "Data sampling options", &g_long_opts[4],
        "Collect specified list of performance stats. Available performance stats are:\n" // force newline
        "  'cpu': collect per-core CPU stats from /proc/stat\n" // force newline
        "  'memory': collect memory stats from /proc/meminfo, /proc/vmstat\n" // force newline
        "  'disk': collect disk stats from /proc/diskstats\n" // force newline
        "  'network': collect network stats from /proc/net/dev\n" // force newline
        "  'cgroup_cpu': collect CPU stats from the 'cpuacct' cgroup\n" // force newline
        "  'cgroup_memory': collect memory stats from 'memory' cgroup\n" // force newline
        /*"  'cgroup_blkio': collect IO stats from 'blkio' cgroup\n" NOT YET AVAILABLE */
        "  'cgroup_network': collect network statistics by interface for the network namespace of the cgroup\n" // force
                                                                                                                // newline
        "  'cgroup_processes': collect stats for each process inside the 'cpuacct' cgroup\n" // force newline
        "  'cgroup_threads': collect stats for each thread inside the 'cpuacct' cgroup\n" // force newline
        "  'all_baremetal': the combination of 'cpu', 'memory', 'disk', 'network'\n" // force newline
        "  'all_cgroup': the combination of 'cgroup_cpu', 'cgroup_memory', 'cgroup_processes'\n" // force newline
        "  'all': the combination of all previous stats (this is the default)\n" // force newline
        "Note that a comma-separated list of above stats can be provided." },
    { "Data sampling options", &g_long_opts[5],
        "Collect all available details about the stats families enabled by --collect.\n"
        "By default, for each family, only the stats that are used by the 'cmonitor_chart' companion utility\n"
        "are collected. With this option a more detailed but larger JSON / InfluxDB data stream is produced." },
    { "Data sampling options", &g_long_opts[6],
        "If cgroup sampling is active (--collect=cgroups*), this option allows to provide explicitly the name of\n"
        "the cgroup to monitor. If 'self' value is passed (the default), the statistics of the cgroups where \n"
        "cmonitor_collector runs will be collected. Note that this option is mostly useful when running \n"
        "cmonitor_collector directly on the baremetal since a process running inside a container cannot monitor\n"
        "the performances of other containers." },
    { "Data sampling options", &g_long_opts[7],
        "If cgroup process/thread sampling is active (--collect=cgroup_processes/cgroup_threads) use the provided\n"
        "score threshold to filter out non-interesting processes/threads. The 'score' is a number that is linearly\n"
        "increasing with the CPU usage. Defaults to value '1' to filter out all processes/threads having zero CPU\n"
        "usage. Use '0' to turn off filtering by score." },
    { "Data sampling options", &g_long_opts[8],
        "Allows to specify custom metadata key:value pairs that will be saved into the JSON output (if saving data\n"
        "locally) under the 'header.custom_metadata' path. See usage examples below.\n" },

    // Options to save data locally
    { "Options to save data locally", &g_long_opts[9],
        "Program will write output files to provided directory (default cwd)." },
    { "Options to save data locally", &g_long_opts[10],
        "Name the output files using provided prefix instead of defaulting to the filenames:\n"
        "\thostname_<year><month><day>_<hour><minutes>.json  (for JSON data)\n"
        "\thostname_<year><month><day>_<hour><minutes>.err   (for error log)\n"
        "Use special prefix 'stdout' to indicate that you want the utility to write on stdout.\n"
        "Use special prefix 'none' to indicate that you want to disable JSON generation." },
    { "Options to save data locally", &g_long_opts[11],
        "Generate a pretty-printed JSON file instead of a machine-friendly JSON (the default).\n" },

    // Options to stream data remotely
    { "Options to stream data remotely", &g_long_opts[12],
        "IP address or hostname of the InfluxDB instance to send measurements to;\n"
        "cmonitor_collector will use a database named 'cmonitor' to store them." },
    { "Options to stream data remotely", &g_long_opts[13], "Port used by InfluxDB." },
    { "Options to stream data remotely", &g_long_opts[14],
        "Set the InfluxDB collector secret (by default use environment variable CMONITOR_SECRET).\n" },
    { "Options to stream data remotely", &g_long_opts[15], "Set the InfluxDB database name.\n" },

    // help
    { "Other options", &g_long_opts[16], "Show version and exit" }, // force newline
    { "Other options", &g_long_opts[17],
        "Enable debug mode; automatically activates --foreground mode" }, // force newline
    { "Other options", &g_long_opts[18], "Show this help" },

    { NULL, NULL, NULL }
};

//------------------------------------------------------------------------------
// Signals
//------------------------------------------------------------------------------

void interrupt(int signum)
{
    switch (signum) {
    case SIGTERM:
    case SIGINT:
        g_bExiting = true;
        break;
    case SIGUSR1:
    case SIGUSR2:
        fflush(NULL);
        exit(0);
        break;
    }
}

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

PerformanceKpiFamily string2PerformanceKpiFamily(const std::string& str)
{
    if (to_lower(str) == "cpu")
        return PK_BAREMETAL_CPU;
    if (to_lower(str) == "disk")
        return PK_BAREMETAL_DISK;
    if (to_lower(str) == "memory")
        return PK_BAREMETAL_MEMORY;
    if (to_lower(str) == "network")
        return PK_BAREMETAL_NETWORK;

    if (to_lower(str) == "cgroup_cpu")
        return PK_CGROUP_CPU_ACCT;
    if (to_lower(str) == "cgroup_memory")
        return PK_CGROUP_MEMORY;
    if (to_lower(str) == "cgroup_blkio")
        return PK_CGROUP_BLKIO;
    if (to_lower(str) == "cgroup_network")
        return PK_CGROUP_NETWORK_INTERFACES;
    if (to_lower(str) == "cgroup_processes")
        return PK_CGROUP_PROCESSES;
    if (to_lower(str) == "cgroup_threads")
        return PK_CGROUP_THREADS;

    if (to_lower(str) == "all_baremetal")
        return PK_ALL_BAREMETAL;
    if (to_lower(str) == "all_cgroup")
        return PK_ALL_CGROUP;
    if (to_lower(str) == "all")
        return PK_ALL;

    return PK_INVALID;
}

std::string performanceKpiFamily2string(PerformanceKpiFamily k)
{
    switch (k) {
    case PK_BAREMETAL_CPU:
        return "cpu";
    case PK_BAREMETAL_DISK:
        return "disk";
    case PK_BAREMETAL_MEMORY:
        return "memory";
    case PK_BAREMETAL_NETWORK:
        return "network";

    case PK_CGROUP_CPU_ACCT:
        return "cgroup_cpu";
    case PK_CGROUP_MEMORY:
        return "cgroup_memory";
    case PK_CGROUP_BLKIO:
        return "cgroup_blkio";
    case PK_CGROUP_NETWORK_INTERFACES:
        return "cgroup_network";
    case PK_CGROUP_PROCESSES:
        return "cgroup_processes";
    case PK_CGROUP_THREADS:
        return "cgroup_threads";

    default:
        return "";
    }
}

//------------------------------------------------------------------------------
// Command line functions
//------------------------------------------------------------------------------

void CMonitorCollectorApp::print_help()
{
    static_assert(sizeof(g_opts_extended) / sizeof(g_opts_extended[0]) == sizeof(g_long_opts) / sizeof(g_long_opts[0]),
        "Mismatching number of options");

    std::cerr << "cmonitor_collector: Performance stats collector outputting JSON format." << std::endl;
    std::cerr << "List of arguments that can be provided follows:" << std::endl;
    std::cerr << std::endl;

    std::string last_sec_name;
    for (int i = 0;; i++) {
        const struct option* opt = g_opts_extended[i].opt_descriptor;
        if (!opt)
            break;

        if (g_opts_extended[i].section_name != last_sec_name) {
            std::cerr << g_opts_extended[i].section_name << std::endl;
            last_sec_name = g_opts_extended[i].section_name;
        }

        std::stringstream help;

        // to keep things we have that the
        //     short option char == value of struct option::val field
        help << "  -" << (char)opt->val << ", --" << opt->name;

        switch (opt->has_arg) {
        case no_argument:
            help << " ";
            break;
        case required_argument:
            help << "=<REQ ARG> ";
            break;
        case optional_argument:
            help << "=[OPT ARG] ";
            break;
        }

        size_t currlen = help.str().size();
        if (currlen < ADDITIONAL_HELP_COLUMN_START)
            help << std::string(ADDITIONAL_HELP_COLUMN_START - currlen, ' ');

        std::string additional_help(g_opts_extended[i].additional_help);
        replace_string(
            additional_help, "\n", "\n" + std::string(ADDITIONAL_HELP_COLUMN_START, ' '), true /*all occurrences*/);
        std::cerr << help.str() << additional_help << std::endl;
    }

    std::cerr << "" << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "    1) Collect data from OS every 5 mins all day:" << std::endl;
    std::cerr << "\tcmonitor_collector -s 300 -c 288 -m /home/perf" << std::endl;
    std::cerr << "    2) Collect data from a docker container:" << std::endl;
    std::cerr << "\tDOCKER_NAME=your_docker_name" << std::endl;
    std::cerr << "\tDOCKER_ID=$(docker ps -aq --no-trunc -f \"name=$DOCKER_NAME\")" << std::endl;
    std::cerr << "\tcmonitor_collector --allow-multiple-instances --num-samples=until-cgroup-alive " << std::endl;
    std::cerr << "\t\t\t--cgroup-name=docker/$DOCKER_ID --custom-metadata='cmonitor_chart_name:$DOCKER_NAME'"
              << std::endl;
    std::cerr << "    3) Use the defaults (-s 60, collect forever), saving to custom file in background:" << std::endl;
    std::cerr << "\tcmonitor_collector --output-filename=my_server_today" << std::endl;
    std::cerr << "    4) Crontab entry:" << std::endl;
    std::cerr << "\t0 4 * * * /usr/bin/cmonitor_collector -s 300 -c 288 -m /home/perf" << std::endl;
    std::cerr << "    5) Crontab entry for pumping data to an InfluxDB:" << std::endl;
    std::cerr << "\t* 0 * * * /usr/bin/cmonitor_collector -s 300 -c 288 -i admin.acme.com -p 8086" << std::endl;
    std::cerr << "    6) Pipe into 'myprog' half-a-day of sampled performance data:" << std::endl;
    std::cerr << "\tcmonitor_collector --sampling-interval=30 --num-samples=1440 --output-filename=stdout --foreground "
                 "| myprog"
              << std::endl;
    std::cerr << "" << std::endl;
    std::cerr << "NOTE: this is the cgroup-aware fork of original njmon software (see https://github.com/f18m/cmonitor)"
              << std::endl;

    // FIXME: add example for LXC container monitoring

    exit(0);
}

void CMonitorCollectorApp::init_defaults()
{
    if (getenv("CMONITOR_SECRET"))
        m_cfg.m_strRemoteSecret = getenv("CMONITOR_SECRET");

    // output file names
    get_hostname();

    time_t timer; /* used to work out the time details*/
    struct tm* tim = nullptr; /* used to work out the local hour/min/second */

    timer = time(0);
    tim = localtime(&timer);
    tim->tm_year += 1900; /* read localtime() manual page!! */
    tim->tm_mon += 1; /* because it is 0 to 11 */

    char filename[1024];
    sprintf(filename, "%s_%02d%02d%02d_%02d%02d", get_hostname().c_str(), tim->tm_year, tim->tm_mon, tim->tm_mday,
        tim->tm_hour, tim->tm_min);
    m_cfg.m_strOutputFilenamePrefix = filename;
}

void CMonitorCollectorApp::parse_args(int argc, char** argv)
{
    // assemble the string of short options by the long options:
    // to keep things we have that the
    //     short option char == value of struct option::val field
    std::string short_opts;
    for (size_t i = 0; i < sizeof(g_long_opts) / sizeof(g_long_opts[0]); i++) {
        if (!g_long_opts[i].val)
            continue;

        short_opts += (char)g_long_opts[i].val;
        if (g_long_opts[i].has_arg == required_argument)
            short_opts += ':';
        else if (g_long_opts[i].has_arg == optional_argument)
            short_opts += "::";
    }

    while (true) {
        int c = getopt_long(argc, argv, short_opts.c_str(), g_long_opts, 0);
        if (c < 0)
            break;
        else {
            switch (c) {
            // Data sampling options
            case 's':
                if (!string2int(optarg, m_cfg.m_nSamplingInterval)) {
                    printf("Unrecognized sampling interval: %s\n", optarg);
                    exit(51);
                }
                if (m_cfg.m_nSamplingInterval == 0) // safety check
                    m_cfg.m_nSamplingInterval = 1;
                break;
            case 'c':
                if (strcmp(optarg, "until-cgroup-alive") == 0)
                    m_cfg.m_nSamples = SPECIAL_NUMSAMPLES_UNTIL_CGROUP_ALIVE;
                else {
                    if (!string2int(optarg, m_cfg.m_nSamples)) {
                        printf("Unrecognized number of samples to collect: %s\n", optarg);
                        exit(51);
                    }
                }
                break;
            case 'k':
                m_cfg.m_bAllowMultipleInstances = true;
                break;
            case 'C': {
                std::vector<std::string> tokens = split_string_in_array(optarg, ',');
                m_cfg.m_nCollectFlags = 0;
                for (auto token : tokens) {
                    PerformanceKpiFamily k = string2PerformanceKpiFamily(token);
                    if (k == PK_INVALID) {
                        printf("Unrecognized performance statistics family provided: %s\n", token.c_str());
                        exit(51);
                    }
                    m_cfg.m_nCollectFlags |= k;
                }
            } break;
            case 'e':
                m_cfg.m_nOutputFields = PF_ALL;
                break;
            case 'F':
                m_cfg.m_bForeground = true;
                break;
            case 'g':
                m_cfg.m_strCGroupName = optarg;
                break;
            case 't':
                if (!string2int(optarg, m_cfg.m_nProcessScoreThreshold)) {
                    printf("Unrecognized score threshold: %s\n", optarg);
                    exit(51);
                }
                break;
            case 'M': {
                std::string key_value = optarg;

                std::vector<std::string> key_value_tokens = split_string_in_array(key_value, ':');
                if (key_value_tokens.size() != 2) {
                    printf(
                        "Invalid custom metadata [%s]. Every custom metadata option should be in the form key:value.\n",
                        optarg);
                    exit(51);
                }

                m_cfg.m_mapCustomMetadata.insert(std::make_pair(key_value_tokens[0], key_value_tokens[1]));
            } break;

                // Local data saving options
            case 'm':
                m_cfg.m_strOutputDir = optarg;
                break;
            case 'f': {
                m_cfg.m_strOutputFilenamePrefix = optarg;

                // if the filename contains the JSON extension, remove it
                size_t nchars = m_cfg.m_strOutputFilenamePrefix.size();
                if (nchars > 5 && m_cfg.m_strOutputFilenamePrefix.substr(nchars - 5) == ".json")
                    m_cfg.m_strOutputFilenamePrefix = m_cfg.m_strOutputFilenamePrefix.substr(0, nchars - 5);
            } break;
            case 'P':
                m_output.enable_json_pretty_print();
                break;

                // Remote data collector options
            case 'i':
                m_cfg.m_strRemoteAddress = optarg;
                break;
            case 'p':
                if (!string2int(optarg, m_cfg.m_nRemotePort)) {
                    printf("Unrecognized remote port: %s\n", optarg);
                    exit(51);
                }
                break;
            case 'X':
                m_cfg.m_strRemoteSecret = optarg;
                break;
            case 'D':
                m_cfg.m_strRemoteDatabaseName = optarg;
                break;

            // help
            case 'v':
                printf("cmonitor_collector version: %s\n", VERSION_STRING);
                exit(0);
                break;
            case 'd':
                m_cfg.m_bDebug = true;
                m_cfg.m_bForeground = true; // stay in foreground!
                break;
            case 'h':
                print_help();
                break;

            default:
                printf("%s: please use --help to read supported options.\n", argv[0]);
                exit(1);
                break;
            }
        }
    }

    if (optind < argc) {
        std::cerr << "Invalid parameters after last option: ";
        for (int i = optind; i < argc; i++)
            std::cerr << argv[i] << " ";
        std::cerr << std::endl;

        // print_help();		// do not clutter output with help
        std::cerr << "Run " << argv[0] << " with --help for more information." << std::endl;
        exit(1);
    }

    // check arguments we just parsed:

    if (!m_cfg.m_strRemoteAddress.empty() && m_cfg.m_nRemotePort <= 0) {
        printf("Option --remote-ip=%s provided but the --remote-port option was not provided\n",
            m_cfg.m_strRemoteAddress.c_str());
        exit(52);
    }
    if (m_cfg.m_strRemoteAddress.empty() && m_cfg.m_nRemotePort > 0) {
        printf("Option --remote-port=%lu provided but the --remote-ip option was not provided\n", m_cfg.m_nRemotePort);
        exit(53);
    }
    if ((m_cfg.m_nCollectFlags & PK_CGROUP_PROCESSES) && (m_cfg.m_nCollectFlags & PK_CGROUP_THREADS)) {
        printf("If --collect=cgroup_threads is provided, it is not required to provide --collect=cgroup_processes "
               "since implicitly statistics for all processes will already be collected\n");
        exit(54);
    }

    optind = 0; /* reset getopt lib */
}

//------------------------------------------------------------------------------
// Application core functions
//------------------------------------------------------------------------------

void CMonitorCollectorApp::get_timestamps(std::string& localTime, std::string& utcTime)
{
    time_t timer; /* used to work out the time details*/
    struct tm* tim = nullptr; /* used to work out the local hour/min/second */

    timer = time(0);
    tim = localtime(&timer);
    tim->tm_year += 1900; /* read localtime() manual page!! */
    tim->tm_mon += 1; /* because it is 0 to 11 */

    /* This is ISO 8601 datatime string format - ugly but get over it! :-) */

    char buffer[256];
    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year, tim->tm_mon, tim->tm_mday, tim->tm_hour, tim->tm_min,
        tim->tm_sec);
    localTime = buffer;

    tim = gmtime(&timer);
    tim->tm_year += 1900; /* read gmtime() manual page!! */
    tim->tm_mon += 1; /* because it is 0 to 11 */

    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year, tim->tm_mon, tim->tm_mday, tim->tm_hour, tim->tm_min,
        tim->tm_sec);
    utcTime = buffer;
}

double CMonitorCollectorApp::get_timestamp_sec()
{
#if 0
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1.0e-6;
#else
    struct timespec tv;
    if (clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
        return (double)tv.tv_sec + (double)tv.tv_nsec * 1.0e-9;
    return 0;
#endif
}

void CMonitorCollectorApp::psample_date_time(long loop)
{
    DEBUGLOG_FUNCTION_START();

    std::string localTime, utcTime;
    get_timestamps(localTime, utcTime);

    m_output.psection_start("timestamp");
    m_output.pstring("datetime", localTime.c_str());
    m_output.pstring("UTC", utcTime.c_str());
    m_output.plong("sample_index", loop);
    m_output.psection_end();
}

void CMonitorCollectorApp::check_pid_file()
{
    // immediately stop running if another instance of this software is already running.
    // Note that hmmonitor creates its own /var/run/ProcName.pid to track current PID,
    // don't mess with it! Instead here an empty LOCKED file is created with .lock extension
    int pid_file = open(PID_FILE, O_CREAT | O_RDWR, 0666);
    int rc = flock(pid_file, LOCK_EX | LOCK_NB);
    if (rc && EWOULDBLOCK == errno) {
        fprintf(stderr,
            "%s: another instance is already running... aborting. Use --allow-multiple-instances in case you actually "
            "want to run multiple instances.\n",
            PID_FILE);
        exit(-1);
    }
    // else: this is the first instance of this software... continue
}

int CMonitorCollectorApp::run(int argc, char** argv)
{
    // if only one instance allowed, do the check:
    if (!m_cfg.m_bAllowMultipleInstances)
        check_pid_file();

    if (!m_cfg.m_strOutputDir.empty()) {
        if (chdir(m_cfg.m_strOutputDir.c_str()) == -1) {
            perror("Change Directory failed");
            fprintf(stderr, "Directory attempted was: %s\n", m_cfg.m_strOutputDir.c_str());
            exit(11);
        } else {
            printf("Changed to directory: %s\n", m_cfg.m_strOutputDir.c_str());
        }
    }

    // init debug/error channels:
    CMonitorLogger::instance()->init_error_output_file(m_cfg.m_strOutputFilenamePrefix);
    if (m_cfg.m_bDebug)
        CMonitorLogger::instance()->enable_debug();

    // init the output channels:
    m_output.init_json_output_file(m_cfg.m_strOutputFilenamePrefix);
    if (!m_cfg.m_strRemoteAddress.empty() && m_cfg.m_nRemotePort != 0) {
        /* We are attempting sending the data remotely */
        m_output.init_influxdb_connection(m_cfg.m_strRemoteAddress, m_cfg.m_nRemotePort, m_cfg.m_strRemoteDatabaseName);
    }

    if (!m_cfg.m_bForeground) {
        assert(!m_cfg.m_bDebug); // in debug mode we enable foreground mode!

        /* disconnect from terminal */
        printf("cmonitor_collector will now run in background, collecting data as requested.\n");
        pid_t childpid;
        if ((childpid = fork()) != 0) {
            exit(0); /* parent returns OK */
        }

        CMonitorLogger::instance()->LogDebug("Running in daemon process:\n");

        // close default file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        setpgrp(); /* become process group leader */
        signal(SIGHUP, SIG_IGN); /* ignore hangups */
    }

    // init incremental stats (don't write yet anything!)
    bool bCollectCGroupInfo = // force newline
        (m_cfg.m_nCollectFlags & PK_CGROUP_CPU_ACCT) || // force newline
        (m_cfg.m_nCollectFlags & PK_CGROUP_MEMORY) || // force newline
        (m_cfg.m_nCollectFlags & PK_CGROUP_BLKIO) || // force newline
        (m_cfg.m_nCollectFlags & PK_CGROUP_PROCESSES) || // force newline
        (m_cfg.m_nCollectFlags & PK_CGROUP_THREADS);

    // if (bCollectCGroupInfo)
    // if cgropu monitoring is enabled we assume the user is interested in the CPU usage, computed from system-wide
    // statistic files, only of the CPUs that can be used by the cgroup-under-monitor:
    // m_system_collector.set_monitored_cpus(m_cgroups_collector.get_cgroup_cpus());

    m_system_collector.proc_stat(0, PF_NONE /* do not emit JSON data */);
    m_system_collector.proc_diskstats(0, PF_NONE /* do not emit JSON data */);
    m_system_collector.proc_net_dev(0, PF_NONE /* do not emit JSON data */);
    if (bCollectCGroupInfo) {
        m_cgroups_collector.cgroup_init();

        if (m_cfg.m_nCollectFlags & PK_CGROUP_CPU_ACCT)
            m_cgroups_collector.cgroup_proc_cpuacct(0);

        if (m_cfg.m_nCollectFlags & PK_CGROUP_PROCESSES)
            m_cgroups_collector.cgroup_proc_tasks(
                0, PF_NONE /* do not emit JSON */, false /* do not include threads */);
        if (m_cfg.m_nCollectFlags & PK_CGROUP_THREADS)
            m_cgroups_collector.cgroup_proc_tasks(0, PF_NONE /* do not emit JSON */, true /* do include threads */);
    }

    double current_time = get_timestamp_sec();

    // write stuff that is present only in the very first sample (never changes):
    m_output.pheader_start();
    m_header_info_generator.header_identity();
    m_header_info_generator.header_cmonitor_info(
        argc, argv, m_cfg.m_nSamplingInterval, m_cfg.m_nSamples, m_cfg.m_nCollectFlags);
    m_header_info_generator.header_etc_os_release();
    m_header_info_generator.header_version();
    if (bCollectCGroupInfo)
        m_cgroups_collector.output_config(); // needs to run _BEFORE_ lscpu() and proc_cpuinfo()
    m_header_info_generator.header_lscpu();
    m_header_info_generator
        .header_cpuinfo(); // ?!? this file contains basically the same info contained in lscpu output ?!?
    m_header_info_generator.header_meminfo();
    m_header_info_generator.header_lshw();
    m_header_info_generator.header_custom_metadata();
    m_output.push_header();

    /* first time just sleep(1) so the first snapshot has some real-ish data */
    if (m_cfg.m_nSamplingInterval <= 60)
        sleep(m_cfg.m_nSamplingInterval);
    else
        sleep(60); /* if a long time between snapshot do a quick one now so we have one in the bank */

    std::set<std::string> charted_stats_from_meminfo;
    if (m_cfg.m_nOutputFields == PF_USED_BY_CHART_SCRIPT_ONLY) {
        charted_stats_from_meminfo.insert("MemTotal");
        charted_stats_from_meminfo.insert("MemFree");
        charted_stats_from_meminfo.insert("Cached");
    }
    // else: leave empty

    std::set<std::string> charted_stats_from_cgroup_memory_v1, charted_stats_from_cgroup_memory_v2;
    if (m_cfg.m_nOutputFields == PF_USED_BY_CHART_SCRIPT_ONLY) {
        // cgroups v1
        charted_stats_from_cgroup_memory_v1.insert("stat.cache");
        charted_stats_from_cgroup_memory_v1.insert("stat.rss");
        charted_stats_from_cgroup_memory_v1.insert("failcnt");
        // cgroups v2
        charted_stats_from_cgroup_memory_v2.insert("stat.anon");
    }
    // else: leave empty

    // start actual data samples:
    CMonitorLogger::instance()->LogDebug(
        "Starting sampling of performance data; collect flags=%u", m_cfg.m_nCollectFlags);
    m_output.psample_array_start();
    double previous_time = current_time;
    for (unsigned int loop = 0; m_cfg.m_nSamples == 0 || loop < m_cfg.m_nSamples; loop++) {
        if (loop != 0)
            sleep(m_cfg.m_nSamplingInterval);
        CMonitorLogger::instance()->LogDebug("Starting sample %u/%lu", loop, m_cfg.m_nSamples);

        // get timestamp for the new sample
        previous_time = current_time;
        current_time = get_timestamp_sec();
        if (current_time == 0)
            continue; // failed in getting current time...

        double elapsed = current_time - previous_time;
        m_output.psample_start();

        // some stats are always collected, regardless of m_cfg.m_nCollectFlags
        psample_date_time(loop);
        // proc_uptime(); // not really useful!!
        m_system_collector.proc_loadavg();

        // baremetal stats:

        if (m_cfg.m_nCollectFlags & PK_BAREMETAL_CPU) {
            m_system_collector.proc_stat(elapsed, m_cfg.m_nOutputFields /* emit JSON */);
        }

        if (m_cfg.m_nCollectFlags & PK_BAREMETAL_MEMORY) {
            proc_read_numeric_stats_from(&m_output, "meminfo", charted_stats_from_meminfo);
            if (m_cfg.m_nOutputFields == PF_ALL)
                proc_read_numeric_stats_from(&m_output, "vmstat", std::set<std::string>());
        }

        if (m_cfg.m_nCollectFlags & PK_BAREMETAL_NETWORK) {
            m_system_collector.proc_net_dev(elapsed, m_cfg.m_nOutputFields /* emit JSON */);
        }

        if (m_cfg.m_nCollectFlags & PK_BAREMETAL_DISK) {
            m_system_collector.proc_diskstats(elapsed, m_cfg.m_nOutputFields /* emit JSON */);
            // proc_filesystems(); // I don't find this really useful...specially for ephemeral containers!
        }

        // cgroup stats:

        if (m_cfg.m_nCollectFlags & PK_CGROUP_CPU_ACCT) {
            // do not list all CPU informations when cgroup mode is ON: don't put information
            // for CPUs outside current cgroup!
            m_cgroups_collector.cgroup_proc_cpuacct(elapsed);
        }

        if (m_cfg.m_nCollectFlags & PK_CGROUP_MEMORY) {
            m_cgroups_collector.cgroup_proc_memory(
                charted_stats_from_cgroup_memory_v1, charted_stats_from_cgroup_memory_v2);
        }

        if (m_cfg.m_nCollectFlags & PK_CGROUP_NETWORK_INTERFACES) {
            m_cgroups_collector.cgroup_proc_network_interfaces(elapsed, m_cfg.m_nOutputFields /* emit JSON */);
        }

        if (m_cfg.m_nCollectFlags & PK_CGROUP_PROCESSES) {
            m_cgroups_collector.cgroup_proc_tasks(
                elapsed, m_cfg.m_nOutputFields /* emit JSON */, false /* do not include threads */);
        }
        if (m_cfg.m_nCollectFlags & PK_CGROUP_THREADS) {
            m_cgroups_collector.cgroup_proc_tasks(
                elapsed, m_cfg.m_nOutputFields /* emit JSON */, true /* do include threads */);
        }

        m_output.push_current_sample();

        // in debug mode provide an indication of how much optimized is cmonitor_collector:
        if (m_cfg.m_bDebug) {
            double sampling_time = get_timestamp_sec() - current_time;
            CMonitorLogger::instance()->LogDebug("Sampling time was %.3fmsec", sampling_time * 1000);
        }

        if (g_bExiting)
            break; // graceful exit allows to produce a valid JSON on SIGTERM signals!
        if (m_cfg.m_nSamples == SPECIAL_NUMSAMPLES_UNTIL_CGROUP_ALIVE && !m_cgroups_collector.cgroup_still_exists())
            break;
    }

    /* finish-of */
    m_output.psample_array_end();
    fflush(NULL);

    CMonitorLogger::instance()->LogDebug("Exiting gracefully with return code 0. Logged %lu errors in this run.",
        CMonitorLogger::instance()->get_num_errors());
    return 0;
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // init defaults (can be overridden by cmd line options):
    CMonitorCollectorApp app;
    app.init_defaults();

    // parse cmd line:
    app.parse_args(argc, argv);

    signal(SIGTERM, interrupt);
    signal(SIGINT, interrupt);
    signal(SIGUSR1, interrupt);
    signal(SIGUSR2, interrupt);

    // run:
    return app.run(argc, argv);
}
