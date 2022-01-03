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
#include "utils_files.h"
#include "utils_misc.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <sys/types.h>

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

    /* network IP addresses */
    struct ifaddrs* interfaces = NULL;
    struct ifaddrs* ifaddrs_ptr = NULL;
    char address_buf[INET6_ADDRSTRLEN];
    char* str;

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

    std::string all_ips;
    if (getifaddrs(&interfaces) == 0) { /* retrieve the current interfaces */
        for (ifaddrs_ptr = interfaces; ifaddrs_ptr != NULL; ifaddrs_ptr = ifaddrs_ptr->ifa_next) {

            if (strncmp(ifaddrs_ptr->ifa_name, "veth", 4) == 0) {
                /* veth**** interfaces are not real/interesting interfaces... skip them */
                continue;
            }

            if (ifaddrs_ptr->ifa_addr) {
                switch (ifaddrs_ptr->ifa_addr->sa_family) {
                case AF_INET:
                    if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                             &((struct sockaddr_in*)ifaddrs_ptr->ifa_addr)->sin_addr, address_buf, sizeof(address_buf)))
                        != NULL) {
                        sprintf(label, "%s_IP4", ifaddrs_ptr->ifa_name);
                        m_pOutput->pstring(label, str);
                        all_ips += std::string(str) + ",";
                    }
                    break;
                case AF_INET6:
                    if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                             &((struct sockaddr_in6*)ifaddrs_ptr->ifa_addr)->sin6_addr, address_buf,
                             sizeof(address_buf)))
                        != NULL) {
                        sprintf(label, "%s_IP6", ifaddrs_ptr->ifa_name);
                        m_pOutput->pstring(label, str);
                        all_ips += std::string(str) + ",";
                    }
                    break;
                default:
                    // sprintf(label,"%s_Not_Supported_%d", ifaddrs_ptr->ifa_name, ifaddrs_ptr->ifa_addr->sa_family);
                    // m_pOutput->pstring(label,"");
                    break;
                }
            } else {
                sprintf(label, "%s_network_ignored", ifaddrs_ptr->ifa_name);
                m_pOutput->pstring(label, "null_address");
            }
        }

        freeifaddrs(interfaces); /* free the dynamic memory */

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

void CMonitorHeaderInfo::header_cpuinfo()
{
    FILE* fp = 0;
    char buf[1024 + 1];
    char string[1024 + 1];
    double value;
    int int_val;
    int processor;
    long power_timebase = 0;
    // long power_nominal_mhz = 0;
    int ispower = 0;

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
            if (processor != -1)
                m_pOutput->psubsection_end();

            // start new section
            sscanf(&buf[12], "%d", &int_val);
            processor = int_val;
            sprintf(string, "proc%d", processor);
            m_pOutput->psubsection_start(string);
            // processor++;
        }

        if (!strncmp("clock", buf, strlen("clock"))) { /* POWER ONLY */
            sscanf(&buf[9], "%lf", &value);
            m_pOutput->pdouble("mhz_clock", value);
            // power_nominal_mhz = value; /* save for sys_device_system_cpu() */
            ispower = 1;
        }
        if (!strncmp("vendor_id", buf, strlen("vendor_id"))) {
            m_pOutput->pstring("vendor_id", &buf[12]);
        }
        if (!strncmp("cpu MHz", buf, strlen("cpu MHz"))) {
            sscanf(&buf[11], "%lf", &value);
            m_pOutput->pdouble("cpu_mhz", value);
        }
        if (!strncmp("cache size", buf, strlen("cache size"))) {
            sscanf(&buf[13], "%lf", &value);
            m_pOutput->pdouble("cache_size", value);
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
        if (!strncmp("model name", buf, strlen("model name"))) {
            m_pOutput->pstring("model_name", &buf[13]);
        }
        if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
            ispower = 1;
            break;
        }
    }
    if (processor != -1)
        m_pOutput->psubsection_end();
    m_pOutput->psection_end();
    if (ispower) {
        m_pOutput->psection_start("cpuinfo_power");
        if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
            m_pOutput->pstring("timebase", &buf[11]);
            power_timebase = atol(&buf[11]);
            m_pOutput->plong("power_timebase", power_timebase);
        }
        while (fgets(buf, 1024, fp) != NULL) {
            buf[strlen(buf) - 1] = 0; /* remove newline */
            if (!strncmp("platform", buf, strlen("platform"))) { /* POWER only */
                m_pOutput->pstring("platform", &buf[11]);
            }
            if (!strncmp("model", buf, strlen("model"))) {
                m_pOutput->pstring("model", &buf[9]);
            }
            if (!strncmp("machine", buf, strlen("machine"))) {
                m_pOutput->pstring("machine", &buf[11]);
            }
            if (!strncmp("firmware", buf, strlen("firmware"))) {
                m_pOutput->pstring("firmware", &buf[11]);
            }
        }
        m_pOutput->psection_end();
    }

    fclose(fp);
}

void CMonitorHeaderInfo::header_meminfo()
{
    std::set<std::string> static_memory_stats; // that never change at runtime
    static_memory_stats.insert("MemTotal");
    static_memory_stats.insert("HugePages_Total");
    static_memory_stats.insert("Hugepagesize");
    proc_read_numeric_stats_from(m_pOutput, "meminfo", static_memory_stats);
}

void CMonitorHeaderInfo::header_version()
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

void CMonitorHeaderInfo::header_lscpu()
{
    FILE* pop = 0;
    int data_col = 21;
    int len = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();
    if (!file_or_dir_exists("/usr/bin/lscpu"))
        return;
    if ((pop = popen("/usr/bin/lscpu", "r")) == NULL)
        return;

    buf[0] = 0;
    m_pOutput->psection_start("lscpu");
    while (fgets(buf, 1024, pop) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        // LogDebug("DEBUG: lscpu line is |%s|\n", buf);
        if (!strncmp("Architecture:", buf, strlen("Architecture:"))) {
            len = strlen(buf);
            for (data_col = 14; data_col < len; data_col++) {
                if (isalnum(buf[data_col]))
                    break;
            }
            m_pOutput->pstring("architecture", &buf[data_col]);
        }
        if (!strncmp("Byte Order:", buf, strlen("Byte Order:"))) {
            m_pOutput->pstring("byte_order", &buf[data_col]);
        }
        if (!strncmp("CPU(s):", buf, strlen("CPU(s):"))) {
            m_pOutput->pstring("cpus", &buf[data_col]);
        }
        if (!strncmp("On-line CPU(s) list:", buf, strlen("On-line CPU(s) list:"))) {
            m_pOutput->pstring("online_cpu_list", &buf[data_col]);
        }
        if (!strncmp("Off-line CPU(s) list:", buf, strlen("Off-line CPU(s) list:"))) {
            m_pOutput->pstring("online_cpu_list", &buf[data_col]);
        }
        if (!strncmp("Model:", buf, strlen("Model:"))) {
            m_pOutput->pstring("model", &buf[data_col]);
        }
        if (!strncmp("Model name:", buf, strlen("Model name:"))) {
            m_pOutput->pstring("model_name", &buf[data_col]);
        }
        if (!strncmp("Thread(s) per core:", buf, strlen("Thread(s) per core:"))) {
            m_pOutput->pstring("threads_per_core", &buf[data_col]);
        }
        if (!strncmp("Core(s) per socket:", buf, strlen("Core(s) per socket:"))) {
            m_pOutput->pstring("cores_per_socket", &buf[data_col]);
        }
        if (!strncmp("Socket(s):", buf, strlen("Socket(s):"))) {
            m_pOutput->pstring("sockets", &buf[data_col]);
        }
        if (!strncmp("NUMA node(s):", buf, strlen("NUMA node(s):"))) {
            m_pOutput->pstring("numa_nodes", &buf[data_col]);
        }
        if (!strncmp("CPU MHz:", buf, strlen("CPU MHz:"))) {
            m_pOutput->pstring("cpu_mhz", &buf[data_col]);
        }
        if (!strncmp("CPU max MHz:", buf, strlen("CPU max MHz:"))) {
            m_pOutput->pstring("cpu_max_mhz", &buf[data_col]);
        }
        if (!strncmp("CPU min MHz:", buf, strlen("CPU min MHz:"))) {
            m_pOutput->pstring("cpu_min_mhz", &buf[data_col]);
        }
        /* Intel only */
        if (!strncmp("BogoMIPS:", buf, strlen("BogoMIPS:"))) {
            m_pOutput->pstring("bogomips", &buf[data_col]);
        }
        if (!strncmp("Vendor ID:", buf, strlen("Vendor ID:"))) {
            m_pOutput->pstring("vendor_id", &buf[data_col]);
        }
        if (!strncmp("CPU family:", buf, strlen("CPU family:"))) {
            m_pOutput->pstring("cpu_family", &buf[data_col]);
        }
        if (!strncmp("Stepping:", buf, strlen("Stepping:"))) {
            m_pOutput->pstring("stepping", &buf[data_col]);
        }
    }
    m_pOutput->psection_end();
    pclose(pop);
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
