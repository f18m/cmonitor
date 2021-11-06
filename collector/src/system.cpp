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
#include "output_frontend.h"
#include "utils.h"
#include "logger.h"
#include <assert.h>
#include <mntent.h>
#include <sys/vfs.h>

#define MAX_LOGICAL_CPU (256)
#define DELTA_TOTAL(stat) ((float)(stat - total_cpu.stat) / (float)elapsed_sec / ((float)(max_cpu_count + 1.0)))
#define DELTA_LOGICAL(stat) ((float)(stat - logical_cpu[cpuno].stat) / (float)elapsed_sec)


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
        g_output.psubsection_start("cpu_total");
        switch (output_opts) {
        case PF_NONE:
            assert(0);
            break;
        case PF_ALL:
        case PF_USED_BY_CHART_SCRIPT_ONLY:
            g_output.pdouble("user", DELTA_TOTAL(user)); /* incrementing counter */
            g_output.pdouble("nice", DELTA_TOTAL(nice)); /* incrementing counter */
            g_output.pdouble("sys", DELTA_TOTAL(sys)); /* incrementing counter */
            g_output.pdouble("idle", DELTA_TOTAL(idle)); /* incrementing counter */
            /*			g_output.pdouble("DEBUG IDLE idle: %lld %lld %lld\n", total_cpu.idle,
             * idle, idle-total_cpu.idle); */
            g_output.pdouble("iowait", DELTA_TOTAL(iowait)); /* incrementing counter */
            g_output.pdouble("hardirq", DELTA_TOTAL(hardirq)); /* incrementing counter */
            g_output.pdouble("softirq", DELTA_TOTAL(softirq)); /* incrementing counter */
            g_output.pdouble("steal", DELTA_TOTAL(steal)); /* incrementing counter */
            g_output.pdouble("guest", DELTA_TOTAL(guest)); /* incrementing counter */
            g_output.pdouble("guestnice", DELTA_TOTAL(guestnice)); /* incrementing counter */
            break;
        }
        g_output.psubsection_end();
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

int CMonitorSystem::proc_stat_cpu_index(const char* cpu_data, double elapsed_sec, OutputFields output_opts,
    cpu_specs_t* logical_cpu)
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
    int cpuno;

    // see http://man7.org/linux/man-pages/man5/proc.5.html
    // Look for "/proc/stat"

    int count = sscanf(cpu_data, /* cpuNNN USER*/
        "%d %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", // force newline
        &cpuno, &user, &nice, &sys, &idle, &iowait, &hardirq, &softirq, &steal, &guest, &guestnice);
    if (count != 11)
        return -1;

    if (cpuno >= MAX_LOGICAL_CPU)
        return -1;
    if (!is_monitored_cpu(cpuno))
        return -1;

    if (output_opts != PF_NONE) {
        char label[512];
        sprintf(label, "cpu%d", cpuno);
        g_output.psubsection_start(label);

        switch (output_opts) {
        case PF_NONE:
            assert(0);
            break;
        case PF_ALL:
        case PF_USED_BY_CHART_SCRIPT_ONLY:
            g_output.pdouble("user", DELTA_LOGICAL(user)); /* counter */
            g_output.pdouble("nice", DELTA_LOGICAL(nice)); /* counter */
            g_output.pdouble("sys", DELTA_LOGICAL(sys)); /* counter */
            g_output.pdouble("idle", DELTA_LOGICAL(idle)); /* counter */
            g_output.pdouble("iowait", DELTA_LOGICAL(iowait)); /* counter */
            g_output.pdouble("hardirq", DELTA_LOGICAL(hardirq)); /* counter */
            g_output.pdouble("softirq", DELTA_LOGICAL(softirq)); /* counter */
            g_output.pdouble("steal", DELTA_LOGICAL(steal)); /* counter */
            g_output.pdouble("guest", DELTA_LOGICAL(guest)); /* counter */
            g_output.pdouble("guestnice", DELTA_LOGICAL(guestnice)); /* counter */
            break;
        }
        g_output.psubsection_end();
    }
    logical_cpu[cpuno].user = user;
    logical_cpu[cpuno].nice = nice;
    logical_cpu[cpuno].sys = sys;
    logical_cpu[cpuno].idle = idle;
    logical_cpu[cpuno].iowait = iowait;
    logical_cpu[cpuno].hardirq = hardirq;
    logical_cpu[cpuno].softirq = softirq;
    logical_cpu[cpuno].steal = steal;
    logical_cpu[cpuno].guest = guest;
    logical_cpu[cpuno].guestnice = guestnice;

    return cpuno;
}

/*
read /proc/stat
*/
void CMonitorSystem::proc_stat(double elapsed_sec, OutputFields output_opts)
{
    int count;
    long long value;

    /* Static data */
    static FILE* fp = 0;
    static char line[8192];
    static int max_cpu_count;

    static long long old_ctxt;
    static long long old_processes;
    // static cpu_specs_t total_cpu;
    static cpu_specs_t logical_cpu[MAX_LOGICAL_CPU];

    DEBUGLOG_FUNCTION_START();
    g_logger.LogDebug("proc_stat(%.4f) max_cpu_count=%d\n", elapsed_sec, max_cpu_count);
    if (fp == 0) {
        if ((fp = fopen("/proc/stat", "r")) == NULL) {
            g_logger.LogErrorWithErrno("failed to open file /proc/stat");
            fp = 0;
            return;
        }
    } else
        rewind(fp);

    if (output_opts != PF_NONE)
        g_output.psection_start("stat");

    while (fgets(line, 1000, fp) != NULL) {
        if (!strncmp(line, "cpu", 3)) {
            if (line[3] == ' ') {
                // found the summary line for ALL cpus together... skip it
                continue;
            } else {
                // found a line like:
                //    cpu1 90470 3217 30294 291392 17250 0 3242 0 0 0

                int cpuno = proc_stat_cpu_index(&line[3], elapsed_sec, output_opts, logical_cpu);
                if (cpuno > max_cpu_count)
                    max_cpu_count = cpuno;
            }
        } else if (!strncmp(line, "ctxt", 4)) {
            value = 0;
            count = sscanf(&line[5], "%lld", &value); /* counter */
            if (count == 1) {
                if (output_opts != PF_NONE) {
                    g_output.psubsection_start("counters");
                    g_output.pdouble("ctxt", ((double)(value - old_ctxt) / elapsed_sec));
                }
                old_ctxt = value;
            }
        } else if (!strncmp(line, "btime", 5)) {
            value = 0;
            count = sscanf(&line[6], "%lld", &value); /* seconds since boot */
            if (output_opts != PF_NONE)
                g_output.plong("btime", value);
        } else if (!strncmp(line, "processes", 9)) {
            value = 0;
            count = sscanf(&line[10], "%lld", &value); /* counter  actually forks */
            if (output_opts != PF_NONE)
                g_output.pdouble("processes_forks", ((double)(value - old_processes) / elapsed_sec));
            old_processes = value;
        } else if (!strncmp(line, "procs_running", 13)) {
            value = 0;
            count = sscanf(&line[14], "%lld", &value);
            if (output_opts != PF_NONE)
                g_output.plong("procs_running", value);
        } else if (!strncmp(line, "procs_blocked", 13)) {
            value = 0;
            count = sscanf(&line[14], "%lld", &value);
            if (output_opts != PF_NONE) {
                g_output.plong("procs_blocked", value);
                g_output.psubsection_end();
            }
        }
    }
    if (output_opts != PF_NONE)
        g_output.psection_end();
}

/*
read /proc/diskstats
*/
void CMonitorSystem::proc_diskstats(double elapsed_sec, OutputFields output_opts)
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
                    // g_logger.LogDebug("DEBUG %ld disks - %s\n", disks, tmpstr);
                }
            }
            pclose(pop);
        } else {
            g_logger.LogErrorWithErrno("failed to list number of disks using 'lsblk'");
            disks_found = 0;
        }
        // g_logger.LogDebug("DEBUG %ld disks\n", disks);
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
                        // g_logger.LogDebug("DEBUG saved %ld %s disk name\n", i, previous[i].dk_name);
                        strcpy(previous[disks_sampled].dk_name, tmpstr);
                        disks_sampled++;
                    } else {
                        g_logger.LogDebug("Discarding disk %s\n", tmpstr);
                        /* loop**** disks are not real */
                    }
                }
            }
            pclose(pop);
        } else
            disks_sampled = 0;

        if ((fp = fopen("/proc/diskstats", "r")) == NULL) {
            g_logger.LogErrorWithErrno("failed to open - /proc/diskstats");
            return;
        }
    } else
        rewind(fp);

    if (output_opts != PF_NONE)
        g_output.psection_start("disks");
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */

        // g_logger.LogDebug("DISKSTATS: \"%s\"", buf);

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
            g_logger.LogError("disk sscanf wanted 14 but returned=%d line=%s\n", dk_stats, buf);

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
            // g_logger.LogDebug("DEBUG disks new %s old %s\n", current.dk_name, previous[i].dk_name);
            if (!strcmp(current.dk_name, previous[i].dk_name)) {

                if (!filtered_out && output_opts != PF_NONE) {
                    g_output.psubsection_start(current.dk_name);

                    switch (output_opts) {
                    case PF_NONE:
                        assert(0);
                        break;

                    case PF_ALL:
                        g_output.pdouble("reads", (current.dk_reads - previous[i].dk_reads) / elapsed_sec);
                        g_output.pdouble("rmerge", (current.dk_rmerge - previous[i].dk_rmerge) / elapsed_sec);
                        g_output.pdouble("rkb", (current.dk_rkb - previous[i].dk_rkb) / elapsed_sec);
                        g_output.pdouble("rmsec", (current.dk_rmsec - previous[i].dk_rmsec) / elapsed_sec);

                        g_output.pdouble("writes", (current.dk_writes - previous[i].dk_writes) / elapsed_sec);
                        g_output.pdouble("wmerge", (current.dk_wmerge - previous[i].dk_wmerge) / elapsed_sec);
                        g_output.pdouble("wkb", (current.dk_wkb - previous[i].dk_wkb) / elapsed_sec);
                        g_output.pdouble("wmsec", (current.dk_wmsec - previous[i].dk_wmsec) / elapsed_sec);

                        g_output.plong("inflight", current.dk_inflight);
                        g_output.pdouble("time", (current.dk_time - previous[i].dk_time) / elapsed_sec);
                        g_output.pdouble("backlog", (current.dk_backlog - previous[i].dk_backlog) / elapsed_sec);
                        g_output.pdouble("xfers", (current.dk_xfers - previous[i].dk_xfers) / elapsed_sec);
                        g_output.plong("bsize", current.dk_bsize);
                        break;

                    case PF_USED_BY_CHART_SCRIPT_ONLY:
                        g_output.pdouble("rkb", (current.dk_rkb - previous[i].dk_rkb) / elapsed_sec);
                        g_output.pdouble("wkb", (current.dk_wkb - previous[i].dk_wkb) / elapsed_sec);
                        break;
                    }

                    g_output.psubsection_end();
                }
                memcpy(&previous[i], &current, sizeof(struct diskinfo));
                break; /* once found stop searching */
            }
        }
    }
    if (output_opts != PF_NONE)
        g_output.psection_end();
}

/*
 read /proc/net/dev
 */
void CMonitorSystem::proc_net_dev(double elapsed_sec, OutputFields output_opts)
{
    struct netinfo {
        char if_name[128];
        long long if_ibytes;
        long long if_ipackets;
        long long if_ierrs;
        long long if_idrop;
        long long if_ififo;
        long long if_iframe;
        long long if_obytes;
        long long if_opackets;
        long long if_oerrs;
        long long if_odrop;
        long long if_ofifo;
        long long if_ocolls;
        long long if_ocarrier;
    };

    static struct netinfo current;
    static struct netinfo* previous;
    static long interfaces_found = 0, interfaces_sampled = 0;
    long long junk;

    static FILE* fp = 0;
    char buf[1024];
    int ret;

    /* popen variables */
    FILE* pop;
    char tmpstr[1024 + 1];
    long i;
    long j;
    long len;

    DEBUGLOG_FUNCTION_START();
    if (fp == (FILE*)0) {
        /* Just count the number of UP network interfaces */
        pop = popen("/sbin/ifconfig -s 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            tmpstr[0] = 0;
            interfaces_found = 0;
            if (fgets(tmpstr, 1024, pop)) {
                for (;; interfaces_found++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 1024, pop) == NULL)
                        break;
                    // g_logger.LogDebug("DEBUG %ld interfaces - %s\n", interfaces, tmpstr);
                }
            }
            pclose(pop);
        } else {
            g_logger.LogErrorWithErrno("failed to list network devices using /sbin/ifconfig");
            interfaces_found = 0;
        }
        // g_logger.LogDebug("DEBUG %ld intergaces\n", interfaces);
        previous = (netinfo*)calloc(sizeof(struct netinfo), interfaces_found);

        pop = popen("/sbin/ifconfig -s 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            if (fgets(tmpstr, 1024, pop)) {
                for (i = 0; i < interfaces_found; i++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 1024, pop) == NULL)
                        break;
                    tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
                    len = strlen(tmpstr);
                    for (j = 0; j < len; j++)
                        if (tmpstr[j] == ' ')
                            tmpstr[j] = 0;

                    if (strncmp(tmpstr, "veth", 4) != 0) {
                        strcpy(previous[interfaces_sampled].if_name, tmpstr);
                        interfaces_sampled++;
                    } else {
                        /* veth**** interfaces are not real */
                        g_logger.LogDebug("Discarding net interface %s\n", tmpstr);
                    }
                }
            }
            pclose(pop);
        } else
            interfaces_found = 0;

        if ((fp = fopen("/proc/net/dev", "r")) == NULL) {
            g_logger.LogErrorWithErrno("failed to open /proc/net/dev");
            return;
        }
    } else
        rewind(fp);

    if (interfaces_found == 0 || interfaces_sampled == 0)
        return; // this happens in e.g. Docker containers having no network

    if (fgets(buf, 1024, fp) == NULL)
        return; /* throw away the header line */
    if (fgets(buf, 1024, fp) == NULL)
        return; /* throw away the header line */

    if (output_opts != PF_NONE)
        g_output.psection_start("network_interfaces");
    while (fgets(buf, 1024, fp) != NULL) {
        strip_spaces(buf);

        bzero(&current, sizeof(struct netinfo));
        ret = sscanf(&buf[0], "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            (char*)current.if_name, &current.if_ibytes, &current.if_ipackets, &current.if_ierrs, &current.if_idrop,
            &current.if_ififo, &current.if_iframe, &junk, &junk, &current.if_obytes, &current.if_opackets,
            &current.if_oerrs, &current.if_odrop, &current.if_ofifo, &current.if_ocolls, &current.if_ocarrier);

        if (ret != 16) {
            g_logger.LogError("net sscanf wanted 16 returned = %d line=%s\n", ret, (char*)buf);
        } else {
            for (i = 0; i < interfaces_found; i++) {
                // g_logger.LogDebug("DEBUG: i=%ld current.if_name=%s, previous=%s interfaces=%ld\n", i,
                // *current.if_name, previous[i].if_name, interfaces);
                if (!strcmp(current.if_name, previous[i].if_name)) {

                    if (output_opts != PF_NONE) {
                        g_output.psubsection_start(current.if_name);
                        switch (output_opts) {
                        case PF_NONE:
                            assert(0);
                            break;

                        case PF_ALL:
                            g_output.pdouble("ibytes", (current.if_ibytes - previous[i].if_ibytes) / elapsed_sec);
                            g_output.pdouble("ipackets", (current.if_ipackets - previous[i].if_ipackets) / elapsed_sec);
                            g_output.pdouble("ierrs", (current.if_ierrs - previous[i].if_ierrs) / elapsed_sec);
                            g_output.pdouble("idrop", (current.if_idrop - previous[i].if_idrop) / elapsed_sec);
                            g_output.pdouble("ififo", (current.if_ififo - previous[i].if_ififo) / elapsed_sec);
                            g_output.pdouble("iframe", (current.if_iframe - previous[i].if_iframe) / elapsed_sec);

                            g_output.pdouble("obytes", (current.if_obytes - previous[i].if_obytes) / elapsed_sec);
                            g_output.pdouble("opackets", (current.if_opackets - previous[i].if_opackets) / elapsed_sec);
                            g_output.pdouble("oerrs", (current.if_oerrs - previous[i].if_oerrs) / elapsed_sec);
                            g_output.pdouble("odrop", (current.if_odrop - previous[i].if_odrop) / elapsed_sec);
                            g_output.pdouble("ofifo", (current.if_ofifo - previous[i].if_ofifo) / elapsed_sec);

                            g_output.pdouble("ocolls", (current.if_ocolls - previous[i].if_ocolls) / elapsed_sec);
                            g_output.pdouble("ocarrier", (current.if_ocarrier - previous[i].if_ocarrier) / elapsed_sec);
                            break;

                        case PF_USED_BY_CHART_SCRIPT_ONLY:
                            g_output.pdouble("ibytes", (current.if_ibytes - previous[i].if_ibytes) / elapsed_sec);
                            g_output.pdouble("obytes", (current.if_obytes - previous[i].if_obytes) / elapsed_sec);
                            g_output.pdouble("ipackets", (current.if_ipackets - previous[i].if_ipackets) / elapsed_sec);
                            g_output.pdouble("opackets", (current.if_opackets - previous[i].if_opackets) / elapsed_sec);
                            break;
                        }
                        g_output.psubsection_end();
                    }
                    memcpy(&previous[i], &current, sizeof(struct netinfo));
                    break; /* once found stop searching */
                }
            }
        }
    }
    if (output_opts != PF_NONE)
        g_output.psection_end();
}

/*
 read /proc/uptime
*/
void CMonitorSystem::proc_uptime()
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
            g_output.psection_start("proc_uptime");
            g_output.plong("total_seconds", value);
            days = value / 60 / 60 / 24;
            hours = (value - (days * 60 * 60 * 24)) / 60 / 60;
            g_output.plong("days", days);
            g_output.plong("hours", hours);
            g_output.psection_end();
        }
    }
}

void CMonitorSystem::proc_loadavg()
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
            g_output.psection_start("proc_loadavg");
            g_output.pdouble("load_avg_1min", load_avg_1min);
            g_output.pdouble("load_avg_5min", load_avg_5min);
            g_output.pdouble("load_avg_15min", load_avg_15min);
            g_output.psection_end();
        }
    }

    fclose(fp);
}

void CMonitorSystem::proc_filesystems()
{
    FILE* fp;
    struct mntent* fs;
    struct statfs vfs;

    DEBUGLOG_FUNCTION_START();
    if ((fp = setmntent("/etc/mtab", "r")) == NULL)
        g_logger.LogError("setmntent(\"/etc/mtab\", \"r\") failed");

    g_output.psection_start("filesystems");
    while ((fs = getmntent(fp)) != NULL) {
        // NOTE: /dev/loop* filesystems are not real filesystems - e.g. on Ubuntu they are used for SNAPs
        if (fs->mnt_fsname[0] == '/' && strncmp(fs->mnt_fsname, "/dev/loop", 9) != 0) {
            if (statfs(fs->mnt_dir, &vfs) != 0) {
                g_logger.LogErrorWithErrno("%s: statfs failed: %d\n", fs->mnt_dir, errno);
            }
            // g_logger.LogDebug("%s, mounted on %s:\n", fs->mnt_dir, fs->mnt_fsname);

            g_output.psubsection_start(fs->mnt_fsname);
            g_output.pstring("fs_dir", fs->mnt_dir);
            g_output.pstring("fs_type", fs->mnt_type);
            g_output.pstring("fs_opts", fs->mnt_opts);

            g_output.plong("fs_freqs", fs->mnt_freq);
            g_output.plong("fs_passno", fs->mnt_passno);
            g_output.plong("fs_bsize", vfs.f_bsize);
            g_output.plong("fs_size_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024);
            g_output.plong("fs_free_mb", (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            g_output.plong(
                "fs_used_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024 - (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            g_output.pdouble(
                "fs_full_percent", ((double)vfs.f_blocks - (double)vfs.f_bfree) / (double)vfs.f_blocks * (double)100.0);
            /*
             * g_output.pdouble("fs_full_percent", ((vfs.f_blocks * vfs.f_bsize) - (vfs.f_bfree * vfs.f_bsize) ) /
             *					(vfs.f_blocks * vfs.f_bsize) * 100.00);
             */
            g_output.plong("fs_avail", (vfs.f_bavail * vfs.f_bsize) / 1024 / 1024);
            g_output.plong("fs_files", vfs.f_files);
            g_output.plong("fs_files_free", vfs.f_ffree);
            g_output.plong("fs_namelength", vfs.f_namelen);
            g_output.psubsection_end();
        }
    }
    g_output.psection_end();
    endmntent(fp);
}
