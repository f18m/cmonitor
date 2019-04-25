//---------------------------------------------------------------------------
// MsTableUtil.cpp:
// (C) Copyright 2018 Empirix Inc.
//
//  Created on: Oct 2018
//      Author: fmontorsi
//     Purpose: Command-line utility to interact with the IS2.0 Redis MsTable
//---------------------------------------------------------------------------

#include <arpa/inet.h>
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
// Globals
//------------------------------------------------------------------------------

#define PRINT_FALSE 0
#define PRINT_TRUE 1

#define PID_FILE "/var/run/njmon.pid"

#define ADDITIONAL_HELP_COLUMN_START (30)

#define DEBUGLOG_FUNCTION_START()                                                                                      \
    if (g_cfg.m_bDebug)                                                                                                \
        fprintf(stderr, "%s called line %d\n", __func__, __LINE__);

// app-wide config settings:
NjmonCollectorAppConfig g_cfg;
NjmonCollectorApp g_app;

//------------------------------------------------------------------------------
// Command Line Globals
//------------------------------------------------------------------------------

struct option opts[] = {
    // Data sampling options
    { "sampling-interval", required_argument, 0, 's' }, // force newline
    { "num-samples", required_argument, 0, 'c' }, // force newline
    { "output-directory", required_argument, 0, 'm' }, // force newline
    { "output-filename", required_argument, 0, 'f' }, // force newline
    { "allow-multiple-instances", no_argument, 0, 'k' }, // force newline
    { "collect", required_argument, 0, 'C' }, // force newline
    { "foreground", no_argument, 0, 'F' }, // force newline

    // Remote data collection options
    { "remote-ip", required_argument, 0, 'i' }, // force newline
    { "remote-port", required_argument, 0, 'p' }, // force newline
    { "remote-secret", required_argument, 0, 'X' }, // force newline

    // help
    { "help", no_argument, 0, 'h' }, // force newline
    { 0, 0, 0, 0 }
};

struct option_extended {
    const char* section_name;
    struct option* opt_descriptor;
    const char* additional_help;
} const opts_extended[] = {
    // Data sampling options
    { "Data sampling options", &opts[0], "Seconds between snapshots of data (default 60 seconds)." },
    { "Data sampling options", &opts[1], "Number of snapshots (default forever)." },
    { "Data sampling options", &opts[2], "Program will write output files to provided directory (default cwd)." },
    { "Data sampling options", &opts[3],
        "Name the output file as provided instead of defaulting to the filenames:\n"
        "\t\tData:   hostname_<year><month><day>_<hour><minutes>.json\n"
        "\t\tErrors: hostname_<year><month><day>_<hour><minutes>.err\n" },
    { "Data sampling options", &opts[4], "Allow multiple instances of njmon_collector to run on this system." },
    { "Data sampling options", &opts[5], "Collect specified list of performance KPIs (TODO WIP)." },
    { "Data sampling options", &opts[6], "Stay in foreground." },

    // Remote data collection options
    { "Remote data collection options", &opts[7], "IP address or hostname of the njmon central collector." },
    { "Remote data collection options", &opts[8], "Port number on collector host." },
    { "Remote data collection options", &opts[9],
        "Set the remote collector secret (by default use environment variable NJMON_SECRET)." },

    // help
    { "Other options", &opts[10], "Show this help" },

    { NULL, NULL, NULL }
};

//------------------------------------------------------------------------------
// Signals
//------------------------------------------------------------------------------

int exit_flag = 0;

void exit_interrupt(int signum) // another addition compared to original source to produce correct JSON on SIGTERM
{
    switch (signum) {
    case SIGTERM:
        exit_flag = 1;
        break;
    }
}

void interrupt(int signum)
{
    switch (signum) {
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

void LogDebug(const char* line, ...)
{
    char currLogLine[256];

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    if (g_cfg.m_bDebug)
        printf("%s\n", currLogLine);
}

void LogError(const char* line, ...)
{
    char currLogLine[256];

    va_list args;
    va_start(args, line);
    vsnprintf(currLogLine, 255, line, args);
    va_end(args);

    printf("ERROR: %s\n", currLogLine);
}

//------------------------------------------------------------------------------
// Command line functions
//------------------------------------------------------------------------------

void NjmonCollectorApp::print_help()
{
    static_assert(sizeof(opts_extended) / sizeof(opts_extended[0]) == sizeof(opts) / sizeof(opts[0]),
        "Mismatching number of options");

    std::cerr << "njmon_collector: Performance stats collector outputting JSON format." << std::endl;
    std::cerr << "List of arguments that can be provided follows:" << std::endl;
    std::cerr << std::endl;

    std::string last_sec_name;
    for (int i = 0;; i++) {
        const struct option* opt = opts_extended[i].opt_descriptor;
        if (!opt)
            break;

        if (opts_extended[i].section_name != last_sec_name) {
            std::cerr << opts_extended[i].section_name << std::endl;
            last_sec_name = opts_extended[i].section_name;
        }

        std::stringstream help;
        help << "  --" << opt->name;

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

        std::string additional_help(opts_extended[i].additional_help);
        std::cerr << help.str() << additional_help << std::endl;
    }

    std::cerr << "" << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "    1 Every 5 mins all day" << std::endl;
    std::cerr << "\tnjmon_collector -s 300 -c 288 -f -m /home/perf" << std::endl;
    std::cerr << "    2 Piping to data handler using half a day of data" << std::endl;
    std::cerr << "\tnjmon_collector -s 30 -c 1440 | myprog" << std::endl;
    std::cerr << "    3 Use the defaults (-s 60 forever) and save to a file " << std::endl;
    std::cerr << "\tnjmon_collector >my_server_today.json" << std::endl;
    std::cerr << "    4 Crontab entry" << std::endl;
    std::cerr << "\t0 4 * * * njmon_collector -s 300 -c 288 -f -m /home/perf" << std::endl;
    // std::cerr << "    5 Crontab - hourly check/restart remote njmon, pipe stats back & insert into local DB" <<
    // std::endl; std::cerr << "\t* 0 * * * /usr/bin/ssh nigel@server /usr/bin/njmon_collector -s 300 -c 288 |
    // /bin/injector" << std::endl;
    std::cerr << "    6 Crontab - for pumping data to the njmon central collector" << std::endl;
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
    get_time();
    get_localtime();
    char filename[1024];
    sprintf(filename, "%s_%02d%02d%02d_%02d%02d", m_strShortHostname.c_str(), tim->tm_year, tim->tm_mon, tim->tm_mday,
        tim->tm_hour, tim->tm_min);
    g_cfg.m_strOutputFilename = filename;
}

void NjmonCollectorApp::parse_args(int argc, char** argv)
{
    while (true) {
        int c = getopt_long(argc, argv, "", opts, 0);
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
                g_cfg.m_strOutputFilename = optarg;
                break;
            case 'k':
                g_cfg.m_bAllowMultipleInstances = true;
                break;
            case 'C':
                // g_cfg.m_nCollectFlags = ;
                break;
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

    optind = 0; /* reset getopt lib */
}

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

void NjmonCollectorApp::get_time() { timer = time(0); }

void NjmonCollectorApp::get_localtime()
{
    tim = localtime(&timer);
    tim->tm_year += 1900; /* read localtime() manual page!! */
    tim->tm_mon += 1; /* because it is 0 to 11 */
}

void NjmonCollectorApp::get_utc()
{
    tim = gmtime(&timer);
    tim->tm_year += 1900; /* read gmtime() manual page!! */
    tim->tm_mon += 1; /* because it is 0 to 11 */
}

void NjmonCollectorApp::date_time(long seconds, long loop, long maxloops)
{
    char buffer[256];

    DEBUGLOG_FUNCTION_START();
    /* This is ISO 8601 datatime string format - ughly but get over it! :-) */
    get_time();
    get_localtime();
    psection("timestamp");
    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year, tim->tm_mon, tim->tm_mday, tim->tm_hour, tim->tm_min,
        tim->tm_sec);
    pstring("datetime", buffer);
    get_utc();
    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year, tim->tm_mon, tim->tm_mday, tim->tm_hour, tim->tm_min,
        tim->tm_sec);
    pstring("UTC", buffer);
    plong("snapshot_seconds", seconds);
    plong("snapshot_maxloops", maxloops);
    plong("snapshot_loop", loop);
    psectionend();
}

/*    check_pid_file() and make_pid_file()
 *    If you start njmon and it finds there is a copy running already then it will quietly stop.
 *    You can hourly start njmon via crontab and not end up with dozens of copies running.
 *    It also means if the server reboots then njmon start in the next hour.
 *    Side-effect: it creates a file called /tmp/njmon.pid
 */

void NjmonCollectorApp::check_pid_file()
{
    // immediately stop running if another instance of this software is already running.
    // Note that hmmonitor creates its own /var/run/ProcName.pid to track current PID,
    // don't mess with it! Instead here an empty LOCKED file is created with .lock extension
    int pid_file = open(PID_FILE, O_CREAT | O_RDWR, 0666);
    int rc = flock(pid_file, LOCK_EX | LOCK_NB);
    if (rc && EWOULDBLOCK == errno) {
        fprintf(stderr, "%s: another instance is already running...aborting.\n", PID_FILE);
        exit(-1);
    }
    // else: this is the first instance of this software... continue
}

void NjmonCollectorApp::file_read_one_stat(const char* file, const char* name)
{
    FILE* fp;
    char buf[1024 + 1];

    if ((fp = fopen(file, "r")) != NULL) {
        if (fgets(buf, 1024, fp) != NULL) {
            if (buf[strlen(buf) - 1] == '\n') /* remove last char = newline */
                buf[strlen(buf) - 1] = 0;
            pstring(name, buf);
        }
        fclose(fp);
    }
}

void NjmonCollectorApp::identity_and_njmon(int argc, char** argv)
{
    int i;

    /* hostname */
    char label[512];
    struct addrinfo hints;
    struct addrinfo* info;
    struct addrinfo* p;

    /* user name and id */
    struct passwd* pw;
    uid_t uid;

    /* network IP addresses */
    struct ifaddrs* interfaces = NULL;
    struct ifaddrs* ifaddrs_ptr = NULL;
    char address_buf[INET6_ADDRSTRLEN];
    char* str;

    DEBUGLOG_FUNCTION_START();

    psection("identity");
    get_hostname();
    pstring("hostname", m_strHostname.c_str());
    pstring("shorthostname", m_strShortHostname.c_str());

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    char hostname[1024];
    if (getaddrinfo(hostname, "http", &hints, &info) == 0) {
        for (p = info, i = 1; p != NULL; p = p->ai_next, i++) {
            sprintf(label, "fullhostname%d", i);
            pstring(label, p->ai_canonname);
        }
    }

    if (getifaddrs(&interfaces) == 0) { /* retrieve the current interfaces */
        for (ifaddrs_ptr = interfaces; ifaddrs_ptr != NULL; ifaddrs_ptr = ifaddrs_ptr->ifa_next) {

            if (ifaddrs_ptr->ifa_addr) {
                switch (ifaddrs_ptr->ifa_addr->sa_family) {
                case AF_INET:
                    if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                             &((struct sockaddr_in*)ifaddrs_ptr->ifa_addr)->sin_addr, address_buf, sizeof(address_buf)))
                        != NULL) {
                        sprintf(label, "%s_IP4", ifaddrs_ptr->ifa_name);
                        pstring(label, str);
                    }
                    break;
                case AF_INET6:
                    if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                             &((struct sockaddr_in6*)ifaddrs_ptr->ifa_addr)->sin6_addr, address_buf,
                             sizeof(address_buf)))
                        != NULL) {
                        sprintf(label, "%s_IP6", ifaddrs_ptr->ifa_name);
                        pstring(label, str);
                    }
                    break;
                default:
                    // sprintf(label,"%s_Not_Supported_%d", ifaddrs_ptr->ifa_name, ifaddrs_ptr->ifa_addr->sa_family);
                    // pstring(label,"");
                    break;
                }
            } else {
                sprintf(label, "%s_network_ignored", ifaddrs_ptr->ifa_name);
                pstring(label, "null_address");
            }
        }

        freeifaddrs(interfaces); /* free the dynamic memory */
    }

    /* POWER and AMD and may be others */
    if (access("/proc/device-tree", R_OK) == 0) {
        file_read_one_stat("/proc/device-tree/compatible", "compatible");
        file_read_one_stat("/proc/device-tree/model", "model");
        file_read_one_stat("/proc/device-tree/part-number", "part-number");
        file_read_one_stat("/proc/device-tree/serial-number", "serial-number");
        file_read_one_stat("/proc/device-tree/system-id", "system-id");
        file_read_one_stat("/proc/device-tree/vendor", "vendor");
    }
    /*x86_64 and AMD64 */
    if (access("/sys/devices/virtual/dmi/id/", R_OK) == 0) {
        file_read_one_stat("/sys/devices/virtual/dmi/id/product_serial", "serial-number");
        file_read_one_stat("/sys/devices/virtual/dmi/id/product_name", "model");
        file_read_one_stat("/sys/devices/virtual/dmi/id/sys_vendor", "vendor");
    }
    psectionend();

    psection("njmon");
    {
        char command[1024] = { 0 };
        for (i = 0; i < argc; i++) {
            strcat(command, argv[i]);
            if (i != argc - 1)
                strcat(command, " ");
        }

        pstring("njmon_command", command);
    }
    pstring("njmon_version", VERSION_STRING);
    uid = geteuid();
    if ((pw = getpwuid(uid)) != NULL) {
        pstring("username", pw->pw_name);
        plong("userid", uid);
    } else {
        pstring("username", "unknown");
    }
    psectionend();
}

int NjmonCollectorApp::run(int argc, char** argv)
{
    // if only one instance allowed, do the check:
    if (!g_cfg.m_bAllowMultipleInstances)
        check_pid_file();

    if (!g_cfg.m_strRemoteAddress.empty()
        && g_cfg.m_nRemotePort != 0) { /* We are attempting sending the data remotely */

        struct hostent* he = gethostbyname(g_cfg.m_strRemoteAddress.c_str());
        if (he == NULL) {
            printf("hostname=%s to IP address convertion failed, bailing out\n", g_cfg.m_strRemoteAddress.c_str());
            exit(98);
        }
        /*
                printf("name=%s\n",he->h_name);
                printf("type=%d = ",he->h_addrtype);
                switch(he->h_addrtype) {
                        case AF_INET: printf("IPv4\n"); break;
                        case AF_INET6: printf("(IPv6\n"); break;
                        default: printf("unknown\n");
                }
                printf("length=%d\n",he->h_length);
        */

        /* this could return multiple IP addresses but we assume its the first one */
        std::string host;
        if (he->h_addr_list[0] != NULL) {
            host = inet_ntoa(*(struct in_addr*)(he->h_addr_list[0]));
        } else {
            printf("hostname=%s to IP address convertion failed, bailing out\n", g_cfg.m_strRemoteAddress.c_str());
            exit(99);
        }
        /*
                get_hostname();
                get_time();
                get_utc();
                sprintf(datastring, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year, tim->tm_mon, tim->tm_mday,
           tim->tm_hour, tim->tm_min, tim->tm_sec); create_socket(host, port, hostname, datastring, secret);*/
    }

    if (!g_cfg.m_strOutputDir.empty()) {
        if (chdir(g_cfg.m_strOutputDir.c_str()) == -1) {
            perror("Change Directory failed");
            printf("Directory attempted was: %s\n", g_cfg.m_strOutputDir.c_str());
            exit(11);
        } else {
            printf("Changed to directory: %s\n", g_cfg.m_strOutputDir.c_str());
        }
    }

    // open output files
    char filename[1024];
    sprintf(filename, "%s.json", g_cfg.m_strOutputFilename.c_str());
    if ((m_outputJson = fopen(filename, "w")) == 0) {
        perror("opening file for stdout");
        fprintf(stderr, "ERROR nmon filename=%s\n", filename);
        exit(13);
    }

    printf("Opened output JSON file '%s'\n", filename);

    sprintf(filename, "%s.err", g_cfg.m_strOutputFilename.c_str());
    if ((m_outputErr = fopen(filename, "w")) == 0) {
        perror("opening file for stderr");
        fprintf(stderr, "ERROR nmon filename=%s\n", filename);
        exit(14);
    }

    printf("Opened output error file '%s'\n", filename);

    fflush(NULL);

    if (!g_cfg.m_bForeground) {
        /* disconnect from terminal */
        LogDebug("forking for daemon");
        __pid_t childpid;
        if ((childpid = fork()) != 0) {
            exit(0); /* parent returns OK */
        }

        LogDebug("child running\n");
        if (!g_cfg.m_bDebug) {
            /*	close(0);
                            close(1);
                            close(2);
            */
            setpgrp(); /* become process group leader */
            signal(SIGHUP, SIG_IGN); /* ignore hangups */
        }
    }

    // allocate output buffer
    buffer_check();
#if 0
    commlen = 1; /* for the terminating zero */
    for (i = 0; i < argc; i++) {
        commlen = commlen + strlen(argv[i]) + 1; /* +1 for spaces */
    }
    command = malloc(commlen);
    command[0] = 0;
    for (i = 0; i < argc; i++) {
        strcat(command, argv[i]);
        if (i != (argc - 1))
            strcat(command, " ");
    }
#endif

    /* seed incrementing counters */
    proc_stat(0, PRINT_FALSE);
    proc_diskstats(0, PRINT_FALSE);
    proc_net_dev(0, PRINT_FALSE);
    if (g_cfg.m_nCollectFlags & PK_CGROUPS)
        cgroup_init();

    struct timeval tv;
    gettimeofday(&tv, 0);

    double current_time = (double)tv.tv_sec + (double)tv.tv_usec * 1.0e-6;

    /* first time just sleep(1) so the first snapshot has some real-ish data */
    if (g_cfg.m_nSamplingInterval <= 60)
        sleep(g_cfg.m_nSamplingInterval);
    else
        sleep(60); /* if a long time between snapshot do a quick one now so we have one in the bank */

    /* pre-amble */
    pstart();
    praw("  \"samples\": [\n");

    unsigned int seconds = 0;
    for (unsigned int loop = 0; g_cfg.m_nSamples == 0 || loop < g_cfg.m_nSamples; loop++) {
        psample();
        if (loop != 0)
            sleep(g_cfg.m_nSamplingInterval);

        /* calculate elapsed time to include sleep and data collection time */
        double previous_time = current_time;
        gettimeofday(&tv, 0);
        current_time = (double)tv.tv_sec + ((double)tv.tv_usec * 1.0e-6);
        double elapsed = current_time - previous_time;

        date_time(seconds, loop, g_cfg.m_nSamples);

        if (loop == 0) {
            identity_and_njmon(argc, argv);
            etc_os_release();
            proc_version();
            lscpu();
            if (g_cfg.m_nCollectFlags & PK_CGROUPS)
                cgroup_config();
        }
        // else: avoid repeating stats that will never change!

        proc_stat(elapsed, PRINT_TRUE);
        // proc_uptime(); // not really useful!!
        proc_loadavg();

        if (g_cfg.m_nCollectFlags & PK_CPU) {
            if (g_cfg.m_nCollectFlags & PK_CGROUPS) {
                // do not list all CPU informations when cgroup mode is ON: don't put information
                // for CPUs outside current cgroup!
                cgroup_proc_cpuacct(elapsed);

                // collect memory stats for current cgroup:
                cgroup_proc_memory();
            } else {
                // list all CPUs
                proc_cpuinfo();
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
            proc_filesystems();
        }

        psampleend(loop == (g_cfg.m_nSamples - 1) || exit_flag);
        push();

        if (exit_flag)
            break; // graceful exit allows to produce a valid JSON on SIGTERM signals!
    }

    /* finish-of */
    remove_ending_comma_if_any();
    praw(" ]\n");
    pfinish();
    push();
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

    // check arguments we just parsed:

    if (!g_cfg.m_strRemoteAddress.empty() && g_cfg.m_nRemotePort <= 0) {
        printf("Option -i %s provided but not the -p port option\n", g_cfg.m_strRemoteAddress.c_str());
        exit(52);
    }
    if (g_cfg.m_strRemoteAddress.empty() && g_cfg.m_nRemotePort > 0) {
        printf("Option -p %ud provided but not the -i ip-address option\n", g_cfg.m_nRemotePort);
        exit(53);
    }

    signal(SIGTERM, exit_interrupt);
    signal(SIGUSR1, interrupt);
    signal(SIGUSR2, interrupt);

    // run:

    return g_app.run(argc, argv);
}
