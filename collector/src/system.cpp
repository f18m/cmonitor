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
#include "utils.h"
#include <assert.h>
#include <mntent.h>
#include <sys/vfs.h>

// ----------------------------------------------------------------------------------
// Macros
// ----------------------------------------------------------------------------------

#define DELTA_TOTAL(stat) ((float)(stat - total_cpu.stat) / (float)elapsed_sec / ((float)(max_cpu_count + 1.0)))

// ----------------------------------------------------------------------------------
// CMonitorSystem
// ----------------------------------------------------------------------------------

void CMonitorSystem::init() { m_cpu_stat.set_file("/proc/stat"); }

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
        for (int i = 0; i < m_cpu_count; i++) {
            if (!is_monitored_cpu(i))
                continue;

#define DELTA_LOGICAL(stat) ((double)(new_values[i].stat - m_cpu_stat_prev_values[i].stat) / elapsed_sec)

            m_pOutput->psubsection_start(fmt::format("cpu{:d}", i).c_str());
            switch (output_opts) {
            case PF_NONE:
                assert(0);
                break;
            case PF_ALL:
            case PF_USED_BY_CHART_SCRIPT_ONLY:
                m_pOutput->pdouble("user", DELTA_LOGICAL(user)); /* counter */
                m_pOutput->pdouble("nice", DELTA_LOGICAL(nice)); /* counter */
                m_pOutput->pdouble("sys", DELTA_LOGICAL(sys)); /* counter */
                m_pOutput->pdouble("idle", DELTA_LOGICAL(idle)); /* counter */
                m_pOutput->pdouble("iowait", DELTA_LOGICAL(iowait)); /* counter */
                m_pOutput->pdouble("hardirq", DELTA_LOGICAL(hardirq)); /* counter */
                m_pOutput->pdouble("softirq", DELTA_LOGICAL(softirq)); /* counter */
                m_pOutput->pdouble("steal", DELTA_LOGICAL(steal)); /* counter */
                m_pOutput->pdouble("guest", DELTA_LOGICAL(guest)); /* counter */
                m_pOutput->pdouble("guestnice", DELTA_LOGICAL(guestnice)); /* counter */
                break;
            }
            m_pOutput->psubsection_end();

#undef DELTA_LOGICAL
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
    for (int i = 0; i < m_cpu_count; i++)
        m_cpu_stat_prev_values[i] = new_values[i];
}

/*
read /proc/diskstats
*/
void CMonitorSystem::sample_diskstats(double elapsed_sec, OutputFields output_opts)
{
    // please refer https://www.kernel.org/doc/Documentation/iostats.txt

    struct diskinfo {
        long dk_major;
        long dk_minor;
        char dk_name[128];

        // reads
        long long dk_reads; // Field 1: This is the total number of reads completed successfully.
        long long dk_rmerge; // Field 2: Reads and writes which are adjacent to each other may be merged for efficiency.
        long long
            dk_rkb; // Field 3: This is the total number of Kbytes read successfully. [converted by us from sectors]
        long long dk_rmsec; // Field 4: This is the total number of milliseconds spent by all reads

        // writes
        long long dk_writes; // Same as Field 1 but for writes
        long long dk_wmerge; // Same as Field 2 but for writes
        long long dk_wkb; // Same as Field 3 but for writes
        long long dk_wmsec; // Same as Field 4 but for writes

        // others
        long long dk_inflight; // Field 9: number of I/Os currently in progress
        long long dk_time; // Field 10: This field increases so long as field 9 is nonzero. (milliseconds) [converted in
                           // percentage]
        long long dk_backlog; // Field 11: weighted # of milliseconds spent doing I/Os

        // computed by ourselves:
        long long dk_xfers; // sum of number of read/write operations
        long long dk_bsize;
    };

    static struct diskinfo current;
    static struct diskinfo* previous;
    static long disks_found = 0, disks_sampled = 0;
    static FILE* fp = 0;
    char buf[1024];
    int dk_stats;

    /* popen variables */
    FILE* pop;
    char tmpstr[1024 + 1];
    long i;
    long j;
    long len;
    int filtered_out = 0;

    DEBUGLOG_FUNCTION_START();
    if (fp == (FILE*)0) {
        /* Just count the number of disks */
        pop = popen("lsblk --nodeps --output NAME,TYPE --raw 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            tmpstr[0] = 0;
            disks_found = 0;
            if (fgets(tmpstr, 127, pop)) {
                for (;; disks_found++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 127, pop) == NULL)
                        break;
                    // CMonitorLogger::instance()->LogDebug("DEBUG %ld disks - %s\n", disks, tmpstr);
                }
            }
            pclose(pop);
        } else {
            CMonitorLogger::instance()->LogErrorWithErrno("failed to list number of disks using 'lsblk'");
            disks_found = 0;
        }
        // CMonitorLogger::instance()->LogDebug("DEBUG %ld disks\n", disks);
        previous = (diskinfo*)calloc(sizeof(struct diskinfo), disks_found);

        pop = popen("lsblk --nodeps --output NAME,TYPE --raw 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            if (fgets(tmpstr, 70, pop)) {
                for (i = 0; i < disks_found; i++) {
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
                        strcpy(previous[disks_sampled].dk_name, tmpstr);
                        disks_sampled++;
                    } else {
                        CMonitorLogger::instance()->LogDebug("Discarding disk %s\n", tmpstr);
                        /* loop**** disks are not real */
                    }
                }
            }
            pclose(pop);
        } else
            disks_sampled = 0;

        if ((fp = fopen("/proc/diskstats", "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("failed to open - /proc/diskstats");
            return;
        }
    } else
        rewind(fp);

    if (output_opts != PF_NONE)
        m_pOutput->psection_start("disks");
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */

        // CMonitorLogger::instance()->LogDebug("DISKSTATS: \"%s\"", buf);

        /* zero the data ready for reading */
        bzero(&current, sizeof(struct diskinfo));

        // try to read all the 14 fields
        dk_stats = sscanf(&buf[0], "%ld %ld %s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
            &current.dk_major, &current.dk_minor, // force newline
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

        for (i = 0; i < disks_found; i++) {
            // CMonitorLogger::instance()->LogDebug("DEBUG disks new %s old %s\n", current.dk_name,
            // previous[i].dk_name);
            if (!strcmp(current.dk_name, previous[i].dk_name)) {

                if (!filtered_out && output_opts != PF_NONE) {
                    m_pOutput->psubsection_start(current.dk_name);

                    switch (output_opts) {
                    case PF_NONE:
                        assert(0);
                        break;

                    case PF_ALL:
                        m_pOutput->pdouble("reads", (current.dk_reads - previous[i].dk_reads) / elapsed_sec);
                        m_pOutput->pdouble("rmerge", (current.dk_rmerge - previous[i].dk_rmerge) / elapsed_sec);
                        m_pOutput->pdouble("rkb", (current.dk_rkb - previous[i].dk_rkb) / elapsed_sec);
                        m_pOutput->pdouble("rmsec", (current.dk_rmsec - previous[i].dk_rmsec) / elapsed_sec);

                        m_pOutput->pdouble("writes", (current.dk_writes - previous[i].dk_writes) / elapsed_sec);
                        m_pOutput->pdouble("wmerge", (current.dk_wmerge - previous[i].dk_wmerge) / elapsed_sec);
                        m_pOutput->pdouble("wkb", (current.dk_wkb - previous[i].dk_wkb) / elapsed_sec);
                        m_pOutput->pdouble("wmsec", (current.dk_wmsec - previous[i].dk_wmsec) / elapsed_sec);

                        m_pOutput->plong("inflight", current.dk_inflight);
                        m_pOutput->pdouble("time", (current.dk_time - previous[i].dk_time) / elapsed_sec);
                        m_pOutput->pdouble("backlog", (current.dk_backlog - previous[i].dk_backlog) / elapsed_sec);
                        m_pOutput->pdouble("xfers", (current.dk_xfers - previous[i].dk_xfers) / elapsed_sec);
                        m_pOutput->plong("bsize", current.dk_bsize);
                        break;

                    case PF_USED_BY_CHART_SCRIPT_ONLY:
                        m_pOutput->pdouble("rkb", (current.dk_rkb - previous[i].dk_rkb) / elapsed_sec);
                        m_pOutput->pdouble("wkb", (current.dk_wkb - previous[i].dk_wkb) / elapsed_sec);
                        break;
                    }

                    m_pOutput->psubsection_end();
                }
                memcpy(&previous[i], &current, sizeof(struct diskinfo));
                break; /* once found stop searching */
            }
        }
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

    DEBUGLOG_FUNCTION_START();

    if (first_time) {
        /* popen variables */
        FILE* pop;
        char tmpstr[1024 + 1];

        // find only interfaces that are UP
        pop = popen("/sbin/ifconfig -s 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            if (fgets(tmpstr, 1024, pop)) {
                for (int i = 0;; i++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 1024, pop) == NULL)
                        break;
                    tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
                    int len = strlen(tmpstr);
                    for (int j = 0; j < len; j++)
                        if (tmpstr[j] == ' ')
                            tmpstr[j] = 0;

                    if (strncmp(tmpstr, "veth", 4) != 0) {
                        m_network_interfaces_up.insert(tmpstr);
                    } else {
                        /* veth**** interfaces are not real */
                        CMonitorLogger::instance()->LogDebug("Discarding net interface %s\n", tmpstr);
                    }
                }
            }
            pclose(pop);
        }
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
    read_net_dev("/proc/net/dev", m_network_interfaces_up, new_stats);

    if (output_opts != PF_NONE) {
        m_pOutput->psection_start("network_interfaces");
        output_net_dev_stats(m_pOutput, elapsed_sec, new_stats, m_previous_netinfo, output_opts);
        m_pOutput->psection_end();
    }

    // finally remember the last sampled stats:
    m_previous_netinfo = new_stats;
}

/* static */
bool CMonitorSystem::read_net_dev(
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
#define CURRENT(member) (current.member)
#define PREVIOUS(member) (previous.member)
#define DELTA(member) (CURRENT(member) - PREVIOUS(member))

    for (auto it : new_stats) {
        const std::string& name = it.first;
        const netinfo_t& current = it.second;

        const auto it_prev = prev_stats.find(name);
        if (it_prev == prev_stats.end())
            continue; // looks like a new interface, skip it in this sample if we cannot take any delta

        // found previous values, statistics can now be generated:
        const netinfo_t& previous = it_prev->second;
        m_pOutput->psubsection_start(name.c_str());

        switch (output_opts) {
        case PF_NONE:
            assert(0);
            break;

        case PF_ALL:
            m_pOutput->plong("ibytes", DELTA(if_ibytes) / elapsed_sec);
            m_pOutput->plong("ipackets", DELTA(if_ipackets) / elapsed_sec);
            m_pOutput->plong("ierrs", DELTA(if_ierrs) / elapsed_sec);
            m_pOutput->plong("idrop", DELTA(if_idrop) / elapsed_sec);
            m_pOutput->plong("ififo", DELTA(if_ififo) / elapsed_sec);
            m_pOutput->plong("iframe", DELTA(if_iframe) / elapsed_sec);

            m_pOutput->plong("obytes", DELTA(if_obytes) / elapsed_sec);
            m_pOutput->plong("opackets", DELTA(if_opackets) / elapsed_sec);
            m_pOutput->plong("oerrs", DELTA(if_oerrs) / elapsed_sec);
            m_pOutput->plong("odrop", DELTA(if_odrop) / elapsed_sec);
            m_pOutput->plong("ofifo", DELTA(if_ofifo) / elapsed_sec);

            m_pOutput->plong("ocolls", DELTA(if_ocolls) / elapsed_sec);
            m_pOutput->plong("ocarrier", DELTA(if_ocarrier) / elapsed_sec);
            break;

        case PF_USED_BY_CHART_SCRIPT_ONLY:
            m_pOutput->plong("ibytes", DELTA(if_ibytes) / elapsed_sec);
            m_pOutput->plong("obytes", DELTA(if_obytes) / elapsed_sec);
            m_pOutput->plong("ipackets", DELTA(if_ipackets) / elapsed_sec);
            m_pOutput->plong("opackets", DELTA(if_opackets) / elapsed_sec);
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
    static FILE* fp = 0;
    char buf[1024 + 1];
    int count;
    long long value;
    long long days;
    long long hours;

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/proc/uptime", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);

    if (fgets(buf, 1024, fp) != NULL) {
        count = sscanf(buf, "%lld", &value);
        if (count == 1) {
            m_pOutput->psection_start("proc_uptime");
            m_pOutput->plong("total_seconds", value);
            days = value / 60 / 60 / 24;
            hours = (value - (days * 60 * 60 * 24)) / 60 / 60;
            m_pOutput->plong("days", days);
            m_pOutput->plong("hours", hours);
            m_pOutput->psection_end();
        }
    }
}

void CMonitorSystem::sample_loadavg()
{
    char buf[1024 + 1];
    int count;
    float load_avg_1min;
    float load_avg_5min;
    float load_avg_15min;
    FILE* fp;

    DEBUGLOG_FUNCTION_START();

    if ((fp = fopen("/proc/loadavg", "r")) == NULL) {
        return;
    }

    if (fgets(buf, 1024, fp) != NULL) {
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

        count = sscanf(buf, "%f %f %f", &load_avg_1min, &load_avg_5min, &load_avg_15min);
        if (count == 3) {
            m_pOutput->psection_start("proc_loadavg");
            m_pOutput->pdouble("load_avg_1min", load_avg_1min);
            m_pOutput->pdouble("load_avg_5min", load_avg_5min);
            m_pOutput->pdouble("load_avg_15min", load_avg_15min);
            m_pOutput->psection_end();
        }
    }

    fclose(fp);
}

void CMonitorSystem::sample_filesystems()
{
    FILE* fp;
    struct mntent* fs;
    struct statfs vfs;

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
