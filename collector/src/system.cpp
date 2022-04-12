/*
 * system.cpp - code for collecting SYSTEM-level statistics (i.e. not cgroup-aware)
 * Developer: Nigel Griffiths.
 * (C) Copyright 2018 Nigel Griffiths

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

#include "system.h"
#include "logger.h"
#include "output_frontend.h"
#include "utils_files.h"
#include "utils_string.h"
#include <arpa/inet.h>
#include <assert.h>
#include <ifaddrs.h>
#include <mntent.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>

// ----------------------------------------------------------------------------------
// Macros
// ----------------------------------------------------------------------------------

#define DELTA_TOTAL(stat) ((float)(stat - total_cpu.stat) / (float)elapsed_sec / ((float)(max_cpu_count + 1.0)))

// ----------------------------------------------------------------------------------
// CMonitorSystem
// ----------------------------------------------------------------------------------

void CMonitorSystem::init()
{
    m_cpu_stat.set_file("/proc/stat");
    m_disk_stat.set_file("/proc/diskstats");
    m_uptime.set_file("/proc/uptime");
    m_loadavg.set_file("/proc/loadavg");
    m_meminfo.set_file("/proc/meminfo");
    m_vmstat.set_file("/proc/vmstat");
}

#if 0 // currently unused
void CMonitorSystem::proc_stat_cpu_total(
    const char* cpu_data, double elapsed_sec, OutputFields output_opts, cpu_specs_t& total_cpu, int max_cpu_count)
{
    long long user;
    long long nice;
    long long sys;
    long long idle;
    long long iowait;
    long long hardirq;
    long long softirq;
    long long steal;
    long long guest;
    long long guestnice;

    // see http://man7.org/linux/man-pages/man5/proc.5.html
    // Look for "/proc/stat"

    int count = sscanf(cpu_data, /* cpu USER */
        "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", // force newline
        &user, &nice, &sys, &idle, &iowait, &hardirq, &softirq, &steal, &guest, &guestnice);
    if (count != 10)
        return;

    if (output_opts != PF_NONE) {
        m_pOutput->psubsection_start("cpu_total");
        switch (output_opts) {
        case PF_NONE:
            assert(0);
            break;
        case PF_ALL:
        case PF_USED_BY_CHART_SCRIPT_ONLY:
            m_pOutput->pdouble("user", DELTA_TOTAL(user)); /* incrementing counter */
            m_pOutput->pdouble("nice", DELTA_TOTAL(nice)); /* incrementing counter */
            m_pOutput->pdouble("sys", DELTA_TOTAL(sys)); /* incrementing counter */
            m_pOutput->pdouble("idle", DELTA_TOTAL(idle)); /* incrementing counter */
            /*			m_pOutput->pdouble("DEBUG IDLE idle: %lld %lld %lld\n", total_cpu.idle,
             * idle, idle-total_cpu.idle); */
            m_pOutput->pdouble("iowait", DELTA_TOTAL(iowait)); /* incrementing counter */
            m_pOutput->pdouble("hardirq", DELTA_TOTAL(hardirq)); /* incrementing counter */
            m_pOutput->pdouble("softirq", DELTA_TOTAL(softirq)); /* incrementing counter */
            m_pOutput->pdouble("steal", DELTA_TOTAL(steal)); /* incrementing counter */
            m_pOutput->pdouble("guest", DELTA_TOTAL(guest)); /* incrementing counter */
            m_pOutput->pdouble("guestnice", DELTA_TOTAL(guestnice)); /* incrementing counter */
            break;
        }
        m_pOutput->psubsection_end();
    }
    total_cpu.user = user;
    total_cpu.nice = nice;
    total_cpu.sys = sys;
    total_cpu.idle = idle;
    total_cpu.iowait = iowait;
    total_cpu.hardirq = hardirq;
    total_cpu.softirq = softirq;
    total_cpu.steal = steal;
    total_cpu.guest = guest;
    total_cpu.guestnice = guestnice;
}
#endif

int CMonitorSystem::proc_stat_cpu_index(const char* cpu_data, cpu_specs_t* cpu_values_out)
{
    int cpuno;

    // see http://man7.org/linux/man-pages/man5/proc.5.html
    // Look for "/proc/stat"

    /* cpu_data must be a pointer immediately after the 'cpu' chars of:
         cpuNNN ...lots of counters
    */
    int count = sscanf(cpu_data, // force newline
        "%d %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", // force newline
        &cpuno, &cpu_values_out->user, &cpu_values_out->nice, &cpu_values_out->sys, &cpu_values_out->idle,
        &cpu_values_out->iowait, &cpu_values_out->hardirq, &cpu_values_out->softirq, &cpu_values_out->steal,
        &cpu_values_out->guest, &cpu_values_out->guestnice);
    if (count != 11)
        return -1;

    if (cpuno >= MAX_LOGICAL_CPU)
        return -1;
    if (!is_monitored_cpu(cpuno))
        return -1;
    return cpuno;
}

/*
read /proc/stat
*/
void CMonitorSystem::sample_cpu_stat(double elapsed_sec, OutputFields output_opts)
{
    long long new_ctx, btime, new_processes, procs_running, procs_blocked;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_CPU) == 0)
        return;

    DEBUGLOG_FUNCTION_START();

    CMonitorLogger::instance()->LogDebug("proc_stat(%.4f) max_cpu_count=%d\n", elapsed_sec, m_cpu_count);
    if (!m_cpu_stat.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_cpu_stat.get_file().c_str());
        return;
    }

    cpu_specs_t new_values[MAX_LOGICAL_CPU];
    cpu_specs_t tmp_values;
    const char* line = m_cpu_stat.get_next_line();
    while (line) {
        if (strncmp(line, "cpu", 3) == 0) {
            if (line[3] == ' ') {
                // found the summary line for ALL cpus together, e.g.:
                //     cpu  265510448 66285 143983783 14772309342 4657946 0 16861124 0 0 0
                // skip it
                line = m_cpu_stat.get_next_line();
                continue;
            } else {
                // found a line for a specific CPU like:
                //    cpu1 90470 3217 30294 291392 17250 0 3242 0 0 0
                // process it
                int cpuno = proc_stat_cpu_index(&line[3], &tmp_values);
                if (cpuno > m_cpu_count)
                    m_cpu_count = cpuno;
                if (cpuno >= 0)
                    new_values[cpuno] = tmp_values;
            }
        } else if (!strncmp(line, "ctxt", 4)) {
            new_ctx = 0;
            sscanf(&line[5], "%lld", &new_ctx); /* counter */
        } else if (!strncmp(line, "btime", 5)) {
            btime = 0;
            sscanf(&line[6], "%lld", &btime); /* seconds since boot */
        } else if (!strncmp(line, "processes", 9)) {
            new_processes = 0;
            sscanf(&line[10], "%lld", &new_processes); /* counter  actually forks */
        } else if (!strncmp(line, "procs_running", 13)) {
            procs_running = 0;
            sscanf(&line[14], "%lld", &procs_running);
        } else if (!strncmp(line, "procs_blocked", 13)) {
            procs_blocked = 0;
            sscanf(&line[14], "%lld", &procs_blocked);
        }

        line = m_cpu_stat.get_next_line();
    }

    if (output_opts != PF_NONE) {
        m_pOutput->psection_start("stat");
        for (int i = 0; i <= m_cpu_count; i++) {
            if (!is_monitored_cpu(i))
                continue;

#define DELTA_CPU_STAT(stat) ((double)(new_values[i].stat - m_cpu_stat_prev_values[i].stat) / elapsed_sec)

            m_pOutput->psubsection_start(fmt::format("cpu{:d}", i).c_str());
            switch (output_opts) {
            case PF_NONE:
                assert(0);
                break;
            case PF_ALL:
            case PF_USED_BY_CHART_SCRIPT_ONLY:
                m_pOutput->pdouble("user", DELTA_CPU_STAT(user)); /* counter */
                m_pOutput->pdouble("nice", DELTA_CPU_STAT(nice)); /* counter */
                m_pOutput->pdouble("sys", DELTA_CPU_STAT(sys)); /* counter */
                m_pOutput->pdouble("idle", DELTA_CPU_STAT(idle)); /* counter */
                m_pOutput->pdouble("iowait", DELTA_CPU_STAT(iowait)); /* counter */
                m_pOutput->pdouble("hardirq", DELTA_CPU_STAT(hardirq)); /* counter */
                m_pOutput->pdouble("softirq", DELTA_CPU_STAT(softirq)); /* counter */
                m_pOutput->pdouble("steal", DELTA_CPU_STAT(steal)); /* counter */
                m_pOutput->pdouble("guest", DELTA_CPU_STAT(guest)); /* counter */
                m_pOutput->pdouble("guestnice", DELTA_CPU_STAT(guestnice)); /* counter */
                break;
            }
            m_pOutput->psubsection_end();
        }

        m_pOutput->psubsection_start("counters");
        m_pOutput->pdouble("ctxt", ((double)(new_ctx - m_cpu_stat_old_ctxt) / elapsed_sec));
        m_pOutput->plong("btime", btime);
        m_pOutput->pdouble("processes_forks", ((double)(new_processes - m_cpu_stat_old_processes) / elapsed_sec));
        m_pOutput->plong("procs_running", procs_running);
        m_pOutput->plong("procs_blocked", procs_blocked);
        m_pOutput->psubsection_end();

        m_pOutput->psection_end();
    }

    m_cpu_stat_old_ctxt = new_ctx;
    m_cpu_stat_old_processes = new_processes;
    for (int i = 0; i <= m_cpu_count; i++)
        m_cpu_stat_prev_values[i] = new_values[i];
}

/*
read /proc/meminfo
which has format
    STATNAME: <value> kB
or
    STATNAME: <value>
*/
bool CMonitorSystem::read_meminfo_stats(FastFileReader& reader, const std::set<std::string>& allowedStatsNames,
    CMonitorOutputFrontend* pOutput, numeric_parser_stats_t& out_stats)
{
    /*
        NOTE: this is a specialized variant of FastFileReader::read_numeric_stats()
    */

    size_t nread = 0, ndiscarded = 0;
    if (!reader.open_or_rewind()) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", reader.get_file().c_str());
        return nread;
    }

    pOutput->psection_start("proc_meminfo");

    std::string label;
    uint64_t value = 0;
    const char* pline = reader.get_next_line();
    while (pline) {

        std::string line(pline);
        bool is_kb = false;
        if (line.size() > 3 && line[line.size() - 2] == 'k' && line[line.size() - 1] == 'B') {
            is_kb = true;
            line.resize(line.size() - 3);
        }
        // CMonitorLogger::instance()->LogDebug("read_meminfo_stats: [%s] is_kb=%d", line.c_str(), is_kb);

        std::string value_str;
        if (split_string_on_first_separator(line, ':', label, value_str)) {

            // skip all the spaces after the colon
            unsigned int i = 0;
            for (; i < value_str.size() && isspace(value_str[i]); i++)
                ;

            if (string2int(&value_str[i], value)) {
                // adjust kB -> bytes if needed
                if (is_kb)
                    value *= 1000;

                // apply KPI filter
                if (allowedStatsNames.empty() /* all stats must be put in output */
                    || allowedStatsNames.find(label) != allowedStatsNames.end()) {
                    pOutput->plong(label.c_str(), value);
                    nread++;
                } else
                    ndiscarded++;
            }
        }

        pline = reader.get_next_line();
    }

    pOutput->psection_end();

    CMonitorLogger::instance()->LogDebug(
        "From %s read=%zu discarded=%zu kpis", reader.get_file().c_str(), nread, ndiscarded);

    return nread;
}

void CMonitorSystem::sample_memory(const std::set<std::string>& charted_stats_from_meminfo)
{
    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_MEMORY) == 0)
        return;

    DEBUGLOG_FUNCTION_START();

    key_value_map_t out;
    numeric_parser_stats_t out_stats;
    read_meminfo_stats(m_meminfo, charted_stats_from_meminfo, m_pOutput, out_stats);

    if (m_pCfg->m_nOutputFields == PF_ALL) {
        key_value_map_t out;
        numeric_parser_stats_t out_stats;
        m_vmstat.read_numeric_stats(std::set<std::string>(), out, out_stats);

        m_pOutput->psection_start("proc_vmstat");
        for (auto entry : out)
            m_pOutput->plong(entry.first.c_str(), entry.second);
        m_pOutput->psection_end();
    }
}

/*
read /proc/diskstats
*/
void CMonitorSystem::sample_diskstats(double elapsed_sec, OutputFields output_opts)
{
    static bool first_time = true;
    int dk_stats;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_DISK) == 0)
        return;

    DEBUGLOG_FUNCTION_START();

    if (first_time) {
        /* popen variables */
        FILE* pop;
        char tmpstr[1024 + 1];
        long i;
        long j;
        long len;

        pop = popen("lsblk --nodeps --output NAME,TYPE --raw 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            if (fgets(tmpstr, 70, pop)) {
                for (i = 0;; i++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 70, pop) == NULL)
                        break;
                    tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
                    len = strlen(tmpstr);
                    for (j = 0; j < len; j++)
                        if (tmpstr[j] == ' ')
                            tmpstr[j] = 0;

                    if (strncmp(tmpstr, "loop", 4) != 0) {
                        // CMonitorLogger::instance()->LogDebug("DEBUG saved %ld %s disk name\n", i,
                        // previous[i].dk_name);
                        m_disks.insert(tmpstr);
                    } else {
                        CMonitorLogger::instance()->LogDebug("Discarding disk %s\n", tmpstr);
                        /* loop**** disks are not real */
                    }
                }
            }
            pclose(pop);
        }

        CMonitorLogger::instance()->LogDebug("Found %zu disks to monitor\n", m_disks.size());
        first_time = false;
    }

    if (!m_disk_stat.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_disk_stat.get_file().c_str());
        return;
    }

    // FIXME: break in 2 parts the parsing of the stat file and the output of measurements just like done for CPU and
    // net stats

    if (output_opts != PF_NONE)
        m_pOutput->psection_start("disks");

    diskinfo_t current;
    const char* buf = m_disk_stat.get_next_line();
    while (buf) {
        // char* pbuf = const_cast<char*>(buf);
        // pbuf[strlen(buf) - 1] = 0; /* remove newline */ // unnecessary???

        // CMonitorLogger::instance()->LogDebug("DISKSTATS: \"%s\"", buf);

        /* zero the data ready for reading */
        bzero(&current, sizeof(diskinfo_t));

        // try to read all the 14 fields
        dk_stats = sscanf(buf, "%ld %ld %s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", &current.dk_major,
            &current.dk_minor, // force newline
            &current.dk_name[0], // force newline
            &current.dk_reads, &current.dk_rmerge, &current.dk_rkb, &current.dk_rmsec, // force newline
            &current.dk_writes, &current.dk_wmerge, &current.dk_wkb, &current.dk_wmsec, // force newline
            &current.dk_inflight, &current.dk_time, &current.dk_backlog);

        if (dk_stats == 7) {
            /* shuffle the data around due to missing columns for partitions */
            current.dk_wkb = current.dk_rmsec;
            current.dk_writes = current.dk_rkb;
            current.dk_rkb = current.dk_rmerge;
            current.dk_rmsec = 0;
            current.dk_rmerge = 0;
        } else if (dk_stats != 14)
            CMonitorLogger::instance()->LogError("disk sscanf wanted 14 but returned=%d line=%s\n", dk_stats, buf);

        current.dk_rkb /= 2; /* convert from sectors to Kbyte, keeping in mind that 1 sector = 512 bytes = 1/2 Kbyte */
        current.dk_wkb /= 2;
        current.dk_xfers = current.dk_reads + current.dk_writes;
        if (current.dk_xfers == 0)
            current.dk_bsize = 0;
        else
            current.dk_bsize = ((current.dk_rkb + current.dk_wkb) / current.dk_xfers) * 1024;

        // f18m: not really sure this is correct... assumes that this field is updated 10 times per second
        current.dk_time /= 10.0; /* in milli-seconds to make it up to 100%, 1000/100 = 10 */

        const auto it_prev = m_previous_diskinfo.find(current.dk_name);
        if (it_prev != m_previous_diskinfo.end()) {
            const diskinfo_t& previous = it_prev->second;

            if (output_opts != PF_NONE) {
                m_pOutput->psubsection_start(current.dk_name);

#define DELTA_DISK_STAT(member) ((double)(current.member - previous.member) / elapsed_sec)

                switch (output_opts) {
                case PF_NONE:
                    assert(0);
                    break;

                case PF_ALL:
                    m_pOutput->pdouble("reads", DELTA_DISK_STAT(dk_reads));
                    m_pOutput->pdouble("rmerge", DELTA_DISK_STAT(dk_rmerge));
                    m_pOutput->pdouble("rkb", DELTA_DISK_STAT(dk_rkb));
                    m_pOutput->pdouble("rmsec", DELTA_DISK_STAT(dk_rmsec));

                    m_pOutput->pdouble("writes", DELTA_DISK_STAT(dk_writes));
                    m_pOutput->pdouble("wmerge", DELTA_DISK_STAT(dk_wmerge));
                    m_pOutput->pdouble("wkb", DELTA_DISK_STAT(dk_wkb));
                    m_pOutput->pdouble("wmsec", DELTA_DISK_STAT(dk_wmsec));

                    m_pOutput->plong("inflight", current.dk_inflight);
                    m_pOutput->pdouble("time", DELTA_DISK_STAT(dk_time));
                    m_pOutput->pdouble("backlog", DELTA_DISK_STAT(dk_backlog));
                    m_pOutput->pdouble("xfers", DELTA_DISK_STAT(dk_xfers));
                    m_pOutput->plong("bsize", current.dk_bsize);
                    break;

                case PF_USED_BY_CHART_SCRIPT_ONLY:
                    m_pOutput->pdouble("rkb", DELTA_DISK_STAT(dk_rkb));
                    m_pOutput->pdouble("wkb", DELTA_DISK_STAT(dk_wkb));
                    break;
                }

                m_pOutput->psubsection_end();
            }
        }

        m_previous_diskinfo[current.dk_name] = current;
        buf = m_disk_stat.get_next_line();
    }
    if (output_opts != PF_NONE)
        m_pOutput->psection_end();
}

/*
 read /proc/net/dev
 */
void CMonitorSystem::sample_net_dev(double elapsed_sec, OutputFields output_opts)
{
    static bool first_time = true;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_NETWORK) == 0)
        return;

    DEBUGLOG_FUNCTION_START();

    if (first_time) {
        // here all network interfaces are considered even if DOWN: the reason is that later on they may become UP
        // and in such case they will become interesting; to maintain all output samples identical over time,
        // thus all network interfaces are considered
        netdevices_map_t devices_and_addresses;
        CMonitorSystem::get_net_dev_list(devices_and_addresses, false /* include_only_interfaces_up */);
        for (auto entry : devices_and_addresses)
            m_network_interfaces_up.insert(entry.first);

        CMonitorLogger::instance()->LogDebug(
            "Found %zu network interfaces to monitor\n", m_network_interfaces_up.size());
        first_time = false;
    }

    if (m_network_interfaces_up.empty())
        return; // this happens in e.g. Docker containers having no network

    // clang-format off
    /*
        the file has a format like (see https://man7.org/linux/man-pages/man5/proc.5.html)

            Inter-|   Receive                                                |  Transmit
             face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
                lo: 2776770   11307    0    0    0     0          0         0  2776770   11307    0    0    0     0       0          0
              eth0: 1215645    2751    0    0    0     0          0         0  1782404    4324    0    0    0   427       0          0
              ppp0: 1622270    5552    1    0    0     0          0         0   354130    5669    0    0    0     0       0          0
              tap0:    7714      81    0    0    0     0          0         0     7714      81    0    0    0     0       0          0
    */
    // clang-format on

    netinfo_map_t new_stats;
    read_net_dev_stats("/proc/net/dev", m_network_interfaces_up, new_stats);

    if (output_opts != PF_NONE) {
        m_pOutput->psection_start("network_interfaces");
        output_net_dev_stats(m_pOutput, elapsed_sec, new_stats, m_previous_netinfo, output_opts);
        m_pOutput->psection_end();
    }

    // finally remember the last sampled stats:
    m_previous_netinfo = new_stats;
}

/* static */
bool CMonitorSystem::get_net_dev_list(netdevices_map_t& out_map, bool include_only_interfaces_up)
{
    std::string all_ips;
    struct ifaddrs* interfaces = NULL;
    struct ifaddrs* ifaddrs_ptr = NULL;
    char address_buf[INET6_ADDRSTRLEN];
    char* str = NULL;

    DEBUGLOG_FUNCTION_START();

    /* retrieve the current interfaces */
    if (getifaddrs(&interfaces) != 0) {
        CMonitorLogger::instance()->LogErrorWithErrno(
            "getifaddrs() failed; cannot retrieve list of network interfaces.\n");
        return false;
    }

    for (ifaddrs_ptr = interfaces; ifaddrs_ptr != NULL; ifaddrs_ptr = ifaddrs_ptr->ifa_next) {

        if (strncmp(ifaddrs_ptr->ifa_name, "veth", 4) == 0) {
            /* veth**** interfaces are not real/interesting interfaces... skip them */
            CMonitorLogger::instance()->LogDebug(
                "skipping network device '%s' since it's a virtual ETH dev\n", ifaddrs_ptr->ifa_name);
            continue;
        }

        if (include_only_interfaces_up && (ifaddrs_ptr->ifa_flags & IFF_UP) == 0) {
            CMonitorLogger::instance()->LogDebug(
                "skipping network device '%s' since it's DOWN\n", ifaddrs_ptr->ifa_name);
            continue;
        }

        if (ifaddrs_ptr->ifa_addr) {
            switch (ifaddrs_ptr->ifa_addr->sa_family) {
            case AF_INET:
                if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                         &((struct sockaddr_in*)ifaddrs_ptr->ifa_addr)->sin_addr, address_buf, sizeof(address_buf)))
                    != NULL) {
                    out_map[ifaddrs_ptr->ifa_name] = str;
                }
                break;
            case AF_INET6:
                if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                         &((struct sockaddr_in6*)ifaddrs_ptr->ifa_addr)->sin6_addr, address_buf, sizeof(address_buf)))
                    != NULL) {
                    out_map[ifaddrs_ptr->ifa_name] = str;
                }
                break;
            default:
                // sprintf(label,"%s_Not_Supported_%d", ifaddrs_ptr->ifa_name, ifaddrs_ptr->ifa_addr->sa_family);
                // m_pOutput->pstring(label,"");
                break;
            }
        } else {
            out_map[ifaddrs_ptr->ifa_name] = "";
        }
    }

    freeifaddrs(interfaces); /* free the dynamic memory */
    return true;
}

/* static */
bool CMonitorSystem::read_net_dev_stats(
    const std::string& filename, const std::set<std::string>& net_iface_whitelist, netinfo_map_t& out_stats)
{
    // clang-format off
    /*
        the file has a format like (see https://man7.org/linux/man-pages/man5/proc.5.html)

            Inter-|   Receive                                                |  Transmit
             face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
                lo: 2776770   11307    0    0    0     0          0         0  2776770   11307    0    0    0     0       0          0
              eth0: 1215645    2751    0    0    0     0          0         0  1782404    4324    0    0    0   427       0          0
              ppp0: 1622270    5552    1    0    0     0          0         0   354130    5669    0    0    0     0       0          0
              tap0:    7714      81    0    0    0     0          0         0     7714      81    0    0    0     0       0          0
    */
    // clang-format on

    // FIXME: instead of doing a fopen() here we could take as arg both a FastFileReader and the filename ;
    //        then we invoke the given FastFileReader set_file() and read from it: in best case if filename didn't
    //        change we will save the operation of opening a new FD!
    FILE* fp = 0;
    if ((fp = fopen(filename.c_str(), "r")) == NULL) {
        CMonitorLogger::instance()->LogErrorWithErrno("failed to open %s", filename.c_str());
        return false;
    }

    char buf[1024];
    if (fgets(buf, 1024, fp) == NULL) /* throw away the header line */
        return false;
    if (fgets(buf, 1024, fp) == NULL) /* throw away the header line */
        return false;

    uint64_t junk;
    while (fgets(buf, 1024, fp) != NULL) {
        strip_spaces(buf);

        char name[128];
        netinfo_t current;
        bzero(&current, sizeof(netinfo_t));
        int ret = sscanf(&buf[0], "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
            name, // force newline
            // input
            &current.if_ibytes, &current.if_ipackets, &current.if_ierrs, &current.if_idrop, &current.if_ififo,
            &current.if_iframe, &junk, &junk, // force newline
            // output
            &current.if_obytes, &current.if_opackets, &current.if_oerrs, &current.if_odrop, &current.if_ofifo,
            &current.if_ocolls, &current.if_ocarrier);

        if (ret != 16) {
            CMonitorLogger::instance()->LogError("net sscanf wanted 16 returned = %d line=%s\n", ret, (char*)buf);
            continue;
        }

        // as fixed rule always discard the loopback device:
        if (strncmp(name, "lo", 2) == 0)
            continue;

        if (net_iface_whitelist.empty() || net_iface_whitelist.find(name) != net_iface_whitelist.end())
            // this interface is in the whitelist, store it:
            out_stats[name] = current;
    }

    return !out_stats.empty();
}

/* static */
bool CMonitorSystem::output_net_dev_stats(CMonitorOutputFrontend* m_pOutput, double elapsed_sec,
    const netinfo_map_t& new_stats, const netinfo_map_t& prev_stats, OutputFields output_opts)
{
#define DELTA_NET_STAT(member) ((double)(current.member - previous.member) / elapsed_sec)

    for (auto it : new_stats) {
        const std::string& name = it.first;
        const netinfo_t& current = it.second;

        const auto it_prev = prev_stats.find(name);
        if (it_prev == prev_stats.end())
            continue; // looks like a new interface, skip it in this sample if we cannot take any delta

        // found previous values, statistics can now be generated:
        const netinfo_t& previous = it_prev->second;
        m_pOutput->psubsection_start(name.c_str());

        // subsection strings
        std::string subsec_name_bytes = name + "_" + "bytes";
        std::string subsec_name_packets = name + "_" + "packets";
        std::string subsec_name_errs = name + "_" + "errs";
        std::string subsec_name_drop = name + "_" + "drop";
        std::string subsec_name_fifo = name + "_" + "fifo";
        std::string subsec_name_frame = name + "_" + "frame";
        std::string subsec_name_colls = name + "_" + "colls";
        std::string subsec_name_carrier = name + "_" + "carrier";

        switch (output_opts) {
        case PF_NONE:
            assert(0);
            break;

        case PF_ALL:
            m_pOutput->psubsection_start(subsec_name_bytes.c_str());
                m_pOutput->plong("input", DELTA_NET_STAT(if_ibytes));
                m_pOutput->plong("output", DELTA_NET_STAT(if_obytes));
            m_pOutput->psubsection_end();

            m_pOutput->psubsection_start(subsec_name_packets.c_str());
               m_pOutput->plong("input", DELTA_NET_STAT(if_ipackets));
               m_pOutput->plong("output", DELTA_NET_STAT(if_opackets));
            m_pOutput->psubsection_end();

            m_pOutput->psubsection_start(subsec_name_errs.c_str());
                m_pOutput->plong("input", DELTA_NET_STAT(if_ierrs));
                m_pOutput->plong("output", DELTA_NET_STAT(if_oerrs));
            m_pOutput->psubsection_end();

            m_pOutput->psubsection_start(subsec_name_drop.c_str());
                m_pOutput->plong("input", DELTA_NET_STAT(if_idrop));
                m_pOutput->plong("output", DELTA_NET_STAT(if_odrop));
            m_pOutput->psubsection_end();

            m_pOutput->psubsection_start(subsec_name_fifo.c_str());
                m_pOutput->plong("input", DELTA_NET_STAT(if_ififo));
                m_pOutput->plong("output", DELTA_NET_STAT(if_ofifo));
            m_pOutput->psubsection_end();

            m_pOutput->psubsection_start(subsec_name_frame.c_str());
                m_pOutput->plong("input", DELTA_NET_STAT(if_iframe));
            m_pOutput->psubsection_end();

            m_pOutput->psubsection_start(subsec_name_colls.c_str());
                m_pOutput->plong("output", DELTA_NET_STAT(if_ocolls));
            m_pOutput->psubsection_end();

            m_pOutput->psubsection_start(subsec_name_carrier.c_str());
                m_pOutput->plong("output", DELTA_NET_STAT(if_ocarrier));
            m_pOutput->psubsection_end();
            break;

        case PF_USED_BY_CHART_SCRIPT_ONLY:
                m_pOutput->plong("ibytes", DELTA_NET_STAT(if_ibytes));
                m_pOutput->plong("obytes", DELTA_NET_STAT(if_obytes));
                m_pOutput->plong("ipackets", DELTA_NET_STAT(if_ipackets));
                m_pOutput->plong("opackets", DELTA_NET_STAT(if_opackets));
            break;
        }
        m_pOutput->psubsection_end();
    }

    return true;
}

/*
 read /proc/uptime
*/
void CMonitorSystem::sample_uptime()
{
    DEBUGLOG_FUNCTION_START();

    if (!m_uptime.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_uptime.get_file().c_str());
        return;
    }

    const char* pline = m_uptime.get_next_line();
    if (!pline)
        return;

    long long value;
    long long days;
    long long hours;
    if (sscanf(pline, "%lld", &value) == 1) {
        days = value / 60 / 60 / 24;
        hours = (value - (days * 60 * 60 * 24)) / 60 / 60;

        m_pOutput->psection_start("proc_uptime");
        m_pOutput->plong("total_seconds", value);
        m_pOutput->plong("days", days);
        m_pOutput->plong("hours", hours);
        m_pOutput->psection_end();
    }
}

void CMonitorSystem::sample_loadavg()
{
    DEBUGLOG_FUNCTION_START();

    if (!m_loadavg.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_loadavg.get_file().c_str());
        return;
    }

    const char* pline = m_loadavg.get_next_line();
    if (!pline)
        return;

    /*
            /proc/loadavg
            The first three fields in this file are load average figures giving
            the  number  of jobs in the run queue (state R) or waiting for disk
            I/O (state D) averaged over 1, 5, and 15  minutes.
            They are the same as the load average numbers given by
            uptime(1) and other programs.  The fourth field consists of
            two numbers separated by a slash (/).  The first of these is
            the number of currently runnable kernel scheduling entities
            (processes, threads).  The value after the slash is the number
            of kernel scheduling entities that currently exist on the sys‐
            tem.  The fifth field is the PID of the process that was most
            recently created on the system.
     */

    float load_avg_1min;
    float load_avg_5min;
    float load_avg_15min;
    if (sscanf(pline, "%f %f %f", &load_avg_1min, &load_avg_5min, &load_avg_15min) == 3) {
        m_pOutput->psection_start("proc_loadavg");
        m_pOutput->pdouble("load_avg_1min", load_avg_1min);
        m_pOutput->pdouble("load_avg_5min", load_avg_5min);
        m_pOutput->pdouble("load_avg_15min", load_avg_15min);
        m_pOutput->psection_end();
    }
}

void CMonitorSystem::sample_filesystems()
{
    FILE* fp;
    struct mntent* fs;
    struct statfs vfs;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_DISK) == 0)
        return;

    DEBUGLOG_FUNCTION_START();
    if ((fp = setmntent("/etc/mtab", "r")) == NULL)
        CMonitorLogger::instance()->LogError("setmntent(\"/etc/mtab\", \"r\") failed");

    m_pOutput->psection_start("filesystems");
    while ((fs = getmntent(fp)) != NULL) {
        // NOTE: /dev/loop* filesystems are not real filesystems - e.g. on Ubuntu they are used for SNAPs
        if (fs->mnt_fsname[0] == '/' && strncmp(fs->mnt_fsname, "/dev/loop", 9) != 0) {
            if (statfs(fs->mnt_dir, &vfs) != 0) {
                CMonitorLogger::instance()->LogErrorWithErrno("%s: statfs failed: %d\n", fs->mnt_dir, errno);
            }
            // CMonitorLogger::instance()->LogDebug("%s, mounted on %s:\n", fs->mnt_dir, fs->mnt_fsname);

            m_pOutput->psubsection_start(fs->mnt_fsname);
            m_pOutput->pstring("fs_dir", fs->mnt_dir);
            m_pOutput->pstring("fs_type", fs->mnt_type);
            m_pOutput->pstring("fs_opts", fs->mnt_opts);

            m_pOutput->plong("fs_freqs", fs->mnt_freq);
            m_pOutput->plong("fs_passno", fs->mnt_passno);
            m_pOutput->plong("fs_bsize", vfs.f_bsize);
            m_pOutput->plong("fs_size_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->plong("fs_free_mb", (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->plong(
                "fs_used_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024 - (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->pdouble(
                "fs_full_percent", ((double)vfs.f_blocks - (double)vfs.f_bfree) / (double)vfs.f_blocks * (double)100.0);
            /*
             * m_pOutput->pdouble("fs_full_percent", ((vfs.f_blocks * vfs.f_bsize) - (vfs.f_bfree * vfs.f_bsize) ) /
             *					(vfs.f_blocks * vfs.f_bsize) * 100.00);
             */
            m_pOutput->plong("fs_avail", (vfs.f_bavail * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->plong("fs_files", vfs.f_files);
            m_pOutput->plong("fs_files_free", vfs.f_ffree);
            m_pOutput->plong("fs_namelength", vfs.f_namelen);
            m_pOutput->psubsection_end();
        }
    }
    m_pOutput->psection_end();
    endmntent(fp);
}

/* static */
unsigned int CMonitorSystem::get_all_cpus(std::set<uint64_t>& cpu_indexes, const std::string& stat_file)
{
    FastFileReader cpu_stat(stat_file);

    cpu_indexes.clear();

    if (!cpu_stat.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", cpu_stat.get_file().c_str());
        return 0;
    }

    const char* line = cpu_stat.get_next_line();
    while (line) {
        if (strncmp(line, "cpu", 3) == 0) {
            if (line[3] == ' ') {
                // found the summary line for ALL cpus together, e.g.:
                //     cpu  265510448 66285 143983783 14772309342 4657946 0 16861124 0 0 0
                // skip it
                line = cpu_stat.get_next_line();
                continue;
            } else {
                // found a line for a specific CPU like:
                //    cpu1 90470 3217 30294 291392 17250 0 3242 0 0 0
                // process it
                const char* pStart = &line[3];

                /*
                from the manpage:
                Since 0 can legitimately be returned on both success and failure, the calling program should set errno
                to 0 before the call, and then determine if an error occurred by checking whether errno has a nonzero
                value after the call.
                */
                errno = 0;
                unsigned long cpuno = strtoul(pStart, NULL, 10);
                if (errno == 0)
                    cpu_indexes.insert(cpuno);
            }
        }

        line = cpu_stat.get_next_line();
    }

    return cpu_indexes.size();
}

void CMonitorSystem::get_list_monitored_files(std::set<std::string>& list)
{
    list.insert(m_uptime.get_file());
    list.insert(m_loadavg.get_file());
    if (m_pCfg->m_nCollectFlags & PK_BAREMETAL_CPU)
        list.insert(m_cpu_stat.get_file());
    if (m_pCfg->m_nCollectFlags & PK_BAREMETAL_MEMORY) {
        list.insert(m_meminfo.get_file());
        list.insert(m_vmstat.get_file());
    }
    if (m_pCfg->m_nCollectFlags & PK_BAREMETAL_DISK)
        list.insert(m_disk_stat.get_file());
}
