/*
 * njmon_main.cpp: core routines for "njmon_collector"
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

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <iostream>
#include <memory.h>
#include <mntent.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

#include "njmon.h"

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define PRINT_FALSE 0
#define PRINT_TRUE 1

#define PID_FILE "/var/run/njmon.pid"

#define ADDITIONAL_HELP_COLUMN_START (40)

//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------

NjmonLoggerUtils g_logger;
NjmonCollectorAppConfig g_cfg;
NjmonCollectorApp g_app;
bool g_bExiting = false;

//------------------------------------------------------------------------------
// Command Line Globals
//------------------------------------------------------------------------------

struct option g_long_opts[] = {
    // Data sampling options
    { "sampling-interval", required_argument, 0, 's' }, // force newline
    { "num-samples", required_argument, 0, 'c' }, // force newline
    { "output-directory", required_argument, 0, 'm' }, // force newline
    { "output-filename", required_argument, 0, 'f' }, // force newline
    { "allow-multiple-instances", no_argument, 0, 'k' }, // force newline
    { "foreground", no_argument, 0, 'F' }, // force newline
    { "collect", required_argument, 0, 'C' }, // force newline

    // Remote data collection options
    { "remote-ip", required_argument, 0, 'i' }, // force newline
    { "remote-port", required_argument, 0, 'p' }, // force newline
    { "remote-secret", required_argument, 0, 'X' }, // force newline

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
    { "Data sampling options", &g_long_opts[0], "Seconds between snapshots of data (default 60 seconds)." },
    { "Data sampling options", &g_long_opts[1], "Number of snapshots; special value 0 means forever (default is 0)." },
    { "Data sampling options", &g_long_opts[2],
        "Program will write output files to provided directory (default cwd)." },
    { "Data sampling options", &g_long_opts[3],
        "Name the output files using provided prefix instead of defaulting to the filenames:\n"
        "\thostname_<year><month><day>_<hour><minutes>.json  (for JSON data)\n"
        "\thostname_<year><month><day>_<hour><minutes>.err   (for error log)\n"
        "Use special prefix 'stdout' to indicate that you want the utility to write on stdout." },
    { "Data sampling options", &g_long_opts[4], "Allow multiple instances of njmon_collector to run on this system." },
    { "Data sampling options", &g_long_opts[5], "Stay in foreground." },
    { "Data sampling options", &g_long_opts[6],
        "Collect specified list of performance stats. Available performance stats are:\n"
        "  cpu: collect per-core CPU stats from /proc/stat                [cgroup-mode: also 'cpuacct']\n"
        "  memory: collect memory stats from /proc/meminfo, /proc/vmstat  [cgroup-mode: also 'memory']\n"
        "  disk: collect disk stats from /proc/diskstats                  [cgroup'mode: also 'blkio']\n"
        "  network: collect network stats from /proc/net/dev\n"
        "  cgroups: activates cgroup-mode\n" // force newline
        "  all: the combination of all previous stats (this is the default)\n"
        "Note that a comma-separated list of above stats can be provided." },

    // Remote data collection options
    { "Remote data collection options", &g_long_opts[7], "IP address or hostname of the njmon central collector." },
    { "Remote data collection options", &g_long_opts[8], "Port number on collector host." },
    { "Remote data collection options", &g_long_opts[9],
        "Set the remote collector secret (by default use environment variable NJMON_SECRET)." },

    // help
    { "Other options", &g_long_opts[10], "Show version and exit" }, // force newline
    { "Other options", &g_long_opts[11],
        "Enable debug mode; automatically activates --foreground mode" }, // force newline
    { "Other options", &g_long_opts[12], "Show this help" },

    { NULL, NULL, NULL }
};

//------------------------------------------------------------------------------
// Signals
//------------------------------------------------------------------------------

void interrupt(int signum)
{
    switch (signum) {
    case SIGTERM:
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
    if (to_lower(str) == "cgroups")
        return PK_CGROUPS;
    if (to_lower(str) == "disk")
        return PK_DISK;
    if (to_lower(str) == "cpu")
        return PK_CPU;
    if (to_lower(str) == "memory")
        return PK_MEMORY;
    if (to_lower(str) == "network")
        return PK_NETWORK;
    if (to_lower(str) == "all")
        return PK_ALL;

    return PK_INVALID;
}

std::string string2PerformanceKpiFamily(PerformanceKpiFamily k)
{
    switch (k) {
    case PK_CGROUPS:
        return "cgroups";
    case PK_CPU:
        return "cpu";
    case PK_DISK:
        return "disk";
    case PK_MEMORY:
        return "memory";
    case PK_NETWORK:
        return "network";

    default:
        return "";
    }
}

//------------------------------------------------------------------------------
// Logger functions
//------------------------------------------------------------------------------

void NjmonLoggerUtils::init_error_output_file(const std::string& filenamePrefix)
{
    if (filenamePrefix == "stdout") {
        // open stderr as FILE* as well:
        if ((m_outputErr = fdopen(STDERR_FILENO, "w")) == 0) {
            perror("opening stderr for write");
            exit(13);
        }
    } else {
        // prepare output error file but don't open it yet
        char filename[1024];
        sprintf(filename, "%s.err", filenamePrefix.c_str());
        m_strErrorFileName = filename;
        printf("Errors (if any) will be logged into the file '%s'\n", m_strErrorFileName.c_str());
    }

    fflush(NULL);
}

void NjmonLoggerUtils::LogDebug(const char* line, ...)
{
    char currLogLine[256];

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    if (g_cfg.m_bDebug) {
        // in debug mode stdout is still open, so we can printf:
        printf("%s", currLogLine);
        size_t lastCh = strlen(currLogLine) - 1;
        if (currLogLine[lastCh] != '\n')
            printf("\n");
    }
}

void NjmonLoggerUtils::LogError(const char* line, ...)
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

void NjmonCollectorApp::print_help()
{
    static_assert(sizeof(g_opts_extended) / sizeof(g_opts_extended[0]) == sizeof(g_long_opts) / sizeof(g_long_opts[0]),
        "Mismatching number of options");

    std::cerr << "njmon_collector: Performance stats collector outputting JSON format." << std::endl;
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
    std::cerr << "\tnjmon_collector -s 300 -c 288 -m /home/perf" << std::endl;
    std::cerr << "    2) Pipe to data handler using half a day of data:" << std::endl;
    std::cerr
        << "\tnjmon_collector --sampling-interval=30 --num-samples=1440 --output-filename=stdout --foreground | myprog"
        << std::endl;
    std::cerr << "    3) Use the defaults (-s 60, collect forever), saving to custom file in background:" << std::endl;
    std::cerr << "\tnjmon_collector --output-filename=my_server_today" << std::endl;
    std::cerr << "    4) Crontab entry:" << std::endl;
    std::cerr << "\t0 4 * * * /usr/bin/njmon_collector -s 300 -c 288 -m /home/perf" << std::endl;
    std::cerr << "    5) Crontab entry for pumping data to the njmon central collector:" << std::endl;
    std::cerr << "\t* 0 * * * /usr/bin/njmon_collector -s 300 -c 288 -i admin.acme.com -p 8181 -X SECRET42 "
              << std::endl;
    std::cerr << "" << std::endl;
    std::cerr
        << "NOTE: this is the cgroup-aware fork of original njmon software (https://github.com/f18m/nmon-cgroup-aware)"
        << std::endl;

    exit(0);
}

void NjmonCollectorApp::init_defaults()
{
    if (getenv("NJMON_SECRET"))
        g_cfg.m_strRemoteSecret = getenv("NJMON_SECRET");

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

void NjmonCollectorApp::parse_args(int argc, char** argv)
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
                g_cfg.m_nSamplingInterval = atoi(optarg);
                break;
            case 'c':
                g_cfg.m_nSamples = atoi(optarg);
                break;
            case 'm':
                g_cfg.m_strOutputDir = optarg;
                break;
            case 'f':
                g_cfg.m_strOutputFilenamePrefix = optarg;
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
            case 'F':
                g_cfg.m_bForeground = true;
                break;

                // Remote data collector options
            case 'i':
                g_cfg.m_strRemoteAddress = optarg;
                break;
            case 'p':
                g_cfg.m_nRemotePort = atoi(optarg);
                break;
            case 'X':
                g_cfg.m_strRemoteSecret = optarg;
                break;

            // help
            case 'v':
                printf("njmon_collector version: %s\n", VERSION_STRING);
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
                printf("%s: init failed\n", argv[0]);
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
        printf("Option -i=%s provided but the -p=port option was not provided\n", g_cfg.m_strRemoteAddress.c_str());
        exit(52);
    }
    if (g_cfg.m_strRemoteAddress.empty() && g_cfg.m_nRemotePort > 0) {
        printf("Option -p=%ud provided but the -i=ip-address option was not provided\n", g_cfg.m_nRemotePort);
        exit(53);
    }

    optind = 0; /* reset getopt lib */
}

//------------------------------------------------------------------------------
// Application core functions
//------------------------------------------------------------------------------

std::string NjmonCollectorApp::get_hostname()
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

void NjmonCollectorApp::get_timestamps(std::string& localTime, std::string& utcTime)
{
    time_t timer; /* used to work out the time details*/
    struct tm* tim = nullptr; /* used to work out the local hour/min/second */

    timer = time(0);
    tim = localtime(&timer);
    tim->tm_year += 1900; /* read localtime() manual page!! */
    tim->tm_mon += 1; /* because it is 0 to 11 */

    /* This is ISO 8601 datatime string format - ughly but get over it! :-) */

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

double NjmonCollectorApp::get_timestamp_sec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1.0e-6;
}

void NjmonCollectorApp::psample_date_time(long loop)
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

void NjmonCollectorApp::check_pid_file()
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

int NjmonCollectorApp::run(int argc, char** argv)
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
        g_output.init_influxdb_connection(g_cfg.m_strRemoteAddress, g_cfg.m_nRemotePort);
    }

    if (!g_cfg.m_bForeground) {
        assert(!g_cfg.m_bDebug); // in debug mode we enable foreground mode!

        /* disconnect from terminal */
        g_logger.LogDebug("Forking for daemonization");
        __pid_t childpid;
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
    bool bCollectCGroupInfo = g_cfg.m_nCollectFlags & PK_CGROUPS;
    proc_stat(0, bCollectCGroupInfo, false /* do not emit JSON data */);
    proc_diskstats(0, PRINT_FALSE);
    proc_net_dev(0, PRINT_FALSE);
    if (bCollectCGroupInfo) {
        cgroup_init();
        cgroup_proc_cpuacct(0, false /* do not emit JSON */);
    }

    double current_time = get_timestamp_sec();

    /* first time just sleep(1) so the first snapshot has some real-ish data */
    if (g_cfg.m_nSamplingInterval <= 60)
        sleep(g_cfg.m_nSamplingInterval);
    else
        sleep(60); /* if a long time between snapshot do a quick one now so we have one in the bank */

    /* pre-amble */
    // praw("{\n");

    // write stuff that is present only in the very first sample (never changes):
    // praw("  \"header\": {\n");
    header_identity();
    header_njmon_info(argc, argv, g_cfg.m_nSamplingInterval, g_cfg.m_nSamples, g_cfg.m_nCollectFlags);
    header_etc_os_release();
    header_version();
    if (g_cfg.m_nCollectFlags & PK_CGROUPS)
        cgroup_config(); // needs to run _BEFORE_ lscpu() and proc_cpuinfo()
    header_lscpu();
    header_cpuinfo(); // ?!? this file contains basically the same info contained in lscpu output ?!?
    header_lshw();
    // premove_ending_comma_if_any();
    // praw("  },\n"); // end of "header"

    // start actual data samples:
    // praw("  \"samples\": [\n");
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

        if (g_cfg.m_nCollectFlags & PK_CPU) {
            if (bCollectCGroupInfo) {
                // do not list all CPU informations when cgroup mode is ON: don't put information
                // for CPUs outside current cgroup!
                proc_stat(elapsed, true /* cgroup */, true /* emit JSON */);
                cgroup_proc_cpuacct(elapsed, true /* emit JSON */);
            } else {
                // list all CPUs
                proc_stat(elapsed, false /* cgroup */, true /* emit JSON */);
            }
        }

        if (g_cfg.m_nCollectFlags & PK_MEMORY) {
            read_data_number("meminfo");
            read_data_number("vmstat");
            if (g_cfg.m_nCollectFlags & PK_CGROUPS) {
                // collect memory stats for current cgroup:
                cgroup_proc_memory();
            }
        }

        if (g_cfg.m_nCollectFlags & PK_NETWORK) {
            proc_net_dev(elapsed, PRINT_TRUE);
        }

        if (g_cfg.m_nCollectFlags & PK_DISK) {
            proc_diskstats(elapsed, PRINT_TRUE);
            // proc_filesystems(); // I don't find this really useful...specially for ephemeral containers!
        }

        g_output.psample_end(loop == (g_cfg.m_nSamples - 1) || g_bExiting);
        g_output.push_current_sample();

        if (g_bExiting)
            break; // graceful exit allows to produce a valid JSON on SIGTERM signals!
    }

    /* finish-of */
    // premove_ending_comma_if_any();
    // praw(" ]\n");
    // premove_ending_comma_if_any();
    // praw("}\n");
    g_output.push_current_sample();
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
    signal(SIGUSR1, interrupt);
    signal(SIGUSR2, interrupt);

    // run:

    return g_app.run(argc, argv);
}
