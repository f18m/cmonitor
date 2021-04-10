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

#include "cmonitor.h"
#include "output_frontend.h"
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
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

CMonitorLoggerUtils g_logger;
CMonitorCollectorAppConfig g_cfg;
CMonitorCollectorApp g_app;
bool g_bExiting = false;

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
        "  'cgroup_processes': collect stats for each process inside the 'cpuacct' cgroup\n" // force newline
        "  'all_baremetal': the combination of 'cpu', 'memory', 'disk', 'network'\n"
        "  'all_cgroup': the combination of 'cgroup_cpu', 'cgroup_memory', 'cgroup_processes'\n"
        "  'all': the combination of all previous stats (this is the default)\n"
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
        "the performances of other containers.\n" },

    // Options to save data locally
    { "Options to save data locally", &g_long_opts[7],
        "Program will write output files to provided directory (default cwd)." },
    { "Options to save data locally", &g_long_opts[8],
        "Name the output files using provided prefix instead of defaulting to the filenames:\n"
        "\thostname_<year><month><day>_<hour><minutes>.json  (for JSON data)\n"
        "\thostname_<year><month><day>_<hour><minutes>.err   (for error log)\n"
        "Use special prefix 'stdout' to indicate that you want the utility to write on stdout.\n"
        "Use special prefix 'none' to indicate that you want to disable JSON genreation." },
    { "Options to save data locally", &g_long_opts[9],
        "Generate a pretty-printed JSON file instead of a machine-friendly JSON (the default).\n" },

    // Options to stream data remotely
    { "Options to stream data remotely", &g_long_opts[10],
        "IP address or hostname of the InfluxDB instance to send measurements to;\n"
        "cmonitor_collector will use a database named 'cmonitor' to store them." },
    { "Options to stream data remotely", &g_long_opts[11], "Port used by InfluxDB." },
    { "Options to stream data remotely", &g_long_opts[12],
        "Set the InfluxDB collector secret (by default use environment variable CMONITOR_SECRET).\n" },
    { "Options to stream data remotely", &g_long_opts[13], "Set the InfluxDB database name.\n" },

    // help
    { "Other options", &g_long_opts[14], "Show version and exit" }, // force newline
    { "Other options", &g_long_opts[15],
        "Enable debug mode; automatically activates --foreground mode" }, // force newline
    { "Other options", &g_long_opts[16], "Show this help" },

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
        return PK_CPU;
    if (to_lower(str) == "disk")
        return PK_DISK;
    if (to_lower(str) == "memory")
        return PK_MEMORY;
    if (to_lower(str) == "network")
        return PK_NETWORK;

    if (to_lower(str) == "cgroup_cpu")
        return PK_CGROUP_CPU_ACCT;
    if (to_lower(str) == "cgroup_memory")
        return PK_CGROUP_MEMORY;
    if (to_lower(str) == "cgroup_blkio")
        return PK_CGROUP_BLKIO;
    if (to_lower(str) == "cgroup_processes")
        return PK_CGROUP_PROCESSES;

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
    case PK_CPU:
        return "cpu";
    case PK_DISK:
        return "disk";
    case PK_MEMORY:
        return "memory";
    case PK_NETWORK:
        return "network";

    case PK_CGROUP_CPU_ACCT:
        return "cgroup_cpu";
    case PK_CGROUP_MEMORY:
        return "cgroup_memory";
    case PK_CGROUP_BLKIO:
        return "cgroup_blkio";
    case PK_CGROUP_PROCESSES:
        return "cgroup_processes";

    default:
        return "";
    }
}

//------------------------------------------------------------------------------
// Logger functions
//------------------------------------------------------------------------------

void CMonitorLoggerUtils::init_error_output_file(const std::string& filenamePrefix)
{
    if (filenamePrefix == "stdout") {
        // open stderr as FILE*:
        if ((m_outputErr = fdopen(STDERR_FILENO, "w")) == 0) {
            perror("opening stderr for write");
            exit(13);
        }
    } else if (filenamePrefix == "none") {
        // avoid opening an error file:
        m_outputErr = nullptr;
    } else {

        m_strErrorFileName = filenamePrefix;
        if (filenamePrefix.size() > 5 && filenamePrefix.substr(filenamePrefix.size() - 5) == ".json")
            m_strErrorFileName = filenamePrefix.substr(0, filenamePrefix.size() - 5) + ".err";
        else
            m_strErrorFileName += ".err";

        // prepare output error file but don't open it yet
        printf("Errors (if any) will be logged into the file '%s'\n", m_strErrorFileName.c_str());

        // however if it already exists, remove it:
        if (file_or_dir_exists(m_strErrorFileName.c_str()))
            unlink(m_strErrorFileName.c_str());
    }

    fflush(NULL);
}

void CMonitorLoggerUtils::LogDebug(const char* line, ...)
{
    char currLogLine[256];

    if (!g_cfg.m_bDebug)
        return;

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    // in debug mode stdout is still open, so we can printf:
    printf("%s", currLogLine);
    size_t lastCh = strlen(currLogLine) - 1;
    if (currLogLine[lastCh] != '\n')
        printf("\n");
}

void CMonitorLoggerUtils::LogError(const char* line, ...)
{
    char currLogLine[256];

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    if (!m_outputErr && !m_strErrorFileName.empty()) {
        // apparently this is the first error happening: time to open the logfile for errors:
        if ((m_outputErr = fopen(m_strErrorFileName.c_str(), "w")) == 0) {
            exit(14);
        }
    }

    if (m_outputErr) {
        // errors always go in their dedicated file
        fprintf(m_outputErr, "ERROR: %s\n", currLogLine);
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
    std::cerr << "    1) Collect data every 5 mins all day:" << std::endl;
    std::cerr << "\tcmonitor_collector -s 300 -c 288 -m /home/perf" << std::endl;
    std::cerr << "    2) Pipe to data handler using half a day of data:" << std::endl;
    std::cerr << "\tcmonitor_collector --sampling-interval=30 --num-samples=1440 --output-filename=stdout --foreground "
                 "| myprog"
              << std::endl;
    std::cerr << "    3) Use the defaults (-s 60, collect forever), saving to custom file in background:" << std::endl;
    std::cerr << "\tcmonitor_collector --output-filename=my_server_today" << std::endl;
    std::cerr << "    4) Crontab entry:" << std::endl;
    std::cerr << "\t0 4 * * * /usr/bin/cmonitor_collector -s 300 -c 288 -m /home/perf" << std::endl;
    std::cerr << "    5) Crontab entry for pumping data to an InfluxDB:" << std::endl;
    std::cerr << "\t* 0 * * * /usr/bin/cmonitor_collector -s 300 -c 288 -i admin.acme.com -p 8086" << std::endl;
    std::cerr << "" << std::endl;
    std::cerr << "NOTE: this is the cgroup-aware fork of original njmon software (see https://github.com/f18m/cmonitor)"
              << std::endl;

    exit(0);
}

void CMonitorCollectorApp::init_defaults()
{
    if (getenv("CMONITOR_SECRET"))
        g_cfg.m_strRemoteSecret = getenv("CMONITOR_SECRET");

    // output file names
    get_hostname();

    time_t timer; /* used to work out the time details*/
    struct tm* tim = nullptr; /* used to work out the local hour/min/second */

    timer = time(0);
    tim = localtime(&timer);
    tim->tm_year += 1900; /* read localtime() manual page!! */
    tim->tm_mon += 1; /* because it is 0 to 11 */

    char filename[1024];
    sprintf(filename, "%s_%02d%02d%02d_%02d%02d", m_strShortHostname.c_str(), tim->tm_year, tim->tm_mon, tim->tm_mday,
        tim->tm_hour, tim->tm_min);
    g_cfg.m_strOutputFilenamePrefix = filename;
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
                if (!string2int(optarg, g_cfg.m_nSamplingInterval)) {
                    printf("Unrecognized sampling interval: %s\n", optarg);
                    exit(51);
                }
                if (g_cfg.m_nSamplingInterval == 0) // safety check
                    g_cfg.m_nSamplingInterval = 1;
                break;
            case 'c':
                if (strcmp(optarg, "until-cgroup-alive") == 0)
                    g_cfg.m_nSamples = SPECIAL_NUMSAMPLES_UNTIL_CGROUP_ALIVE;
                else {
                    if (!string2int(optarg, g_cfg.m_nSamples)) {
                        printf("Unrecognized number of samples to collect: %s\n", optarg);
                        exit(51);
                    }
                }
                break;
            case 'k':
                g_cfg.m_bAllowMultipleInstances = true;
                break;
            case 'C': {
                std::vector<std::string> tokens = split_string_in_array(optarg, ',');
                g_cfg.m_nCollectFlags = 0;
                for (auto token : tokens) {
                    PerformanceKpiFamily k = string2PerformanceKpiFamily(token);
                    if (k == PK_INVALID) {
                        printf("Unrecognized performance statistics family provided: %s\n", token.c_str());
                        exit(51);
                    }
                    g_cfg.m_nCollectFlags |= k;
                }
            } break;
            case 'e':
                g_cfg.m_nOutputFields = PF_ALL;
                break;
            case 'F':
                g_cfg.m_bForeground = true;
                break;
            case 'g':
                g_cfg.m_strCGroupName = optarg;
                break;

                // Local data saving options
            case 'm':
                g_cfg.m_strOutputDir = optarg;
                break;
            case 'f': {
                g_cfg.m_strOutputFilenamePrefix = optarg;

                // if the filename contains the JSON extension, remove it
                size_t nchars = g_cfg.m_strOutputFilenamePrefix.size();
                if (nchars > 5 && g_cfg.m_strOutputFilenamePrefix.substr(nchars - 5) == ".json")
                    g_cfg.m_strOutputFilenamePrefix = g_cfg.m_strOutputFilenamePrefix.substr(0, nchars - 5);
            } break;
            case 'P':
                g_output.enable_json_pretty_print();
                break;

                // Remote data collector options
            case 'i':
                g_cfg.m_strRemoteAddress = optarg;
                break;
            case 'p':
                if (!string2int(optarg, g_cfg.m_nRemotePort)) {
                    printf("Unrecognized remote port: %s\n", optarg);
                    exit(51);
                }
                break;
            case 'X':
                g_cfg.m_strRemoteSecret = optarg;
                break;
            case 'D':
                g_cfg.m_strRemoteDatabaseName = optarg;
                break;

            // help
            case 'v':
                printf("cmonitor_collector version: %s\n", VERSION_STRING);
                exit(0);
                break;
            case 'd':
                g_cfg.m_bDebug = true;
                g_cfg.m_bForeground = true; // stay in foreground!
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

    if (!g_cfg.m_strRemoteAddress.empty() && g_cfg.m_nRemotePort <= 0) {
        printf("Option --remote-ip=%s provided but the --remote-port option was not provided\n",
            g_cfg.m_strRemoteAddress.c_str());
        exit(52);
    }
    if (g_cfg.m_strRemoteAddress.empty() && g_cfg.m_nRemotePort > 0) {
        printf("Option --remote-port=%lu provided but the --remote-ip option was not provided\n", g_cfg.m_nRemotePort);
        exit(53);
    }

    optind = 0; /* reset getopt lib */
}

//------------------------------------------------------------------------------
// Application core functions
//------------------------------------------------------------------------------

std::string CMonitorCollectorApp::get_hostname()
{
    DEBUGLOG_FUNCTION_START();
    if (!m_strHostname.empty())
        return m_strHostname;

    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) != 0)
        m_strHostname = "unknown-hostname";
    else
        m_strHostname = hostname;

    for (size_t i = 0; i < strlen(hostname); i++)
        if (hostname[i] == '.')
            break;
        else
            m_strShortHostname.push_back(hostname[i]);

    return m_strHostname;
}

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
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1.0e-6;
}

void CMonitorCollectorApp::psample_date_time(long loop)
{
    DEBUGLOG_FUNCTION_START();

    std::string localTime, utcTime;
    get_timestamps(localTime, utcTime);

    g_output.psection_start("timestamp");
    g_output.pstring("datetime", localTime.c_str());
    g_output.pstring("UTC", utcTime.c_str());
    g_output.plong("sample_index", loop);
    g_output.psection_end();
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
    if (!g_cfg.m_bAllowMultipleInstances)
        check_pid_file();

    if (!g_cfg.m_strOutputDir.empty()) {
        if (chdir(g_cfg.m_strOutputDir.c_str()) == -1) {
            perror("Change Directory failed");
            fprintf(stderr, "Directory attempted was: %s\n", g_cfg.m_strOutputDir.c_str());
            exit(11);
        } else {
            printf("Changed to directory: %s\n", g_cfg.m_strOutputDir.c_str());
        }
    }

    // init debug/error channels:
    g_logger.init_error_output_file(g_cfg.m_strOutputFilenamePrefix);

    // init the output channels:
    g_output.init_json_output_file(g_cfg.m_strOutputFilenamePrefix);
    if (!g_cfg.m_strRemoteAddress.empty() && g_cfg.m_nRemotePort != 0) {
        /* We are attempting sending the data remotely */
        g_output.init_influxdb_connection(g_cfg.m_strRemoteAddress, g_cfg.m_nRemotePort, g_cfg.m_strRemoteDatabaseName);
    }

    if (!g_cfg.m_bForeground) {
        assert(!g_cfg.m_bDebug); // in debug mode we enable foreground mode!

        /* disconnect from terminal */
        g_logger.LogDebug("Forking for daemonization");
        pid_t childpid;
        if ((childpid = fork()) != 0) {
            exit(0); /* parent returns OK */
        }

        g_logger.LogDebug("Running in daemon process:\n");

        // close default file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        setpgrp(); /* become process group leader */
        signal(SIGHUP, SIG_IGN); /* ignore hangups */
    }

    // init incremental stats (don't write yet anything!)
    bool bCollectCGroupInfo = // force newline
        (g_cfg.m_nCollectFlags & PK_CGROUP_CPU_ACCT) || // force newline
        (g_cfg.m_nCollectFlags & PK_CGROUP_MEMORY) || // force newline
        (g_cfg.m_nCollectFlags & PK_CGROUP_BLKIO) || // force newline
        (g_cfg.m_nCollectFlags & PK_CGROUP_PROCESSES); // force newline
    proc_stat(0, bCollectCGroupInfo, PF_NONE /* do not emit JSON data */);
    proc_diskstats(0, PF_NONE /* do not emit JSON data */);
    proc_net_dev(0, PF_NONE /* do not emit JSON data */);
    if (bCollectCGroupInfo) {
        cgroup_init();

        if (g_cfg.m_nCollectFlags & PK_CGROUP_CPU_ACCT)
            cgroup_proc_cpuacct(0, false /* do not emit JSON */);

        if (g_cfg.m_nCollectFlags & PK_CGROUP_PROCESSES)
            cgroup_proc_tasks(0, PF_NONE /* do not emit JSON */);
    }

    double current_time = get_timestamp_sec();

    // write stuff that is present only in the very first sample (never changes):
    g_output.pheader_start();
    header_identity();
    header_cmonitor_info(argc, argv, g_cfg.m_nSamplingInterval, g_cfg.m_nSamples, g_cfg.m_nCollectFlags);
    header_etc_os_release();
    header_version();
    if (bCollectCGroupInfo)
        cgroup_config(); // needs to run _BEFORE_ lscpu() and proc_cpuinfo()
    header_lscpu();
    header_cpuinfo(); // ?!? this file contains basically the same info contained in lscpu output ?!?
    header_meminfo();
    header_lshw();
    g_output.push_header();

    /* first time just sleep(1) so the first snapshot has some real-ish data */
    if (g_cfg.m_nSamplingInterval <= 60)
        sleep(g_cfg.m_nSamplingInterval);
    else
        sleep(60); /* if a long time between snapshot do a quick one now so we have one in the bank */

    std::set<std::string> charted_stats_from_meminfo;
    if (g_cfg.m_nOutputFields == PF_USED_BY_CHART_SCRIPT_ONLY) {
        charted_stats_from_meminfo.insert("MemTotal");
        charted_stats_from_meminfo.insert("MemFree");
        charted_stats_from_meminfo.insert("Cached");
    }
    // else: leave empty

    std::set<std::string> charted_stats_from_cgroup_memory;
    if (g_cfg.m_nOutputFields == PF_USED_BY_CHART_SCRIPT_ONLY) {
        charted_stats_from_cgroup_memory.insert("total_cache");
        charted_stats_from_cgroup_memory.insert("total_rss");
        charted_stats_from_cgroup_memory.insert("failcnt");
    }
    // else: leave empty

    // start actual data samples:
    g_logger.LogDebug("Starting sampling of performance data; collect flags=%lu", g_cfg.m_nCollectFlags);
    g_output.psample_array_start();
    for (unsigned int loop = 0; g_cfg.m_nSamples == 0 || loop < g_cfg.m_nSamples; loop++) {
        if (loop != 0)
            sleep(g_cfg.m_nSamplingInterval);

        /* calculate elapsed time to include sleep and data collection time */
        double previous_time = current_time;
        current_time = get_timestamp_sec();
        double elapsed = current_time - previous_time;

        g_output.psample_start();

        // some stats are always collected, regardless of g_cfg.m_nCollectFlags
        psample_date_time(loop);
        // proc_uptime(); // not really useful!!
        proc_loadavg();

        // baremetal stats:

        if (g_cfg.m_nCollectFlags & PK_CPU) {
            proc_stat(elapsed, false /* collect from ALL cpus */, g_cfg.m_nOutputFields /* emit JSON */);
        }

        if (g_cfg.m_nCollectFlags & PK_MEMORY) {
            proc_read_numeric_stats_from("meminfo", charted_stats_from_meminfo);
            if (g_cfg.m_nOutputFields == PF_ALL)
                proc_read_numeric_stats_from("vmstat", std::set<std::string>());
        }

        if (g_cfg.m_nCollectFlags & PK_NETWORK) {
            proc_net_dev(elapsed, g_cfg.m_nOutputFields /* emit JSON */);
        }

        if (g_cfg.m_nCollectFlags & PK_DISK) {
            proc_diskstats(elapsed, g_cfg.m_nOutputFields /* emit JSON */);
            // proc_filesystems(); // I don't find this really useful...specially for ephemeral containers!
        }

        // cgroup stats:

        if (g_cfg.m_nCollectFlags & PK_CGROUP_CPU_ACCT) {
            // do not list all CPU informations when cgroup mode is ON: don't put information
            // for CPUs outside current cgroup!
            cgroup_proc_cpuacct(elapsed, true /* emit JSON */);
            cgroup_proc_cpu_throttle();
        }

        if (g_cfg.m_nCollectFlags & PK_CGROUP_MEMORY) {
            cgroup_proc_memory(charted_stats_from_cgroup_memory);
        }
        if (g_cfg.m_nCollectFlags & PK_CGROUP_PROCESSES) {
            cgroup_proc_tasks(elapsed, g_cfg.m_nOutputFields /* emit JSON */);
        }

        g_output.push_current_sample();

        if (g_bExiting)
            break; // graceful exit allows to produce a valid JSON on SIGTERM signals!
        if (g_cfg.m_nSamples == SPECIAL_NUMSAMPLES_UNTIL_CGROUP_ALIVE && !cgroup_still_exists())
            break;
    }

    /* finish-of */
    g_output.psample_array_end();
    fflush(NULL);

    g_logger.LogDebug("Exiting gracefully with return code 0");
    return 0;
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // init defaults (can be overridden by cmd line options):

    g_app.init_defaults();

    // parse cmd line:

    g_app.parse_args(argc, argv);

    signal(SIGTERM, interrupt);
    signal(SIGINT, interrupt);
    signal(SIGUSR1, interrupt);
    signal(SIGUSR2, interrupt);

    // run:

    return g_app.run(argc, argv);
}
