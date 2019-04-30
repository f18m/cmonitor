/*
 * njmon_proc_stats.cpp -- collects Linux performance data and generates JSON format data.
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

#include "njmon.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <memory.h>
#include <mntent.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

#define ADD_LABEL(ch) label[labelch++] = ch
#define ADD_NUM(ch) numstr[numstrch++] = ch
#define MAX_LOGICAL_CPU 256
#define DELTA_TOTAL(stat) ((float)(stat - total_cpu.stat) / (float)elapsed / ((float)(max_cpuno + 1.0)))
#define DELTA_LOGICAL(stat) ((float)(stat - logical_cpu[cpuno].stat) / (float)elapsed)

/*
read files in format
name number
name: number
name: number kB
*/
void NjmonCollectorApp::read_data_number(const char* statname)
{
    FILE* fp = 0;
    char line[1024];
    char filename[1024];
    char label[512];
    char number[512];
    int i;
    int len;

    DEBUGLOG_FUNCTION_START();
    sprintf(filename, "/proc/%s", statname);
    if ((fp = fopen(filename, "r")) == NULL) {
        LogError("Failed to open performance file %s", filename);
        return;
    }
    sprintf(label, "proc_%s", statname);
    psection(label);
    while (fgets(line, 1000, fp) != NULL) {
        len = strlen(line);
        for (i = 0; i < len; i++) {
            if (line[i] == '(')
                line[i] = '_';
            if (line[i] == ')')
                line[i] = ' ';
            if (line[i] == ':')
                line[i] = ' ';
            if (line[i] == '\n')
                line[i] = 0;
        }
        sscanf(line, "%s %s", label, number);
        /*printf("read_data_numer(%s) |%s| |%s|=%lld\n", statname,label,numstr,atoll(numstr));*/
        plong(label, atoll(number));
    }
    psectionend();
    (void)fclose(fp);
}

/*
read /proc/stat and unpick
*/
void NjmonCollectorApp::proc_stat(double elapsed, bool onlyCgroupAllowedCpus, bool print)
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
    // int cpu_total;
    int count;
    int cpuno;
    long long value;

    /* Static data */
    static FILE* fp = 0;
    static char line[8192];
    static int max_cpuno;

    /* structure to recall previous values */
    struct utilisation {
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
    };

    static long long old_ctxt;
    static long long old_processes;
    static struct utilisation total_cpu;
    static struct utilisation logical_cpu[MAX_LOGICAL_CPU];
    char label[512];

    DEBUGLOG_FUNCTION_START();
    /* printf("DEBUG\t--> proc_stat(%.4f, %d) max_cpuno=%d\n",elapsed, print,max_cpuno); */
    if (fp == 0) {
        if ((fp = fopen("/proc/stat", "r")) == NULL) {
            LogError("failed to open file /proc/stat");
            fp = 0;
            return;
        }
    } else
        rewind(fp);

    if (print)
        psection("stat");
    while (fgets(line, 1000, fp) != NULL) {
        // len = strlen(line);

        if (!strncmp(line, "cpu", 3)) {
            if (!strncmp(line, "cpu ", 4)) {
                if (!onlyCgroupAllowedCpus) {
                    // cpu_total = 1;
                    count = sscanf(&line[4], /* cpu USER */
                        "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", &user, &nice, &sys, &idle, &iowait,
                        &hardirq, &softirq, &steal, &guest, &guestnice);
                    if (print) {
                        psub("cpu_total");
                        pdouble("user", DELTA_TOTAL(user)); /* incrementing counter */
                        pdouble("nice", DELTA_TOTAL(nice)); /* incrementing counter */
                        pdouble("sys", DELTA_TOTAL(sys)); /* incrementing counter */
                        pdouble("idle", DELTA_TOTAL(idle)); /* incrementing counter */
                        /*			pdouble("DEBUG IDLE idle: %lld %lld %lld\n", total_cpu.idle, idle,
                         * idle-total_cpu.idle); */
                        pdouble("iowait", DELTA_TOTAL(iowait)); /* incrementing counter */
                        pdouble("hardirq", DELTA_TOTAL(hardirq)); /* incrementing counter */
                        pdouble("softirq", DELTA_TOTAL(softirq)); /* incrementing counter */
                        pdouble("steal", DELTA_TOTAL(steal)); /* incrementing counter */
                        pdouble("guest", DELTA_TOTAL(guest)); /* incrementing counter */
                        pdouble("guestnice", DELTA_TOTAL(guestnice)); /* incrementing counter */
                        psubend();
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
                } // else: do not report the TOTAL cpus if cgroup-mode is on: we report only the stats of CPUs in
                  // current cgroup
            } else {
                // cpu_total = 0;
                count = sscanf(&line[3], /* cpuNNN USER*/
                    "%d %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", &cpuno, &user, &nice, &sys, &idle, &iowait,
                    &hardirq, &softirq, &steal, &guest, &guestnice);
                if (cpuno > max_cpuno)
                    max_cpuno = cpuno;
                if (cpuno >= MAX_LOGICAL_CPU)
                    continue;
                if (onlyCgroupAllowedCpus && !cgroup_is_allowed_cpu(cpuno))
                    continue;
                if (print) {
                    sprintf(label, "cpu%d", cpuno);
                    psub(label);
                    pdouble("user", DELTA_LOGICAL(user)); /* counter */
                    pdouble("nice", DELTA_LOGICAL(nice)); /* counter */
                    pdouble("sys", DELTA_LOGICAL(sys)); /* counter */
                    pdouble("idle", DELTA_LOGICAL(idle)); /* counter */
                    /*			pdouble("DEBUG IDLE idle: %lld %lld %lld\n", logical_cpu[cpuno].idle, idle,
                     * idle-logical_cpu[cpuno].idle); */
                    pdouble("iowait", DELTA_LOGICAL(iowait)); /* counter */
                    pdouble("hardirq", DELTA_LOGICAL(hardirq)); /* counter */
                    pdouble("softirq", DELTA_LOGICAL(softirq)); /* counter */
                    pdouble("steal", DELTA_LOGICAL(steal)); /* counter */
                    pdouble("guest", DELTA_LOGICAL(guest)); /* counter */
                    pdouble("guestnice", DELTA_LOGICAL(guestnice)); /* counter */
                    psubend();
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
            }
        }
        if (!strncmp(line, "ctxt", 4)) {
            value = 0;
            count = sscanf(&line[5], "%lld", &value); /* counter */
            if (count == 1) {
                if (print) {
                    psub("counters");
                    pdouble("ctxt", ((double)(value - old_ctxt) / elapsed));
                }
                old_ctxt = value;
            }
        }
        if (!strncmp(line, "btime", 5)) {
            value = 0;
            count = sscanf(&line[6], "%lld", &value); /* seconds since boot */
            if (print)
                plong("btime", value);
        }
        if (!strncmp(line, "processes", 9)) {
            value = 0;
            count = sscanf(&line[10], "%lld", &value); /* counter  actually forks */
            if (print)
                pdouble("processes_forks", ((double)(value - old_processes) / elapsed));
            old_processes = value;
        }
        if (!strncmp(line, "procs_running", 13)) {
            value = 0;
            count = sscanf(&line[14], "%lld", &value);
            if (print)
                plong("procs_running", value);
        }
        if (!strncmp(line, "procs_blocked", 13)) {
            value = 0;
            count = sscanf(&line[14], "%lld", &value);
            if (print) {
                plong("procs_blocked", value);
                psubend();
            }
        }
    }
    if (print)
        psectionend();
}

void NjmonCollectorApp::proc_diskstats(double elapsed, int print)
{
    struct diskinfo {
        long dk_major;
        long dk_minor;
        char dk_name[128];
        long long dk_reads;
        long long dk_rmerge;
        long long dk_rkb;
        long long dk_rmsec;
        long long dk_writes;
        long long dk_wmerge;
        long long dk_wkb;
        long long dk_wmsec;
        long long dk_inflight;
        long long dk_time;
        long long dk_backlog;
        long long dk_xfers;
        long long dk_bsize;
    };
    static struct diskinfo current;
    static struct diskinfo* previous;
    static FILE* fp = 0;
    char buf[1024];
    int dk_stats;

    /* popen variables */
    FILE* pop;
    static long disks = 0;
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
            disks = 0;
            if (fgets(tmpstr, 127, pop)) {
                for (;; disks++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 127, pop) == NULL)
                        break;
                    /*printf("DEBUG %ld disks - %s\n",disks,tmpstr);*/
                }
            }
            pclose(pop);
        } else
            disks = 0;
        /*printf("DEBUG %ld disks\n",disks); */
        previous = (diskinfo*)malloc(sizeof(struct diskinfo) * disks);

        pop = popen("lsblk --nodeps --output NAME,TYPE --raw 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            if (fgets(tmpstr, 70, pop)) {
                for (i = 0; i < disks; i++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 70, pop) == NULL)
                        break;
                    tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
                    len = strlen(tmpstr);
                    for (j = 0; j < len; j++)
                        if (tmpstr[j] == ' ')
                            tmpstr[j] = 0;
                    strcpy(previous[i].dk_name, tmpstr);
                    /*printf("DEBUG saved %ld %s disk name\n",i,previous[i].dk_name);*/
                }
            }
            pclose(pop);
        } else
            disks = 0;

        if ((fp = fopen("/proc/diskstats", "r")) == NULL) {
            LogError("failed to open - /proc/diskstats");
            return;
        }
    } else
        rewind(fp);

    if (print)
        psection("disks");
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        /*printf("DISKSTATS: \"%s\"", buf);*/
        /* zero the data ready for reading */
        bzero(&current, sizeof(struct diskinfo));
        dk_stats = sscanf(&buf[0], "%ld %ld %s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
            &current.dk_major, &current.dk_minor, &current.dk_name[0], &current.dk_reads, &current.dk_rmerge,
            &current.dk_rkb, &current.dk_rmsec, &current.dk_writes, &current.dk_wmerge, &current.dk_wkb,
            &current.dk_wmsec, &current.dk_inflight, &current.dk_time, &current.dk_backlog);

        if (dk_stats == 7) { /* shuffle the data around due to missing columns for partitions */

            current.dk_wkb = current.dk_rmsec;
            current.dk_writes = current.dk_rkb;
            current.dk_rkb = current.dk_rmerge;
            current.dk_rmsec = 0;
            current.dk_rmerge = 0;

        } else if (dk_stats != 14)
            fprintf(stderr, "disk sscanf wanted 14 but returned=%d line=%s\n", dk_stats, buf);

        current.dk_rkb /= 2; /* sectors = 512 bytes */
        current.dk_wkb /= 2;
        current.dk_xfers = current.dk_reads + current.dk_writes;
        if (current.dk_xfers == 0)
            current.dk_bsize = 0;
        else
            current.dk_bsize = ((current.dk_rkb + current.dk_wkb) / current.dk_xfers) * 1024;

        current.dk_time /= 10.0; /* in milli-seconds to make it upto 100%, 1000/100 = 10 */

        for (i = 0; i < disks; i++) {
            /*printf("DEBUG disks new %s old %s\n", current.dk_name,previous[i].dk_name);*/
            if (!strcmp(current.dk_name, previous[i].dk_name)) {

                /* loop**** disks are not real */
                if (strncmp(current.dk_name, "loop", 4) == 0)
                    filtered_out = 1;
                else
                    filtered_out = 0;

                if (print && !filtered_out) {
                    psub(current.dk_name);
                    /*
                                    printf("major",      current.dk_major);
                                    printf("minor",      current.dk_minor);
                    */
                    pdouble("reads", (current.dk_reads - previous[i].dk_reads) / elapsed);
                    /*printf("DEBUG  reads: %lld %lld %.2f,\n",    current.dk_reads, previous[i].dk_reads,  elapsed); */
                    pdouble("rmerge", (current.dk_rmerge - previous[i].dk_rmerge) / elapsed);
                    pdouble("rkb", (current.dk_rkb - previous[i].dk_rkb) / elapsed);
                    pdouble("rmsec", (current.dk_rmsec - previous[i].dk_rmsec) / elapsed);

                    pdouble("writes", (current.dk_writes - previous[i].dk_writes) / elapsed);
                    pdouble("wmerge", (current.dk_wmerge - previous[i].dk_wmerge) / elapsed);
                    pdouble("wkb", (current.dk_wkb - previous[i].dk_wkb) / elapsed);
                    pdouble("wmsec", (current.dk_wmsec - previous[i].dk_wmsec) / elapsed);

                    plong("inflight", current.dk_inflight);
                    pdouble("time", (current.dk_time - previous[i].dk_time) / elapsed);
                    pdouble("backlog", (current.dk_backlog - previous[i].dk_backlog) / elapsed);
                    pdouble("xfers", (current.dk_xfers - previous[i].dk_xfers) / elapsed);
                    plong("bsize", current.dk_bsize);
                    psubend();
                }
                memcpy(&previous[i], &current, sizeof(struct diskinfo));
                break; /* once found stop searching */
            }
        }
    }
    if (print)
        psectionend();
}

void NjmonCollectorApp::strip_spaces(char* s)
{
    char* p;
    int spaced = 1;

    p = s;
    for (p = s; *p != 0; p++) {
        if (*p == ':')
            *p = ' ';
        if (*p != ' ') {
            *s = *p;
            s++;
            spaced = 0;
        } else if (spaced) {
            /* do no thing as this is second space */
        } else {
            *s = *p;
            s++;
            spaced = 1;
        }
    }
    *s = 0;
}

void NjmonCollectorApp::proc_net_dev(double elapsed, int print)
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
    long long junk;

    static FILE* fp = 0;
    char buf[1024];
    static long interfaces;
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
            interfaces = 0;
            if (fgets(tmpstr, 1024, pop)) {
                for (;; interfaces++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 1024, pop) == NULL)
                        break;
                    /*printf("DEBUG %ld intergaces - %s\n",interfaces,tmpstr);*/
                }
            }
            pclose(pop);
        } else
            interfaces = 0;
        /*printf("DEBUG %ld intergaces\n",interfaces); */
        previous = (netinfo*)malloc(sizeof(struct netinfo) * interfaces);

        pop = popen("/sbin/ifconfig -s 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            if (fgets(tmpstr, 1024, pop)) {
                for (i = 0; i < interfaces; i++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 1024, pop) == NULL)
                        break;
                    tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
                    len = strlen(tmpstr);
                    for (j = 0; j < len; j++)
                        if (tmpstr[j] == ' ')
                            tmpstr[j] = 0;
                    strcpy(previous[i].if_name, tmpstr);
                    /*printf("DEBUG saved %ld %s interfaces name\n",i,previous[i].if_name);*/
                }
            }
            pclose(pop);
        } else
            interfaces = 0;

        if ((fp = fopen("/proc/net/dev", "r")) == NULL) {
            LogError("failed to open - /proc/net/dev");
            return;
        }
    } else
        rewind(fp);

    if (interfaces == 0)
        return; // this happens in e.g. Docker containers having no network

    if (fgets(buf, 1024, fp) == NULL)
        return; /* throw away the header line */
    if (fgets(buf, 1024, fp) == NULL)
        return; /* throw away the header line */

    if (print)
        psection("network_interfaces");
    while (fgets(buf, 1024, fp) != NULL) {
        strip_spaces(buf);
        bzero(&current, sizeof(struct netinfo));
        ret = sscanf(&buf[0], "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            (char*)current.if_name, &current.if_ibytes, &current.if_ipackets, &current.if_ierrs, &current.if_idrop,
            &current.if_ififo, &current.if_iframe, &junk, &junk, &current.if_obytes, &current.if_opackets,
            &current.if_oerrs, &current.if_odrop, &current.if_ofifo, &current.if_ocolls, &current.if_ocarrier);
        if (ret != 16) {
            fprintf(stderr, "net sscanf wanted 16 returned = %d line=%s\n", ret, (char*)buf);
        } else {
            for (i = 0; i < interfaces; i++) {
                /*printf("DEBUG: i=%ld current.if_name=%s, previous=%s interfaces=%ld\n",i,
                 * current.if_name,previous[i].if_name, interfaces);*/
                if (!strcmp(current.if_name, previous[i].if_name)) {
                    if (print) {
                        psub(current.if_name);
                        pdouble("ibytes", (current.if_ibytes - previous[i].if_ibytes) / elapsed);
                        pdouble("ipackets", (current.if_ipackets - previous[i].if_ipackets) / elapsed);
                        pdouble("ierrs", (current.if_ierrs - previous[i].if_ierrs) / elapsed);
                        pdouble("idrop", (current.if_idrop - previous[i].if_idrop) / elapsed);
                        pdouble("ififo", (current.if_ififo - previous[i].if_ififo) / elapsed);
                        pdouble("iframe", (current.if_iframe - previous[i].if_iframe) / elapsed);

                        pdouble("obytes", (current.if_obytes - previous[i].if_obytes) / elapsed);
                        pdouble("opackets", (current.if_opackets - previous[i].if_opackets) / elapsed);
                        pdouble("oerrs", (current.if_oerrs - previous[i].if_oerrs) / elapsed);
                        pdouble("odrop", (current.if_odrop - previous[i].if_odrop) / elapsed);
                        pdouble("ofifo", (current.if_ofifo - previous[i].if_ofifo) / elapsed);

                        pdouble("ocolls", (current.if_ocolls - previous[i].if_ocolls) / elapsed);
                        pdouble("ocarrier", (current.if_ocarrier - previous[i].if_ocarrier) / elapsed);
                        psubend();
                    }
                    memcpy(&previous[i], &current, sizeof(struct netinfo));
                    break; /* once found stop searching */
                }
            }
        }
    }
    if (print)
        psectionend();
}

void NjmonCollectorApp::etc_os_release()
{
    static FILE* fp = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/etc/os-release", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);

    psection("os_release");
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        if (buf[strlen(buf) - 1] == '"')
            buf[strlen(buf) - 1] = 0; /* remove double quote */

        if (!strncmp(buf, "NAME=", strlen("NAME="))) {
            pstring("name", &buf[strlen("NAME=") + 1]);
        }
        if (!strncmp(buf, "VERSION=", strlen("VERSION="))) {
            pstring("version", &buf[strlen("VERSION=") + 1]);
        }
        if (!strncmp(buf, "PRETTY_NAME=", strlen("PRETTY_NAME="))) {
            pstring("pretty_name", &buf[strlen("PRETTY_NAME=") + 1]);
        }
        if (!strncmp(buf, "VERSION_ID=", strlen("VERSION_ID="))) {
            pstring("version_id", &buf[strlen("VERSION_ID=") + 1]);
        }
    }
    psectionend();
}

void NjmonCollectorApp::proc_version()
{
    static FILE* fp = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/proc/version", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);
    if (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        for (size_t i = 0; i < strlen(buf); i++) {
            if (buf[i] == '"')
                buf[i] = '|';
        }
        psection("proc_version");
        pstring("version", buf);
        psectionend();
    }
}

void NjmonCollectorApp::lscpu()
{
    FILE* pop = 0;
    int data_col = 21;
    int len = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();
    if ((pop = popen("/usr/bin/lscpu", "r")) == NULL)
        return;

    buf[0] = 0;
    psection("lscpu");
    while (fgets(buf, 1024, pop) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        /*printf("DEBUG: lscpu line is |%s|\n",buf); */
        if (!strncmp("Architecture:", buf, strlen("Architecture:"))) {
            len = strlen(buf);
            for (data_col = 14; data_col < len; data_col++) {
                if (isalnum(buf[data_col]))
                    break;
            }
            pstring("architecture", &buf[data_col]);
        }
        if (!strncmp("Byte Order:", buf, strlen("Byte Order:"))) {
            pstring("byte_order", &buf[data_col]);
        }
        if (!strncmp("CPU(s):", buf, strlen("CPU(s):"))) {
            pstring("cpus", &buf[data_col]);
        }
        if (!strncmp("On-line CPU(s) list:", buf, strlen("On-line CPU(s) list:"))) {
            pstring("online_cpu_list", &buf[data_col]);
        }
        if (!strncmp("Off-line CPU(s) list:", buf, strlen("Off-line CPU(s) list:"))) {
            pstring("online_cpu_list", &buf[data_col]);
        }
        if (!strncmp("Model:", buf, strlen("Model:"))) {
            pstring("model", &buf[data_col]);
        }
        if (!strncmp("Model name:", buf, strlen("Model name:"))) {
            pstring("model_name", &buf[data_col]);
        }
        if (!strncmp("Thread(s) per core:", buf, strlen("Thread(s) per core:"))) {
            pstring("threads_per_core", &buf[data_col]);
        }
        if (!strncmp("Core(s) per socket:", buf, strlen("Core(s) per socket:"))) {
            pstring("cores_per_socket", &buf[data_col]);
        }
        if (!strncmp("Socket(s):", buf, strlen("Socket(s):"))) {
            pstring("sockets", &buf[data_col]);
        }
        if (!strncmp("NUMA node(s):", buf, strlen("NUMA node(s):"))) {
            pstring("numa_nodes", &buf[data_col]);
        }
        if (!strncmp("CPU MHz:", buf, strlen("CPU MHz:"))) {
            pstring("cpu_mhz", &buf[data_col]);
        }
        if (!strncmp("CPU max MHz:", buf, strlen("CPU max MHz:"))) {
            pstring("cpu_max_mhz", &buf[data_col]);
        }
        if (!strncmp("CPU min MHz:", buf, strlen("CPU min MHz:"))) {
            pstring("cpu_min_mhz", &buf[data_col]);
        }
        /* Intel only */
        if (!strncmp("BogoMIPS:", buf, strlen("BogoMIPS:"))) {
            pstring("bogomips", &buf[data_col]);
        }
        if (!strncmp("Vendor ID:", buf, strlen("Vendor ID:"))) {
            pstring("vendor_id", &buf[data_col]);
        }
        if (!strncmp("CPU family:", buf, strlen("CPU family:"))) {
            pstring("cpu_family", &buf[data_col]);
        }
        if (!strncmp("Stepping:", buf, strlen("Stepping:"))) {
            pstring("stepping", &buf[data_col]);
        }
    }
    psectionend();
    pclose(pop);
}

void NjmonCollectorApp::lshw()
{
    FILE* pop = 0;
    char buf[4096 + 1];

    DEBUGLOG_FUNCTION_START();

    if (!file_exists("/usr/bin/lshw"))
        return;

    // lshw supports JSON output natively so we just copy/paste its output
    // into our output file.
    // IMPORTANT: unfortunately when running from inside a container lshw will
    //            not be able to provide all the information it provides if launched
    //            on the baremetal...

    if ((pop = popen("/usr/bin/lshw -json", "r")) == NULL)
        return;

    buf[0] = 0;
    praw("    \"lshw\": ");
    while (fgets(buf, 4096, pop) != NULL) {
        buffer_check();
        praw("    "); // indentation
        praw(buf);
        buf[0] = 0;
    }
    pclose(pop);
}

void NjmonCollectorApp::proc_uptime()
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
            psection("proc_uptime");
            plong("total_seconds", value);
            days = value / 60 / 60 / 24;
            hours = (value - (days * 60 * 60 * 24)) / 60 / 60;
            plong("days", days);
            plong("hours", hours);
            psectionend();
        }
    }
}

void NjmonCollectorApp::proc_loadavg()
{
    static FILE* fp = 0;
    char buf[1024 + 1];
    int count;
    float load_avg_1min;
    float load_avg_5min;
    float load_avg_15min;

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/proc/loadavg", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);

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
            psection("proc_loadavg");
            pdouble("load_avg_1min", load_avg_1min);
            pdouble("load_avg_5min", load_avg_5min);
            pdouble("load_avg_15min", load_avg_15min);
            psectionend();
        }
    }
}

void NjmonCollectorApp::proc_filesystems()
{
    FILE* fp;
    struct mntent* fs;
    struct statfs vfs;

    DEBUGLOG_FUNCTION_START();
    if ((fp = setmntent("/etc/mtab", "r")) == NULL)
        LogError("setmntent(\"/etc/mtab\", \"r\") failed");

    psection("filesystems");
    while ((fs = getmntent(fp)) != NULL) {
        // NOTE: /dev/loop* filesystems are not real filesystems - e.g. on Ubuntu they are used for SNAPs
        if (fs->mnt_fsname[0] == '/' && strncmp(fs->mnt_fsname, "/dev/loop", 9) != 0) {
            if (statfs(fs->mnt_dir, &vfs) != 0) {
                LogError("%s: statfs failed: %d\n", fs->mnt_dir, errno);
            }
            /*printf("%s, mounted on %s:\n", fs->mnt_dir, fs->mnt_fsname); */

            psub(fs->mnt_fsname);
            pstring("fs_dir", fs->mnt_dir);
            pstring("fs_type", fs->mnt_type);
            pstring("fs_opts", fs->mnt_opts);

            plong("fs_freqs", fs->mnt_freq);
            plong("fs_passno", fs->mnt_passno);
            plong("fs_bsize", vfs.f_bsize);
            plong("fs_size_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024);
            plong("fs_free_mb", (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            plong("fs_used_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024 - (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            pdouble(
                "fs_full_percent", ((double)vfs.f_blocks - (double)vfs.f_bfree) / (double)vfs.f_blocks * (double)100.0);
            /*
             * pdouble("fs_full_percent", ((vfs.f_blocks * vfs.f_bsize) - (vfs.f_bfree * vfs.f_bsize) ) /
             *					(vfs.f_blocks * vfs.f_bsize) * 100.00);
             */
            plong("fs_avail", (vfs.f_bavail * vfs.f_bsize) / 1024 / 1024);
            plong("fs_files", vfs.f_files);
            plong("fs_files_free", vfs.f_ffree);
            plong("fs_namelength", vfs.f_namelen);
            psubend();
        }
    }
    psectionend();
    endmntent(fp);
}

long power_timebase = 0;
long power_nominal_mhz = 0;
int ispower = 0;

void NjmonCollectorApp::proc_cpuinfo()
{
    static FILE* fp = 0;
    char buf[1024 + 1];
    char string[1024 + 1];
    double value;
    int int_val;
    int processor;

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);

    psection("cpuinfo");
    processor = -1;
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        /* moronically cpuinfo file format has Tab characters !!! */

        if (!strncmp("processor", buf, strlen("processor"))) {
            // end previous section
            if (processor != -1)
                psubend();

            // start new section
            sscanf(&buf[12], "%d", &int_val);
            processor = int_val;
            sprintf(string, "proc%d", processor);
            psub(string);
            // processor++;
        }

        if (cgroup_is_allowed_cpu(processor)) {

            if (!strncmp("clock", buf, strlen("clock"))) { /* POWER ONLY */
                sscanf(&buf[9], "%lf", &value);
                pdouble("mhz_clock", value);
                power_nominal_mhz = value; /* save for sys_device_system_cpu() */
                ispower = 1;
            }
            if (!strncmp("vendor_id", buf, strlen("vendor_id"))) {
                pstring("vendor_id", &buf[12]);
            }
            if (!strncmp("cpu MHz", buf, strlen("cpu MHz"))) {
                sscanf(&buf[11], "%lf", &value);
                pdouble("cpu_mhz", value);
            }
            if (!strncmp("cache size", buf, strlen("cache size"))) {
                sscanf(&buf[13], "%lf", &value);
                pdouble("cache_size", value);
            }
            if (!strncmp("physical id", buf, strlen("physical id"))) {
                sscanf(&buf[14], "%d", &int_val);
                plong("physical_id", int_val);
            }
            if (!strncmp("siblings", buf, strlen("siblings"))) {
                sscanf(&buf[11], "%d", &int_val);
                plong("siblings", int_val);
            }
            if (!strncmp("core id", buf, strlen("core id"))) {
                sscanf(&buf[10], "%d", &int_val);
                plong("core_id", int_val);
            }
            if (!strncmp("cpu cores", buf, strlen("cpu cores"))) {
                sscanf(&buf[12], "%d", &int_val);
                plong("cpu_cores", int_val);
            }
            if (!strncmp("model name", buf, strlen("model name"))) {
                pstring("model_name", &buf[13]);
            }
            if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
                ispower = 1;
                break;
            }
        }
    }
    if (processor != -1)
        psubend();
    psectionend();
    if (ispower) {
        psection("cpuinfo_power");
        if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
            pstring("timebase", &buf[11]);
            power_timebase = atol(&buf[11]);
            plong("power_timebase", power_timebase);
        }
        while (fgets(buf, 1024, fp) != NULL) {
            buf[strlen(buf) - 1] = 0; /* remove newline */
            if (!strncmp("platform", buf, strlen("platform"))) { /* POWER only */
                pstring("platform", &buf[11]);
            }
            if (!strncmp("model", buf, strlen("model"))) {
                pstring("model", &buf[9]);
            }
            if (!strncmp("machine", buf, strlen("machine"))) {
                pstring("machine", &buf[11]);
            }
            if (!strncmp("firmware", buf, strlen("firmware"))) {
                pstring("firmware", &buf[11]);
            }
        }
        psectionend();
    }
}
