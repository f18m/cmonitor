/*
 * header_info.cpp: routines to generate the initial header
 *                  of information about the server being monitored
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

#include "header_info.h"
#include "logger.h"
#include "output_frontend.h"
#include "system.h"
#include "utils_files.h"
#include "utils_misc.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <sys/types.h>

// ----------------------------------------------------------------------------------
// CMonitorHeaderInfo
// ----------------------------------------------------------------------------------

void CMonitorHeaderInfo::file_read_one_stat(const char* file, const char* name)
{
    FILE* fp;
    char buf[1024 + 1];

    if ((fp = fopen(file, "r")) != NULL) {
        if (fgets(buf, 1024, fp) != NULL) {
            if (buf[strlen(buf) - 1] == '\n') /* remove last char = newline */
                buf[strlen(buf) - 1] = 0;
            m_pOutput->pstring(name, buf);
        }
        fclose(fp);
    }
}

void CMonitorHeaderInfo::header_identity()
{
    int i;

    /* hostname */
    char label[512];
    struct addrinfo hints;
    struct addrinfo* info = NULL;
    struct addrinfo* p = NULL;

    DEBUGLOG_FUNCTION_START();

    m_pOutput->psection_start("identity");
    std::string strFullHostname = get_hostname();
    m_pOutput->pstring("hostname", strFullHostname.c_str());

    // remove everything after first dot:
    std::string shortHostname(strFullHostname);
    size_t dot_pos = shortHostname.find('.');
    if (dot_pos != std::string::npos)
        shortHostname.resize(dot_pos);
    m_pOutput->pstring("shorthostname", shortHostname.c_str());

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    char hostname[1024] = { 0 };
    if (getaddrinfo(hostname, "http", &hints, &info) == 0) {
        for (p = info, i = 1; p != NULL; p = p->ai_next, i++) {
            sprintf(label, "fullhostname%d", i);
            m_pOutput->pstring(label, p->ai_canonname);
        }
    }

    netdevices_map_t netdev;
    if (CMonitorSystem::get_net_dev_list(netdev, true /* include_only_interfaces_up */)) {
        std::string all_ips;
        for (auto entry : netdev) {
            if (entry.first == "lo")
                continue; // skip loopback -- it does not identify a server

            m_pOutput->pstring(entry.first.c_str(), entry.second.c_str());
            all_ips += std::string(entry.second) + ",";
        }

        if (all_ips.size() > 1) {
            all_ips.pop_back(); // remove last comma
            m_pOutput->pstring("all_ip_addresses", all_ips.c_str());
        }
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
    m_pOutput->psection_end();
}

void CMonitorHeaderInfo::header_cmonitor_info(
    int argc, char** argv, long sampling_interval_msec, long num_samples, unsigned int collect_flags)
{
    m_pOutput->psection_start("cmonitor");

    // rebuild the string used to start this app:
    std::string command;
    for (int i = 0; i < argc; i++) {
        command += argv[i];
        if (i != argc - 1)
            command += " ";
    }

    // -------------------------------------------------
    // the full set of arguments provided by commandline & version
    m_pOutput->pstring("command", command.c_str());
    m_pOutput->pstring("version", VERSION_STRING);

    // -------------------------------------------------
    // time information
    time_t t = time(NULL);
    struct tm lt = { 0 };
    localtime_r(&t, &lt);
    m_pOutput->plong("gmt_offset_seconds", lt.tm_gmtoff);
    m_pOutput->pstring("timezone_name", lt.tm_zone);
    m_pOutput->pdouble("sample_interval_seconds", (double)sampling_interval_msec / 1000.0f);

    // -------------------------------------------------
    // num and contents of each sample

    m_pOutput->plong("sample_num", num_samples);

    std::string str;
    for (size_t j = 1; j < PK_MAX; j *= 2) {
        PerformanceKpiFamily k = (PerformanceKpiFamily)j;
        if (collect_flags & k) {
            std::string str2 = performanceKpiFamily2string(k);
            if (!str2.empty())
                str += str2 + ",";
        }
    }
    if (!str.empty())
        str.pop_back();
    m_pOutput->pstring("collecting", str.c_str());

    // -------------------------------------------------
    // users/permissions info

    struct passwd* pw;
    uid_t uid;
    uid = geteuid();
    if ((pw = getpwuid(uid)) != NULL) {
        m_pOutput->pstring("username", pw->pw_name);
        m_pOutput->plong("userid", uid);
    } else {
        m_pOutput->pstring("username", "unknown");
    }

    m_pOutput->plong("pid", getpid());

    m_pOutput->psection_end();
}

void CMonitorHeaderInfo::header_etc_os_release()
{
    FILE* fp = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();

    if ((fp = fopen("/etc/os-release", "r")) == NULL) {
        return;
    }

    // since 2012 systemd has pushed for introduction of a standardized file where OS-level info is provided;
    // see http://0pointer.de/blog/projects/os-release
    m_pOutput->psection_start("os_release");
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        if (buf[strlen(buf) - 1] == '"')
            buf[strlen(buf) - 1] = 0; /* remove double quote */

        if (!strncmp(buf, "NAME=", strlen("NAME="))) {
            m_pOutput->pstring("name", &buf[strlen("NAME=") + 1]);
        }
        if (!strncmp(buf, "VERSION=", strlen("VERSION="))) {
            m_pOutput->pstring("version", &buf[strlen("VERSION=") + 1]);
        }
        if (!strncmp(buf, "PRETTY_NAME=", strlen("PRETTY_NAME="))) {
            m_pOutput->pstring("pretty_name", &buf[strlen("PRETTY_NAME=") + 1]);
        }
        if (!strncmp(buf, "VERSION_ID=", strlen("VERSION_ID="))) {
            m_pOutput->pstring("version_id", &buf[strlen("VERSION_ID=") + 1]);
        }
    }
    m_pOutput->psection_end();

    fclose(fp);
}

void CMonitorHeaderInfo::header_proc_cpuinfo()
{
    FILE* fp = 0;
    char buf[1024 + 1];
    char string[1024 + 1];
    double value;
    int int_val;
    int processor;

    DEBUGLOG_FUNCTION_START();
    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
        return;
    }

    m_pOutput->psection_start("cpuinfo");
    processor = -1;
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        /* moronically cpuinfo file format has Tab characters !!! */

        if (!strncmp("processor", buf, strlen("processor"))) {
            // end previous section
            if (processor != -1) {
                uint64_t value;
                // NOTE: these values are in kHz
                if (read_integer(
                        fmt::format("/sys/devices/system/cpu/cpu{}/cpufreq/scaling_min_freq", processor), value))
                    m_pOutput->pdouble("scaling_min_freq_mhz", value / (1000));
                if (read_integer(
                        fmt::format("/sys/devices/system/cpu/cpu{}/cpufreq/scaling_max_freq", processor), value))
                    m_pOutput->pdouble("scaling_max_freq_mhz", value / (1000));

                // close the section
                m_pOutput->psubsection_end();
            }

            // start new section
            sscanf(&buf[12], "%d", &int_val);
            processor = int_val;
            sprintf(string, "proc%d", processor);
            m_pOutput->psubsection_start(string);
            // processor++;
        }

        if (!strncmp("vendor_id", buf, strlen("vendor_id"))) {
            m_pOutput->pstring("vendor_id", &buf[12]);
        }
        if (!strncmp("cpu MHz", buf, strlen("cpu MHz"))) {

            /*
                The problem with "cpu MHz" is that it represents the CURRENT clock... this does change continuosly
                and is not a good KPI to place in the static header section!
                sscanf(&buf[11], "%lf", &value);
                m_pOutput->pdouble("cpu_mhz", value);
            */
        }
        if (!strncmp("cache size", buf, strlen("cache size"))) {
            sscanf(&buf[13], "%lf", &value);
            // the cache size appears to be expressed always in KB
            // this looks like to be the L3 cache on most systems I tested
            m_pOutput->pdouble("cache_size_kb", value);
        }
        if (!strncmp("physical id", buf, strlen("physical id"))) {
            sscanf(&buf[14], "%d", &int_val);
            m_pOutput->plong("physical_id", int_val);
        }
        if (!strncmp("siblings", buf, strlen("siblings"))) {
            sscanf(&buf[11], "%d", &int_val);
            m_pOutput->plong("siblings", int_val);
        }
        if (!strncmp("core id", buf, strlen("core id"))) {
            sscanf(&buf[10], "%d", &int_val);
            m_pOutput->plong("core_id", int_val);
        }
        if (!strncmp("cpu cores", buf, strlen("cpu cores"))) {
            sscanf(&buf[12], "%d", &int_val);
            m_pOutput->plong("cpu_cores", int_val);
        }
        if (!strncmp("bogomips", buf, strlen("bogomips"))) {
            sscanf(&buf[11], "%d", &int_val);
            m_pOutput->plong("bogomips", int_val);
        }
        if (!strncmp("model name", buf, strlen("model name"))) {
            m_pOutput->pstring("model_name", &buf[13]);
        }
    }
    if (processor != -1) {
        uint64_t value;
        // NOTE: these values are in kHz
        if (read_integer(fmt::format("/sys/devices/system/cpu/cpu{}/cpufreq/scaling_min_freq", processor), value))
            m_pOutput->pdouble("scaling_min_freq_mhz", value / (1000));
        if (read_integer(fmt::format("/sys/devices/system/cpu/cpu{}/cpufreq/scaling_max_freq", processor), value))
            m_pOutput->pdouble("scaling_max_freq_mhz", value / (1000));

        m_pOutput->psubsection_end();
    }
    m_pOutput->psection_end();

    fclose(fp);
}

void CMonitorHeaderInfo::header_sys_devices_numa_nodes()
{
    FILE* fp = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();

    m_pOutput->psection_start("numa_nodes");
    for (unsigned int i = 0; i < 8; i++) {
        if ((fp = fopen(fmt::format("/sys/devices/system/node/node{}/cpulist", i).c_str(), "r")) == NULL)
            continue;

        fgets(buf, 1024, fp);
        fclose(fp);
        if (buf[strlen(buf) - 1] == '\n') /* remove last char = newline */
            buf[strlen(buf) - 1] = 0;

        m_pOutput->pstring(fmt::format("node{}", i).c_str(), buf);
    }
    m_pOutput->psection_end();
}

void CMonitorHeaderInfo::header_proc_meminfo()
{
    std::set<std::string> static_memory_stats; // that never change at runtime
    static_memory_stats.insert("MemTotal");
    static_memory_stats.insert("HugePages_Total");
    static_memory_stats.insert("Hugepagesize");
    CMonitorSystem::output_meminfo_stats(m_pOutput, static_memory_stats);
}

void CMonitorHeaderInfo::header_proc_version()
{
    FILE* fp = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();

    if ((fp = fopen("/proc/version", "r")) == NULL) {
        return;
    }

    if (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        for (size_t i = 0; i < strlen(buf); i++) {
            if (buf[i] == '"')
                buf[i] = '|';
        }
        m_pOutput->psection_start("proc_version");
        m_pOutput->pstring("version", buf);
        m_pOutput->psection_end();
    }

    fclose(fp);
}

void CMonitorHeaderInfo::header_lshw()
{
#if 0
    FILE* pop = 0;
    char buf[4096 + 1];

    DEBUGLOG_FUNCTION_START();

    if (!file_exists("/usr/bin/lshw"))
        return;

    // header_lshw supports JSON output natively so we just copy/paste its output
    // into our output file.
    // IMPORTANT: unfortunately when running from inside a container header_lshw will
    //            not be able to provide all the information it provides if launched
    //            on the baremetal...

    if ((pop = popen("/usr/bin/lshw -json", "r")) == NULL)
        return;

    buf[0] = 0;
    praw("    \"header_lshw\": ");
    while (fgets(buf, 4096, pop) != NULL) {
        pbuffer_check();
        praw("    "); // indentation
        praw(buf);
        buf[0] = 0;
    }
    pclose(pop);
#endif
}

void CMonitorHeaderInfo::header_custom_metadata()
{
    m_pOutput->psection_start("custom_metadata");
    for (const auto& entry : m_pCfg->m_mapCustomMetadata)
        m_pOutput->pstring(entry.first.c_str(), entry.second.c_str());
    m_pOutput->psection_end();
}
