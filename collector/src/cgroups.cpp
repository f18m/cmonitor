/*
 * cgroups.cpp -- code for collecting CGROUP statistics
 * Developer: Francesco Montorsi.
 * (C) Copyright 2018 Francesco Montorsi

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
#include "logger.h"
#include "output_frontend.h"
#include "utils.h"
#include <assert.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

// ----------------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------------

#define MIN_ELAPSED_SECS (0.1)
#define PAGESIZE_BYTES (1024 * 4)
#define GIGABYTE (1000ul * 1000ul * 1000ul)
#define MEMORY_LIMIT_MAX_VALUE (1000 * 1000 * GIGABYTE)

typedef std::map<std::string /* controller type */, std::string /* path */> cgroup_paths_map_t;

// ----------------------------------------------------------------------------------
// C++ Helper functions
// ----------------------------------------------------------------------------------

std::string CGroupDetected2string(CGroupDetected k)
{
    switch (k) {
    case CG_NONE:
        return "none";
    case CG_VERSION1:
        return "v1";
    case CG_VERSION2:
        return "v2";
    default:
        return "";
    }
}

uint64_t compute_proc_score(const procsinfo_t* current_stats, const procsinfo_t* prev_stats, double elapsed_secs)
{
    static double ticks_per_sec = (double)sysconf(_SC_CLK_TCK); // clock ticks per second

    // take the total time this process/task/thread has been scheduled in both USER and KERNEL space:
    uint64_t cputime_clock_ticks = 0;
    if (current_stats->pi_utime >= prev_stats->pi_utime && // force newline
        current_stats->pi_stime >= prev_stats->pi_stime) {
        cputime_clock_ticks = // force newline
            (current_stats->pi_utime - prev_stats->pi_utime) + // userspace
            (current_stats->pi_stime - prev_stats->pi_stime); // kernelspace
    }

    // give a score which is linear in both CPU time and virtual memory size:
    // return (cputime_clock_ticks * ticks_per_sec) * current_stats->pi_vsize;

    // prefer a score based only on CPU time:
    // FIXME: it would be nice to have the "score policy" configurable and not hardcoded like that
    return cputime_clock_ticks * ticks_per_sec;
}

/* Lookup the right process state string */
const char* get_state(char n)
{
    static char duff[64];
    switch (n) {
    case 'R':
        return "Running";
    case 'S':
        return "Sleeping-interruptible";
    case 'D':
        return "Waiting-uninterruptible";
    case 'Z':
        return "Zombie";
    case 'T':
        return "Stopped";
    case 't':
        return "Tracing";
    case 'W':
        return "Paging-or-Waking";
    case 'X':
        return "Dead";
    case 'x':
        return "dead";
    case 'K':
        return "Wakekill";
    case 'P':
        return "Parked";
    default:
        snprintf(duff, 64, "State=%d(%c)", n, n);
        return duff;
    }
}

bool CMonitorCgroups::cgroup_proc_procsinfo(
    pid_t pid, bool include_threads, procsinfo_t* pout, OutputFields output_opts)
{
#define MAX_STAT_FILE_PREFIX_LEN 1000
#define MAX_PROC_FILENAME_LEN (MAX_STAT_FILE_PREFIX_LEN + 64)
#define MAX_PROC_CONTENT_LEN 4096

    FILE* fp = NULL;
    char stat_file_prefix[MAX_STAT_FILE_PREFIX_LEN] = { '\0' };
    char filename[MAX_PROC_FILENAME_LEN] = { '\0' };
    char buf[MAX_PROC_CONTENT_LEN] = { '\0' };

    memset(pout, 0, sizeof(procsinfo_t));

    /* the statistic directory for the process */
    snprintf(filename, MAX_PROC_FILENAME_LEN, "%s/proc/%d", m_proc_prefix.c_str(), pid);
    struct stat statbuf;
    if (stat(filename, &statbuf) != 0) {
        CMonitorLogger::instance()->LogErrorWithErrno("ERROR: failed to stat file %s", filename);
        return false;
    }

    // by looking at the owner of the directory we know which user is running it:
    pout->uid = statbuf.st_uid;
    struct passwd* pw = getpwuid(statbuf.st_uid);
    if (pw) {
        strncpy(pout->username, pw->pw_name, 63);
        pout->username[63] = 0;
    }

    /*
        ABOUT STATISTIC FILES CONSIDERED IN THIS FUNCTION:
        For multithreaded application it might be tricky to understand /proc file organization.
        Consider a single process with PID=TID=A having 2 secondary threads with TID=B and TID=C.
        The kernel "stat" files layout (but the same applies to all other files considered below
        like "statm", "status" and "io") will look like:
            /proc/A
                  +-- stat            contains statistics about the whole process having PID=TID=A
                  +-- task/A/stat     contains statistics about the main thread (PID=TID=A)
                  +-- task/B/stat     contains statistics about the secondary thread TID=B
                  +-- task/C/stat     contains statistics about the secondary thread TID=C
         So far so good, here comes the tricky part:
            /proc/B                   it exists even if B is just a secondary thread of PID=A
                  +
                  +-- stat            this is the tricky part... you would expect this to contain stats of TID=B
                  +                   but instead it contains statistics about the whole process (PID=TID=A)
                  +-- task/A/stat     contains statistics about the main thread (PID=TID=A)
                  +-- task/B/stat     contains statistics about the secondary thread TID=B
                  +-- task/C/stat     contains statistics about the secondary thread TID=C

         This means that:
         a) WHEN COLLECTING PER-THREAD STATISTICS:
         To make sure we always get statistics for the thread identified by PID=pid, regardless of the fact it's
         the main thread or a secondary one, we always look at /proc/<pid>/task/<pid>/<statistics-file>

         b) WHEN COLLECTING PER-PROCESS STATISTICS:
         To make sure we collect the stats for the whole process identified by PID=pid (and not just its main thread),
         we look at /proc/<pid>/<statistics-file>
    */
    if (include_threads)
        snprintf(stat_file_prefix, MAX_STAT_FILE_PREFIX_LEN, "%s/proc/%d/task/%d/", m_proc_prefix.c_str(), pid, pid);
    else
        snprintf(stat_file_prefix, MAX_STAT_FILE_PREFIX_LEN, "%s/proc/%d/", m_proc_prefix.c_str(), pid);

    { /* process the statistic file for the process/thread */
        snprintf(filename, MAX_PROC_FILENAME_LEN, "%s/stat", stat_file_prefix);
        if ((fp = fopen(filename, "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("ERROR: failed to open file %s", filename);
            return false;
        }

        size_t size = fread(buf, 1, MAX_PROC_CONTENT_LEN, fp);
        bool io_error = ferror(fp);
        bool reached_eof = feof(fp);
        fclose(fp); // regardless of what happened, always close the file
        if (size == 0 || size >= MAX_PROC_CONTENT_LEN || io_error) {
            CMonitorLogger::instance()->LogError(
                "ERROR: procsinfo read returned = %zu assuming process stopped pid=%d errno=%d\n", size, pid, errno);
            return false;
        }
        if (!reached_eof) {
            CMonitorLogger::instance()->LogError(
                "ERROR: procsinfo read returned = %zu for pid=%d but did not reach EOF\n", size, pid);
            return false;
        }

        // make sure the buffer is always NUL-terminated
        buf[size - 1] = '\0';

        // read columns (1) and (2):   "pid" and "comm"
        // see http://man7.org/linux/man-pages/man5/proc.5.html, search for /proc/[pid]/stat
        int ret = sscanf(buf, "%d (%s)", &pout->pi_pid, &pout->pi_comm[0]);
        if (ret != 2) {
            CMonitorLogger::instance()->LogError("procsinfo sscanf returned = %d line=%s\n", ret, buf);
            return false;
        }
        pout->pi_comm[strlen(pout->pi_comm) - 1] = 0;

        // never seen a case where inside /proc/<pid>/task/<pid>/stat you find mention of a pid != <pid>
        if (pout->pi_pid != pid) {
            CMonitorLogger::instance()->LogError(
                "ERROR: found pid=%d inside the filename=%s... unexpected mismatch\n", pout->pi_pid, filename);
            return false;
        }

        /* now look for ") " as dumb Infiniband driver includes "()" */
        size_t count = 0;
        for (count = 0; count < size; count++)
            if (buf[count] == ')' && buf[count + 1] == ' ')
                break;
        if (count >= size - 2) {
            CMonitorLogger::instance()->LogError("procsinfo failed to find end of command buf=%s\n", buf);
            return false;
        }
        count++; // skip ')'
        count++; // skip space after parentheses

        // see http://man7.org/linux/man-pages/man5/proc.5.html, search for /proc/[pid]/stat
        /* column 1 and 2 are handled above */
        long junk;
        ret = sscanf(&buf[count],
            "%c %d %d %d %d %d %lu %lu %lu %lu " /* from 3 to 13 */
            "%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu " /* from 14 to 23 */
            "%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu " /* from 24 to 33 */
            "%lu %lu %lu %lu %lu %d %d %lu %lu %llu", /* from 34 to 42 */
            &pout->pi_state, /*3 - these numbers are taken from "man proc" */
            &pout->pi_ppid, /*4*/
            &pout->pi_pgrp, /*5*/
            &pout->pi_session, /*6*/
            &pout->pi_tty_nr, /*7*/
            &pout->pi_tty_pgrp, /*8*/
            &pout->pi_flags, /*9*/
            &pout->pi_minflt, /*10*/
            &pout->pi_child_min_flt, /*11*/
            &pout->pi_majflt, /*12*/
            &pout->pi_child_maj_flt, /*13*/
            &pout->pi_utime, /*14*/ // CPU time spent in user space
                             &pout->pi_stime,
            /*15*/ // CPU time spent in kernel space
            &pout->pi_child_utime, /*16*/
            &pout->pi_child_stime, /*17*/
            &pout->pi_priority, /*18*/
            &pout->pi_nice, /*19*/
            &pout->pi_num_threads, /*20*/
            &junk, /*21*/
            &pout->pi_start_time, /*22*/
            &pout->pi_vsize, /*23*/
            &pout->pi_rss, /*24*/
            &pout->pi_rsslimit, /*25*/
            &pout->pi_start_code, /*26*/
            &pout->pi_end_code, /*27*/
            &pout->pi_start_stack, /*28*/
            &pout->pi_esp, /*29*/
            &pout->pi_eip, /*30*/
            &pout->pi_signal_pending, /*31*/
            &pout->pi_signal_blocked, /*32*/
            &pout->pi_signal_ignore, /*33*/
            &pout->pi_signal_catch, /*34*/
            &pout->pi_wchan, /*35*/
            &pout->pi_swap_pages, /*36*/
            &pout->pi_child_swap_pages, /*37*/
            &pout->pi_signal_exit, /*38*/
            &pout->pi_last_cpu, /*39*/
            &pout->pi_realtime_priority, /*40*/
            &pout->pi_sched_policy, /*41*/
            &pout->pi_delayacct_blkio_ticks /*42*/
        );
        if (ret != 40) {
            CMonitorLogger::instance()->LogError(
                "procsinfo sscanf wanted 40 returned = %d pid=%d line=%s\n", ret, pid, buf);
            return false;
        }
    }

    if (output_opts == PF_ALL) { /* process the statm file for the process/thread */

        snprintf(filename, MAX_PROC_FILENAME_LEN, "%s/statm", stat_file_prefix);
        if ((fp = fopen(filename, "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("failed to open file %s", filename);
            return false;
        }
        size_t size = fread(buf, 1, MAX_PROC_CONTENT_LEN - 1, fp);
        fclose(fp); /* close it even if the read failed, the file could have been removed
                    between open & read i.e. the device driver does not behave like a file */
        if (size == 0) {
            CMonitorLogger::instance()->LogError("failed to read file %s", filename);
            return false;
        }

        int ret = sscanf(&buf[0], "%lu %lu %lu %lu %lu %lu %lu", // force newline
            &pout->statm_size, &pout->statm_resident, &pout->statm_share, &pout->statm_trs, &pout->statm_lrs,
            &pout->statm_drs, &pout->statm_dt);
        if (ret != 7) {
            CMonitorLogger::instance()->LogError("sscanf wanted 7 returned = %d line=%s\n", ret, buf);
            return false;
        }
    }

    { /* process the status file for the process/thread */

        snprintf(filename, MAX_PROC_FILENAME_LEN, "%s/status", stat_file_prefix);
        if ((fp = fopen(filename, "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("failed to open file %s", filename);
            return false;
        }
        for (int i = 0;; i++) {
            if (fgets(buf, 1024, fp) == NULL) {
                break;
            }
            if (strncmp("Tgid:", buf, 5) == 0) {
                // this info is only available from the /status file apparently and not from /stat
                // and indicates whether this PID is the main thread (TGID==PID) or a secondary thread (TGID!=PID)
                sscanf(&buf[6], "%d", &pout->pi_tgid);
            }
        }
        fclose(fp);
    }

    { /* process the I/O file for the process/thread */
        pout->io_read_bytes = 0;
        pout->io_write_bytes = 0;
        snprintf(filename, MAX_PROC_FILENAME_LEN, "%s/io", stat_file_prefix);
        if ((fp = fopen(filename, "r")) != NULL) {
            for (int i = 0; i < 6; i++) {
                if (fgets(buf, 1024, fp) == NULL) {
                    break;
                }
                if (strncmp("rchar:", buf, 6) == 0)
                    sscanf(&buf[7], "%lld", &pout->io_rchar);
                if (strncmp("wchar:", buf, 6) == 0)
                    sscanf(&buf[7], "%lld", &pout->io_wchar);
                if (strncmp("read_bytes:", buf, 11) == 0)
                    sscanf(&buf[12], "%lld", &pout->io_read_bytes);
                if (strncmp("write_bytes:", buf, 12) == 0)
                    sscanf(&buf[13], "%lld", &pout->io_write_bytes);
            }
        }

        if (fp != NULL)
            fclose(fp);
    }
    return true;
}

/* static */
bool get_cgroup_paths_for_this_pid(cgroup_paths_map_t& cgroup_pathsOUT)
{
    /*
     *
     * ABOUT /proc/%d/cgroup:
     *   See http://man7.org/linux/man-pages/man7/cgroups.7.html, look for "/proc/[pid]/cgroup (since Linux 2.6.24)"
     *   Each line is composed by:
     *                     hierarchy-ID:controller-list:cgroup-path
     *   and the "controller-list"="name=systemd" seems to be always the indicative name of the whole cgroup.
     *   Example contents on a baremetal process:
     *
     *   	$ cat /proc/self/cgroup
                        12:pids:/user.slice/user-0.slice/session-5.scope
                        11:cpuset:/
                        10:perf_event:/
                        9:devices:/user.slice
                        8:memory:/user/root/0
                        7:hugetlb:/
                        6:blkio:/user.slice
                        5:cpu,cpuacct:/user.slice
                        4:freezer:/user/root/0
                        3:rdma:/
                        2:net_cls,net_prio:/
                        1:name=systemd:/user.slice/user-0.slice/session-5.scope
                        0::/user.slice/user-0.slice/session-5.scope
     */
    std::ifstream inputf("/proc/self/cgroup");
    if (!inputf.is_open())
        return false; // cannot read the cgroup information!

    std::string line;
    while (std::getline(inputf, line)) {
        std::vector<std::string> tuple = split_string_in_array(line, ':');
        if (tuple.size() != 3)
            return false; // invalid format

        std::string hierarchy_id = tuple[0];
        std::string controller_list = tuple[1];
        std::string path = tuple[2];

        cgroup_pathsOUT[controller_list] = path;
    }

    return !cgroup_pathsOUT.empty();
}

/* static */
bool are_cgroups_v2_enabled(std::string& cgroup_pathOUT)
{
    /*
     *
     * ABOUT /proc/%d/cgroup:
     *   See http://man7.org/linux/man-pages/man7/cgroups.7.html, look for "/proc/[pid]/cgroup (since Linux 2.6.24)"
     *   Each line is composed by:
     *                     hierarchy-ID:controller-list:cgroup-path
     *   The problem is that this file does not provide you the FULL cgroup path, which depends on where exactly that
     *   cgroup has been mounted.
     *
     * ABOUT /proc/%d/mounts:
     *   See http://man7.org/linux/man-pages/man5/fstab.5.html
     *   Each line is composed by:
     *                     fs_spec  fs_file  fs_vfstype  fs_mntops  fs_freq  fs_passno
     *   We are interested into the lines that provide the "cgroup" or "cgroup2" fs_spec
     *
     *   For cgroups v1:
     *     under LXC:
     *       cgroup /sys/fs/cgroup/cpuset/lxc/container1-main cgroup rw,nosuid,nodev,noexec,relatime,cpuset 0 0
     *     under Docker:
     *       cgroup /sys/fs/cgroup/cpuset cgroup ro,nosuid,nodev,noexec,relatime,cpuset 0 0
     *   the second string fs_file (/sys/fs/cgroup/cpuset/lxc/container1-main or /sys/fs/cgroup/cpuset) tells you where
     * to find all the current value of that cgroup; the fourth string fs_mntops contains the indication of the cgroup
     * type (e.g. cpuset)
     *
     *   For cgroups v2:
     *     under Docker:
     *      cgroup2 /sys/fs/cgroup cgroup2 rw,seclabel,nosuid,nodev,noexec,relatime,nsdelegate 0 0
     *
     */
    std::ifstream inputf("/proc/self/mounts");
    if (!inputf.is_open())
        return false; // cannot read the cgroup information!

    std::string line;
    while (std::getline(inputf, line)) {
        // cout << line << '\n';
        std::vector<std::string> tuple = split_string_in_array(line, ' ');
        if (tuple.size() != 6)
            return false; // invalid format

        std::string fs_spec = tuple[0];
        std::string fs_file = tuple[1];
        std::string fs_vfstype = tuple[2];
        // std::string fs_mntops = tuple[3];

        if (fs_spec == "cgroup2" && fs_vfstype == "cgroup2") {
            // found the "cgroup type" that belongs to cgroups v2... note that in this "if" branch the "cgroup_type"
            // is not used: cgroupsv2, also known as "unified cgroup hierarchy", will have a single path for the whole
            // cgroup, instead of having multiple ones for each different "cgroup_type"
            cgroup_pathOUT = fs_file;
            return true;
        }
    }

    return false; // cgroup name not found
}

/* static */
bool get_cgroup_v1_abs_path_prefix_for_this_pid(const std::string& cgroup_type, std::string& cgroup_pathOUT)
{
    /*
     *
     * ABOUT /proc/%d/cgroup:
     *   See http://man7.org/linux/man-pages/man7/cgroups.7.html, look for "/proc/[pid]/cgroup (since Linux 2.6.24)"
     *   Each line is composed by:
     *                     hierarchy-ID:controller-list:cgroup-path
     *   The problem is that this file does not provide you the FULL cgroup path, which depends on where exactly that
     *   cgroup has been mounted.
     *
     * ABOUT /proc/%d/mounts:
     *   See http://man7.org/linux/man-pages/man5/fstab.5.html
     *   Each line is composed by:
     *                     fs_spec  fs_file  fs_vfstype  fs_mntops  fs_freq  fs_passno
     *   We are interested into the lines that provide the "cgroup" or "cgroup2" fs_spec
     *
     *   For cgroups v1:
     *     under LXC:
     *       cgroup /sys/fs/cgroup/cpuset/lxc/container1-main cgroup rw,nosuid,nodev,noexec,relatime,cpuset 0 0
     *     under Docker:
     *       cgroup /sys/fs/cgroup/cpuset cgroup ro,nosuid,nodev,noexec,relatime,cpuset 0 0
     *   the second string fs_file (/sys/fs/cgroup/cpuset/lxc/container1-main or /sys/fs/cgroup/cpuset) tells you where
     * to find all the current value of that cgroup; the fourth string fs_mntops contains the indication of the cgroup
     * type (e.g. cpuset)
     *
     *   For cgroups v2:
     *     under Docker:
     *      cgroup2 /sys/fs/cgroup cgroup2 rw,seclabel,nosuid,nodev,noexec,relatime,nsdelegate 0 0
     *
     */
    std::ifstream inputf("/proc/self/mounts");
    if (!inputf.is_open())
        return false; // cannot read the cgroup information!

    std::string line;
    while (std::getline(inputf, line)) {
        // cout << line << '\n';
        std::vector<std::string> tuple = split_string_in_array(line, ' ');
        if (tuple.size() != 6)
            return false; // invalid format

        std::string fs_spec = tuple[0];
        std::string fs_file = tuple[1];
        std::string fs_vfstype = tuple[2];
        std::string fs_mntops = tuple[3];

        if (fs_spec == "cgroup" && fs_vfstype == "cgroup" && fs_mntops.find(cgroup_type) != std::string::npos) {
            // found the "cgroup type" that belongs to cgroups v1

            if (fs_file.empty() || fs_file == "/") {
                // !!this process is NOT running under any cgroup!!
                cgroup_pathOUT = "";
                return false;
            } else {
                cgroup_pathOUT = fs_file;
                return true;
            }
        }
    }

    return false; // cgroup name not found
}

bool read_from_system_cpu_for_current_cgroup(std::string kernelPath, std::set<uint64_t>& cpus)
{
    std::set<uint64_t> empty_set;
    return read_integers_with_range_validation(kernelPath + "/cpuset.cpus", 0, INT32_MAX, cpus);
}

bool CMonitorCgroups::read_cpuacct_line(const std::string& path, std::vector<uint64_t>& valuesINT /* OUT */)
{
    FILE* fp1 = 0;
    if ((fp1 = fopen(path.c_str(), "r")) == NULL)
        return false;

    if (fgets(m_buff, CGROUP_COLLECTOR_BUFF_SIZE, fp1) == NULL) {
        fclose(fp1);
        return false;
    }

    fclose(fp1);

    std::vector<std::string> values = split_string_in_array(m_buff, ' ');
    if (m_num_cpus_cpuacct_cgroup == 0) {
        // first time we read the CPU stats
        m_num_cpus_cpuacct_cgroup = values.size();
    } else {
        if (values.size() != m_num_cpus_cpuacct_cgroup) {
            // error: we read a different number of CPUs compared to previous read
            m_num_cpus_cpuacct_cgroup = 0;
            return false;
        }
    }

    valuesINT.resize(m_num_cpus_cpuacct_cgroup);
    for (unsigned int i = 0; i < m_num_cpus_cpuacct_cgroup; i++)
        if (!string2int(values[i].c_str(), valuesINT[i]))
            return false;

    return true;
}

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCgroups::cgroup_init( // force newline
    const std::string& cgroup_prefix_for_test, // force newline
    const std::string& proc_prefix_for_test)
{
    m_nCGroupsFound = CG_NONE;
    m_cgroup_systemd_name = "N/A";
    m_proc_prefix = proc_prefix_for_test;

    // ABSOLUTE PATH PREFIXES
    // Typical examples (cgroup v1)
    //    m_cgroup_memory_kernel_path   = /sys/fs/cgroup/memory/
    //    m_cgroup_cpuacct_kernel_path  = /sys/fs/cgroup/cpu,cpuacct/     or     /sys/fs/cgroup/cpuacct,cpu/
    //    m_cgroup_cpuset_kernel_path   = /sys/fs/cgroup/cpuset/
    // Typical examples (cgroup v2)
    //    m_cgroup_memory_kernel_path = m_cgroup_cpuacct_kernel_path = m_cgroup_cpuset_kernel_path =
    //    /sys/fs/cgroup/system.slice/

    std::string cgroupsv2_basepath;
    std::string cpuacct_controller_name = "cpu,cpuacct";
    if (!cgroup_prefix_for_test.empty()) {

        std::string dir = cgroup_prefix_for_test + "/sys/fs/cgroup/memory";
        if (file_or_dir_exists(dir.c_str())) {
            // assume we're unit testing cgroups v1
            m_cgroup_memory_kernel_path = cgroup_prefix_for_test + "/sys/fs/cgroup/memory"; // force newline
            m_cgroup_cpuacct_kernel_path = cgroup_prefix_for_test + "/sys/fs/cgroup/cpu,cpuacct"; // force newline
            m_cgroup_cpuset_kernel_path = cgroup_prefix_for_test + "/sys/fs/cgroup/cpuset"; // force newline
            m_nCGroupsFound = CG_VERSION1;
        } else {
            // assume we're unit testing cgroups v2
            m_cgroup_memory_kernel_path = cgroup_prefix_for_test;
            m_cgroup_cpuacct_kernel_path = cgroup_prefix_for_test;
            m_cgroup_cpuset_kernel_path = cgroup_prefix_for_test;
            m_nCGroupsFound = CG_VERSION2;
        }

    } else if (are_cgroups_v2_enabled(cgroupsv2_basepath)) {
        m_nCGroupsFound = CG_VERSION2;

        m_cgroup_memory_kernel_path = cgroupsv2_basepath;
        m_cgroup_cpuacct_kernel_path = cgroupsv2_basepath;
        m_cgroup_cpuset_kernel_path = cgroupsv2_basepath;
    } else {
        // try to detect cgroups v1
        m_nCGroupsFound = CG_VERSION1;

        if (!get_cgroup_v1_abs_path_prefix_for_this_pid("memory", m_cgroup_memory_kernel_path)) {
            CMonitorLogger::instance()->LogError(
                "Could not find the 'memory' cgroup path prefix. CGroup mode disabled.\n");
            m_nCGroupsFound = CG_NONE;
            return;
        }

        if (!get_cgroup_v1_abs_path_prefix_for_this_pid(cpuacct_controller_name, m_cgroup_cpuacct_kernel_path)) {

            // on some Linux distributions, the name of the cgroup has the "cpu" and "cpuacct" names inverted..
            // retry inverting the order:
            cpuacct_controller_name = "cpuacct,cpu";

            if (!get_cgroup_v1_abs_path_prefix_for_this_pid(cpuacct_controller_name, m_cgroup_cpuacct_kernel_path)) {
                CMonitorLogger::instance()->LogError(
                    "Could not find the 'cpuacct' cgroup path prefix. CGroup mode disabled.\n");
                m_nCGroupsFound = CG_NONE;
                return;
            }
        }

        if (!get_cgroup_v1_abs_path_prefix_for_this_pid("cpuset", m_cgroup_cpuset_kernel_path)) {
            CMonitorLogger::instance()->LogError(
                "Could not find the 'cpuset' cgroup path prefix. CGroup mode disabled.\n");
            m_nCGroupsFound = CG_NONE;
            return;
        }
    }

    // NOW DETECT ACTUAL CGROUP PATHS TO MONITOR

    if (m_pCfg->m_strCGroupName.empty() || m_pCfg->m_strCGroupName == "self") {

        // assume the user wants to monitor the same cgroup where cmonitor_collector is running:

        CMonitorLogger::instance()->LogDebug("No cgroup name provided. Trying to autodetect my own cgroup.");

        cgroup_paths_map_t cgroup_paths;
        if (!get_cgroup_paths_for_this_pid(cgroup_paths)) {
            CMonitorLogger::instance()->LogDebug("Could not get the cgroup paths. CGroup mode disabled.\n");
            m_nCGroupsFound = CG_NONE;
            return;
        }

        CMonitorLogger::instance()->LogDebug(
            "Found cpuset cgroup mounted at %s\n", m_cgroup_cpuset_kernel_path.c_str());
        CMonitorLogger::instance()->LogDebug(
            "Found cpuacct cgroup mounted at %s\n", m_cgroup_cpuacct_kernel_path.c_str());
        CMonitorLogger::instance()->LogDebug(
            "Found memory cgroup mounted at %s\n", m_cgroup_memory_kernel_path.c_str());

        // NOTE: in case we're inside Docker or LXC we should be able to find ourselves inside the
        //       paths composed only by the ABS PREFIXES
        if (cgroup_init_check_for_our_pid()) {
            m_cgroup_systemd_name = cgroup_paths["name=systemd"];
        } else {
            // try to adjust the full cgroup paths by adding the cgroup paths read from /proc/self/cgroup
            // to the absolute prefixes: this is typically necessary when running outside Docker/LXC but
            // just inside systemd:
            m_cgroup_memory_kernel_path += "/" + cgroup_paths["memory"];
            m_cgroup_cpuacct_kernel_path += "/" + cgroup_paths[cpuacct_controller_name];
            m_cgroup_cpuset_kernel_path += "/" + cgroup_paths["cpuset"];
            CMonitorLogger::instance()->LogDebug(
                "Adjusting cpuset cgroup path to %s\n", m_cgroup_cpuset_kernel_path.c_str());
            CMonitorLogger::instance()->LogDebug(
                "Adjusting cpuacct cgroup path to %s\n", m_cgroup_cpuacct_kernel_path.c_str());
            CMonitorLogger::instance()->LogDebug(
                "Adjusting memory cgroup path to %s\n", m_cgroup_memory_kernel_path.c_str());
            if (cgroup_init_check_for_our_pid())
                m_cgroup_systemd_name = cgroup_paths["name=systemd"];
        }
    } else {
        CMonitorLogger::instance()->LogDebug(
            "Cgroup name [%s] provided. Trying to detect the paths for the actual cgroups to monitor.",
            m_pCfg->m_strCGroupName.c_str());

        // assume that the provided cgroup is using the same absolute CGROUP prefix of this process...
        // this should be pretty safe since in practice it means assuming that there is a single kernel folder like
        // /sys/fs/cgroup/...
        m_cgroup_memory_kernel_path += "/" + m_pCfg->m_strCGroupName;
        m_cgroup_cpuacct_kernel_path += "/" + m_pCfg->m_strCGroupName;
        m_cgroup_cpuset_kernel_path += "/" + m_pCfg->m_strCGroupName;

        // verify the provided cgroup name is actually existing on disk:
        if (!file_or_dir_exists(m_cgroup_memory_kernel_path.c_str())) {
            CMonitorLogger::instance()->LogError(
                "Cannot find the cgroup directory corresponding to the provided cgroup name: directory "
                "[%s] does not exist. CGroup mode disabled.",
                m_cgroup_memory_kernel_path.c_str());
            m_nCGroupsFound = CG_NONE;
            return;
        }

        m_cgroup_systemd_name = m_pCfg->m_strCGroupName;

        CMonitorLogger::instance()->LogDebug("Set cpuset cgroup path to %s\n", m_cgroup_cpuset_kernel_path.c_str());
        CMonitorLogger::instance()->LogDebug("Set cpuacct cgroup path to %s\n", m_cgroup_cpuacct_kernel_path.c_str());
        CMonitorLogger::instance()->LogDebug("Set memory cgroup path to %s\n", m_cgroup_memory_kernel_path.c_str());
    }

    switch (m_nCGroupsFound) {
    case CG_NONE:
        break;
    case CG_VERSION1:
        cgroup_v1_read_limits();
        break;
    case CG_VERSION2:
        cgroup_v2_read_limits();
        break;
    }
}

void CMonitorCgroups::cgroup_v1_read_limits()
{
    // READ LIMITS IMPOSED BY CGROUPS

    if (!read_integer(m_cgroup_memory_kernel_path + "/memory.limit_in_bytes", m_cgroup_memory_limit_bytes)) {
        CMonitorLogger::instance()->LogError(
            "Could not read the memory limit from 'memory' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }
    // IMPORTANT: m_cgroup_memory_limit_bytes might assume some crazy high value like 9*10^6 GB
    //            that means "no limit"
    if (m_cgroup_memory_limit_bytes > MEMORY_LIMIT_MAX_VALUE)
        m_cgroup_memory_limit_bytes = UINT64_MAX;
    if (m_cgroup_memory_limit_bytes == 0) {
        CMonitorLogger::instance()->LogError(
            "Could not read the memory limit from 'memory' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }

    if (!read_from_system_cpu_for_current_cgroup(m_cgroup_cpuset_kernel_path, m_cgroup_cpus)) {
        CMonitorLogger::instance()->LogError("Could not read the CPUs from 'cpuset' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }

    if (!read_integer(m_cgroup_cpuacct_kernel_path + "/cpu.cfs_period_us", m_cgroup_cpuacct_period_us)) {
        CMonitorLogger::instance()->LogError(
            "Could not read the CPU period from 'cpuacct' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }

    // IMPORTANT: m_cgroup_cpuacct_quota_us might assume the special value UINT64_MAX when "-1" is reported by the
    // cgroup controller... it just means "no limit"
    if (!read_integer(m_cgroup_cpuacct_kernel_path + "/cpu.cfs_quota_us", m_cgroup_cpuacct_quota_us)) {
        CMonitorLogger::instance()->LogError(
            "Could not read the CPU quota from 'cpuacct' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }

    // cpuset and memory cgroups found:
    CMonitorLogger::instance()->LogDebug(
        "CGroup monitoring successfully enabled. CGroup name is %s\n", m_cgroup_systemd_name.c_str());
    CMonitorLogger::instance()->LogDebug("Found cpuset cgroup limiting to CPUs %s, mounted at %s\n",
        stl_container2string(m_cgroup_cpus, ",").c_str(), m_cgroup_cpuset_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Found cpuacct cgroup limiting at %lu/%lu usecs mounted at %s\n",
        m_cgroup_cpuacct_quota_us, m_cgroup_cpuacct_period_us, m_cgroup_cpuacct_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Found memory cgroup limiting to %luB, mounted at %s\n",
        m_cgroup_memory_limit_bytes, m_cgroup_memory_kernel_path.c_str());
}

void CMonitorCgroups::cgroup_v2_read_limits()
{
    // READ LIMITS IMPOSED BY CGROUPS

    // FIXME: m_cgroup_memory_limit_bytes might assume the special value "max" reported by the
    // cgroup controller... it just means "no limit"... we need to handle that using UINT64_MAX
    if (!read_integer(m_cgroup_memory_kernel_path + "/memory.max", m_cgroup_memory_limit_bytes)) {
        CMonitorLogger::instance()->LogError(
            "Could not read the memory limit from 'memory' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }

    if (!read_from_system_cpu_for_current_cgroup(m_cgroup_cpuset_kernel_path, m_cgroup_cpus)) {
        CMonitorLogger::instance()->LogError("Could not read the CPUs from 'cpuset' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }

    // FIXME: m_cgroup_cpuacct_quota_us might assume the special value "max" reported by the
    // cgroup controller... it just means "no limit"... we need to handle that using UINT64_MAX
    if (!read_two_integers(
            m_cgroup_cpuacct_kernel_path + "/cpu.max", m_cgroup_cpuacct_quota_us, m_cgroup_cpuacct_period_us)) {
        CMonitorLogger::instance()->LogError(
            "Could not read the CPU period from 'cpuacct' cgroup. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return;
    }

    // cpuset and memory cgroups found:
    CMonitorLogger::instance()->LogDebug(
        "CGroup monitoring successfully enabled. CGroup name is %s\n", m_cgroup_systemd_name.c_str());
    CMonitorLogger::instance()->LogDebug("Found cpuset cgroup limiting to CPUs %s, mounted at %s\n",
        stl_container2string(m_cgroup_cpus, ",").c_str(), m_cgroup_cpuset_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Found cpuacct cgroup limiting at %lu/%lu usecs mounted at %s\n",
        m_cgroup_cpuacct_quota_us, m_cgroup_cpuacct_period_us, m_cgroup_cpuacct_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Found memory cgroup limiting to %luB, mounted at %s\n",
        m_cgroup_memory_limit_bytes, m_cgroup_memory_kernel_path.c_str());
}

bool CMonitorCgroups::cgroup_init_check_for_our_pid()
{
    // CGROUP CHECKS
    // now if we got the right paths, we should be able to find our pid in all these cgroups
    // NOTE: depending on the container technology (Docker, LXC or LXD) or on the absence of containers
    //       but presence of cgroups (like those of e.g. systemd) we may have a masquerated PID
    //       (e.g. PID '8' inside a Docker container while the real PID is 2348 on baremetal)

    pid_t ourPid = getpid();
    bool found = true;

    if (search_integer(m_cgroup_memory_kernel_path + "/tasks", uint64_t(ourPid)))
        CMonitorLogger::instance()->LogDebug("Successfully found our PID %d in the 'memory' cgroup.\n", ourPid);
    else {
        CMonitorLogger::instance()->LogDebug("Could not find our PID %d in the 'memory' cgroup.\n", ourPid);
        found = false;
    }

    if (search_integer(m_cgroup_cpuacct_kernel_path + "/tasks", uint64_t(ourPid)))
        CMonitorLogger::instance()->LogDebug("Successfully found our PID %d in the 'cpuacct' cgroup.\n", ourPid);
    else {
        CMonitorLogger::instance()->LogDebug("Could not find our PID %d in the 'cpuacct' cgroup.\n", ourPid);
        found = false;
    }

    if (search_integer(m_cgroup_cpuset_kernel_path + "/tasks", uint64_t(ourPid)))
        CMonitorLogger::instance()->LogDebug("Successfully found our PID %d in the 'cpuset' cgroup.\n", ourPid);
    else {
        CMonitorLogger::instance()->LogDebug("Could not find our PID %d in the 'cpuset' cgroup.\n", ourPid);
        found = false;
    }

    return found;
}

void CMonitorCgroups::output_config()
{
    if (m_nCGroupsFound == CG_NONE)
        return;

    m_pOutput->psection_start("cgroup_config");

    // the cgroup name & version
    m_pOutput->pstring("name", m_cgroup_systemd_name.c_str());
    m_pOutput->pstring("version", CGroupDetected2string(m_nCGroupsFound).c_str());

    // the cgroup paths
    m_pOutput->pstring("memory_path", &m_cgroup_memory_kernel_path[0]);
    m_pOutput->pstring("cpuacct_path", &m_cgroup_cpuacct_kernel_path[0]);
    m_pOutput->pstring("cpuset_path", &m_cgroup_cpuset_kernel_path[0]);

    // actual cgroup limits
    std::string tmp = stl_container2string(m_cgroup_cpus, ",");
    m_pOutput->pstring("cpus", &tmp[0]);
    if (m_cgroup_cpuacct_quota_us == UINT64_MAX)
        m_pOutput->pdouble("cpu_quota_perc", -1.0f);
    else if (m_cgroup_cpuacct_period_us)
        m_pOutput->pdouble("cpu_quota_perc", (double)m_cgroup_cpuacct_quota_us / (double)m_cgroup_cpuacct_period_us);
    else
        m_pOutput->pdouble("cpu_quota_perc", 0.0);

    if (m_cgroup_memory_limit_bytes == UINT64_MAX)
        m_pOutput->pdouble("memory_limit_bytes", -1.0f);
    else
        m_pOutput->plong("memory_limit_bytes", m_cgroup_memory_limit_bytes);

    m_pOutput->psection_end();
}

bool CMonitorCgroups::cgroup_still_exists()
{
    return file_or_dir_exists(m_cgroup_memory_kernel_path.c_str()) && // force newline
        file_or_dir_exists(m_cgroup_cpuacct_kernel_path.c_str()) && // force newline
        file_or_dir_exists(m_cgroup_cpuset_kernel_path.c_str());
}

bool CMonitorCgroups::cgroup_is_allowed_cpu(int cpu)
{
    if (m_nCGroupsFound == CG_NONE)
        return true; // allowed
    return m_cgroup_cpus.find(cpu) != m_cgroup_cpus.end();
}

size_t CMonitorCgroups::cgroup_proc_memory_dump_flat_keyed(
    const std::string& path, const std::set<std::string>& allowedStatsNames, const std::string& label_prefix)
{
    size_t nread = 0;
    FILE* fp_memory_stats;
    if ((fp_memory_stats = fopen(path.c_str(), "r")) == NULL)
        return nread;

    std::string label, value_str;
    while (fgets(m_buff, 1000, fp_memory_stats) != NULL) {

        if (m_nCGroupsFound == CG_VERSION1)
            if (strncmp(m_buff, "total_", 6) != 0)
                continue; // skip NON-totals: collect only cgroup-total values

        size_t len = strlen(m_buff);
        if (m_buff[len - 1] == '\n')
            m_buff[len - 1] = 0;
#if 0 // in both cgroups v1 and cgroups v2 the "memory" controller will never generate 'special' characters
        for (size_t i = 0; i < len; i++) {
            if (m_buff[i] == '(')
                m_buff[i] = '_';
            if (m_buff[i] == ')')
                m_buff[i] = ' ';
            if (m_buff[i] == ':')
                m_buff[i] = ' ';
        }
#endif

        uint64_t value = 0;
        if (split_string_on_first_separator(m_buff, ' ', label, value_str) && string2int(value_str.c_str(), value)) {
            if (allowedStatsNames.empty() /* all stats must be put in output */
                || allowedStatsNames.find(label) != allowedStatsNames.end()) {
                m_pOutput->plong((label_prefix + label).c_str(), value);
                nread++;
            }
        }
    }

    fclose(fp_memory_stats);

    return nread;
}

void CMonitorCgroups::cgroup_proc_memory(
    const std::set<std::string>& allowedStatsNames_v1, const std::set<std::string>& allowedStatsNames_v2)
{
    uint64_t value;

    if (m_nCGroupsFound == CG_NONE)
        return;

    // See
    //   https://lwn.net/Articles/529927/
    //   https://www.kernel.org/doc/Documentation/cgroup-v1/memory.txt
    //   https://www.kernel.org/doc/Documentation/cgroup-v2.txt

    m_pOutput->psection_start("cgroup_memory_stats");

    if (m_nCGroupsFound == CG_VERSION2)
        // list as first value the main "current" KPI
        if (read_integer(m_cgroup_memory_kernel_path + "/memory.current", value))
            m_pOutput->plong("stat.current", value);

    // dump main memory statistics file
    const std::set<std::string>& allowedStatsNames
        = (m_nCGroupsFound == CG_VERSION1) ? allowedStatsNames_v1 : allowedStatsNames_v2;
    cgroup_proc_memory_dump_flat_keyed(m_cgroup_memory_kernel_path + "/memory.stat", allowedStatsNames, "stat.");

    switch (m_nCGroupsFound) {
    case CG_VERSION1:
        if (read_integer(m_cgroup_memory_kernel_path + "/memory.failcnt", value))
            m_pOutput->plong("failcnt", value);
        break;

    case CG_VERSION2:
        cgroup_proc_memory_dump_flat_keyed(
            m_cgroup_memory_kernel_path + "/memory.events", allowedStatsNames, "events.");
        break;

    case CG_NONE:
        break;
    }

    m_pOutput->psection_end();
}

void CMonitorCgroups::cgroup_proc_cpuacct(double elapsed_sec)
{
    if (m_nCGroupsFound == CG_NONE)
        return;

    bool print = (m_num_cpuacct_samples_collected > 0);
    m_num_cpuacct_samples_collected++;

    /* NOTE: newer distros have stats like
     *     /sys/fs/cgroup/cpu,cpuacct/cpuacct.usage_percpu_sys
     *     /sys/fs/cgroup/cpu,cpuacct/cpuacct.usage_percpu_user
     * but older ones (e.g. Centos7) have only:
     *     /sys/fs/cgroup/cpu,cpuacct/cpuacct.usage_percpu
     * Here we try to handle both cases.
     *
     * See:
     *  https://www.kernel.org/doc/Documentation/cgroup-v1/cpuacct.txt
     *  https://www.kernel.org/doc/Documentation/cgroup-v2.txt
     *  https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/6/html/resource_management_guide/sec-cpuacct
     */

    // non-static data:
    char label[512];

    if (print)
        m_pOutput->psection_start("cgroup_cpuacct_stats");

    std::string cgroup_stat_file = m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_sys";
    cpuacct_utilisation_t total_cpu_usage = { 0 };
    bool bValidData = true;
    if (file_or_dir_exists(cgroup_stat_file.c_str())) {

        // this system supports per-cpu system/user stats:

        std::vector<uint64_t> counter_nsec_sys_mode;
        if (!read_cpuacct_line(cgroup_stat_file, counter_nsec_sys_mode))
            bValidData = false;

        std::vector<uint64_t> counter_nsec_user_mode;
        if (!read_cpuacct_line(m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_user", counter_nsec_user_mode))
            bValidData = false;

        if (counter_nsec_sys_mode.size() != counter_nsec_user_mode.size())
            bValidData = false;
        if (counter_nsec_sys_mode.empty())
            bValidData = false;

        if (bValidData) {
            CMonitorLogger::instance()->LogDebug("Found cpuacct.usage_percpu_sys/user cgroups; computing CPU usage "
                                                 "for %.2fsec delta time and %zu CPUs "
                                                 "(print=%d)\n",
                elapsed_sec, counter_nsec_user_mode.size(), print);

            for (size_t i = 0; i < counter_nsec_user_mode.size(); i++) {

                /*
                 * We know how much time has elapsed; we thus divide the delta
                 * of the incremental counter of ns spent in user mode by the elapsed
                 * to understand how much time (for this CPU) was spent in user mode.
                 *
                 * HOW TO TEST THIS CODE:
                 * run
                 *     make ; src/cmonitor_collector -C -c100 -s1 >test.json
                 *     taskset --cpu-list 3 stress --cpu 1   # launch a "stress" process with CPU-affinity on cpu #3
                 * then just verify that
                 *     watch -n1 'grep cpu3 -A6 -B1 test.json | tail -20'
                 * produces cpu3 at 100%
                 */
                CMonitorLogger::instance()->LogDebug(
                    "CPU %zu, current user=%lu, current sys=%lu, prev user=%lu, prev sys=%lu", // force newline
                    i, counter_nsec_user_mode[i], counter_nsec_sys_mode[i],
                    m_cpuacct_prev_values[i].counter_nsec_user_mode, m_cpuacct_prev_values[i].counter_nsec_sys_mode);
                if (cgroup_is_allowed_cpu(i) && print && elapsed_sec > MIN_ELAPSED_SECS) {
                    double cpuUserPercent = // force newline
                        100 * ((double)(counter_nsec_user_mode[i] - m_cpuacct_prev_values[i].counter_nsec_user_mode))
                        / (elapsed_sec * 1E9);
                    double cpuSysPercent = // force newline
                        100 * ((double)(counter_nsec_sys_mode[i] - m_cpuacct_prev_values[i].counter_nsec_sys_mode))
                        / (elapsed_sec * 1E9);

                    // output JSON counter
                    sprintf(label, "cpu%zu", i);
                    m_pOutput->psubsection_start(label);
                    m_pOutput->pdouble("user", cpuUserPercent);
                    m_pOutput->pdouble("sys", cpuSysPercent);
                    m_pOutput->psubsection_end();
                }

                // maintain the total cpu usage counter
                total_cpu_usage.counter_nsec_user_mode += counter_nsec_user_mode[i];
                total_cpu_usage.counter_nsec_sys_mode += counter_nsec_sys_mode[i];

                // save for next cycle
                m_cpuacct_prev_values[i].counter_nsec_user_mode = counter_nsec_user_mode[i];
                m_cpuacct_prev_values[i].counter_nsec_sys_mode = counter_nsec_sys_mode[i];
            }
        }

    } else {
        // just get the per-cpu total:

        // update "cgroup_stat_file" which might be used later for error logging
        cgroup_stat_file = m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu";

        std::vector<uint64_t> counter_nsec_user_mode;
        if (!read_cpuacct_line(cgroup_stat_file, counter_nsec_user_mode))
            bValidData = false;
        if (counter_nsec_user_mode.empty())
            bValidData = false;

        if (bValidData) {
            CMonitorLogger::instance()->LogDebug("Found data from cgroup cpuacct.usage_percpu");

            for (size_t i = 0; i < counter_nsec_user_mode.size(); i++) {

                /*
                 * Same comments for USER/SYS computations done above apply here!
                 */
                if (cgroup_is_allowed_cpu(i) && print && elapsed_sec > MIN_ELAPSED_SECS) {
                    double cpuUserPercent = // force newline
                        100 * ((double)(counter_nsec_user_mode[i] - m_cpuacct_prev_values[i].counter_nsec_user_mode))
                        / (elapsed_sec * 1E9);

                    // output JSON counter
                    sprintf(label, "cpu%zu", i);
                    m_pOutput->psubsection_start(label);
                    m_pOutput->pdouble("user", cpuUserPercent);
                    m_pOutput->psubsection_end();
                }

                // maintain the total cpu usage counter
                total_cpu_usage.counter_nsec_user_mode += counter_nsec_user_mode[i];

                // save for next cycle
                m_cpuacct_prev_values[i].counter_nsec_user_mode = counter_nsec_user_mode[i];
            }
        }
    }

    // emit aggregated counter across all cpus
    if (bValidData) {
        if (print && elapsed_sec > MIN_ELAPSED_SECS) {
            double cpuUserPercent = // force newline
                100
                * ((double)(total_cpu_usage.counter_nsec_user_mode
                    - m_cpuacct_prev_values_for_total_cpu.counter_nsec_user_mode))
                / (elapsed_sec * 1E9);
            double cpuSysPercent = // force newline
                100
                * ((double)(total_cpu_usage.counter_nsec_sys_mode
                    - m_cpuacct_prev_values_for_total_cpu.counter_nsec_sys_mode))
                / (elapsed_sec * 1E9);

            // output JSON counter
            m_pOutput->psubsection_start("cpu_tot");
            m_pOutput->pdouble("user", cpuUserPercent);
            m_pOutput->pdouble("sys", cpuSysPercent);
            m_pOutput->psubsection_end();
        }

        // save for next cycle
        m_cpuacct_prev_values_for_total_cpu = total_cpu_usage;
    } else {
        CMonitorLogger::instance()->LogError("failed to open %s", cgroup_stat_file.c_str());
    }

    // See
    //   https://www.kernel.org/doc/Documentation/cgroup-v2.txt
    //   https://medium.com/indeed-engineering/unthrottled-fixing-cpu-limits-in-the-cloud-a0995ede8e89

    cgroup_stat_file = m_cgroup_cpuacct_kernel_path + "/cpu.stat";
    if (file_or_dir_exists(cgroup_stat_file.c_str())) {
        if (m_fp_cpuacct_stats == 0) {
            if ((m_fp_cpuacct_stats = fopen(cgroup_stat_file.c_str(), "r")) == NULL) {
                m_fp_cpuacct_stats = 0;
            }
        } else {
            rewind(m_fp_cpuacct_stats);
        }

        if (m_fp_cpuacct_stats) {
            if (print)
                m_pOutput->psubsection_start("throttling");

            while (fgets(m_buff, 1000, m_fp_cpuacct_stats) != NULL) {
                uint64_t value;
                char label[512];
                sscanf(m_buff, "%s %lu", label, &value);
                if (print)
                    m_pOutput->plong(label, value);
            }

            if (print)
                m_pOutput->psubsection_end();
        }
    } else {
        CMonitorLogger::instance()->LogError("failed to open %s", cgroup_stat_file.c_str());
    }

    if (print)
        m_pOutput->psection_end();
}

bool CMonitorCgroups::cgroup_collect_pids(std::vector<pid_t>& pids)
{
    std::string path = m_cgroup_cpuacct_kernel_path + "/tasks";
    CMonitorLogger::instance()->LogDebug("Trying to read tasks inside the monitored cgroup from %s.\n", path.c_str());
    if (!file_or_dir_exists(path.c_str()))
        return false;

    std::ifstream inputf(path);
    if (!inputf.is_open())
        return false; // cannot read the cgroup information!

    DEBUGLOG_FUNCTION_START();

    std::string line;
    while (std::getline(inputf, line)) {
        uint64_t pid;
        // this PID is actually a TID (thread ID) most of the time... because in the kernel process/thread
        // distinction is much less strong than userspace: they're all tasks
        if (string2int(line.c_str(), pid))
            pids.push_back((pid_t)pid);
    }

    CMonitorLogger::instance()->LogDebug(
        "Found %zu PIDs/TIDs to monitor: %s.\n", pids.size(), stl_container2string(pids, ",").c_str());

    return true;
}

void CMonitorCgroups::cgroup_proc_tasks(double elapsed_sec, OutputFields output_opts, bool include_threads)
{
    char str[256];

    if (m_nCGroupsFound == CG_NONE)
        return;

    if (m_num_tasks_samples_collected == 0)
        output_opts = PF_NONE; // the first sample is used as bootstrap: we cannot generate any meaningful delta and
                               // thus any meaningful output
    m_num_tasks_samples_collected++;

    // swap databases
    m_pid_database_current_index = !m_pid_database_current_index;
    std::map<pid_t, procsinfo_t>& currDB = m_pid_databases[m_pid_database_current_index];
    std::map<pid_t, procsinfo_t>& prevDB = m_pid_databases[!m_pid_database_current_index];

    // collect all PIDs for current cgroup
    std::vector<pid_t> all_pids;
    if (!cgroup_collect_pids(all_pids))
        return;

    // get new fresh processes data and update current database:
    currDB.clear();
    for (size_t i = 0; i < all_pids.size(); i++) {

        // acquire all possible informations on this PID (or TID)
        procsinfo_t procData;
        cgroup_proc_procsinfo(all_pids[i], include_threads, &procData, output_opts);

        if (include_threads) {
            // CMonitorLogger::instance()->LogDebug("Found thread %d %d", procData.pi_pid, procData.pi_tgid);
            currDB.insert(std::make_pair(all_pids[i], procData));
        } else {
            // only the main thread has its PID == TGID...
            if (procData.pi_pid == procData.pi_tgid)
                // this is the main thread of current PID... insert it into the database
                currDB.insert(std::make_pair(all_pids[i], procData));
        }
    }

    if (output_opts == PF_NONE) {
        CMonitorLogger::instance()->LogDebug(
            "Initialized process DB with %lu entries on this first sample. Not generating any output.\n",
            currDB.size());
        return;
    }

    // Sort the processes by their "score" by inserting them into an ordered map
    assert(m_topper_procs.empty());
    CMonitorLogger::instance()->LogDebug(
        "The current process DB has %lu entries, the DB storing previous statuses has %lu entries.\n", currDB.size(),
        prevDB.size());

    for (const auto& current_entry : currDB) {
        pid_t current_pid = current_entry.first;
        const procsinfo_t* pcurrent_status = &current_entry.second;

        // find the previous stats for this PID:
        auto itPrevStatus = prevDB.find(current_pid);
        if (itPrevStatus != prevDB.end()) {
            const procsinfo_t* pprev_status = &itPrevStatus->second;

            // compute the score
            uint64_t score = compute_proc_score(pcurrent_status, pprev_status, elapsed_sec);
            proc_topper_t newEntry = { .current = pcurrent_status, .prev = pprev_status };
            m_topper_procs.insert(std::make_pair(score, newEntry));

            // of the 40 fields of procsinfo_t we're mostly interested in user and system time:
            CMonitorLogger::instance()->LogDebug(
                "pid=%d: %s: utime=%lu, stime=%lu, prev_utime=%lu, prev_stime=%lu, score=%lu", // force newline
                pcurrent_status->pi_pid, pcurrent_status->pi_comm, // force newline
                pcurrent_status->pi_utime, pcurrent_status->pi_stime, // force newline
                pprev_status->pi_utime, pprev_status->pi_stime, score);
            // CMonitorLogger::instance()->LogDebug("PID=%lu -> score=%lu", current_entry.first, score);
        }
    }

    if (m_topper_procs.empty())
        return;

    CMonitorLogger::instance()->LogDebug(
        "Tracking %zu/%zu processes/threads (include_threads=%d); min/max score found: %lu/%lu", // force
                                                                                                 // newline
        currDB.size(), all_pids.size(), include_threads, m_topper_procs.begin()->first, m_topper_procs.rbegin()->first);

    // Now output all data for each process, starting from the minimal score PROCESS_SCORE_IGNORE_THRESHOLD
    static double ticks = (double)sysconf(_SC_CLK_TCK); // clock ticks per second
    size_t nProcsOverThreshold = 0;
    m_pOutput->psection_start("cgroup_tasks");
    for (auto entry = m_topper_procs.lower_bound(m_pCfg->m_nProcessScoreThreshold); entry != m_topper_procs.end();
         entry++) {
        uint64_t score = (*entry).first;

        // note that the m_topper_procs map contains pointers to "currDB" and "prevDB"
        const procsinfo_t* p = (*entry).second.current;
        const procsinfo_t* q = (*entry).second.prev;

#define CURRENT(member) (p->member)
#define PREVIOUS(member) (q->member)
#define TIMEDELTA(member) (CURRENT(member) - PREVIOUS(member))
#define COUNTDELTA(member) ((PREVIOUS(member) > CURRENT(member)) ? 0 : (CURRENT(member) - PREVIOUS(member)))

        sprintf(str, "pid_%ld", (long)CURRENT(pi_pid));
        m_pOutput->psubsection_start(str);
        m_pOutput->plong("cmon_score", score);

        /*
         * Process fields
         */
        m_pOutput->pstring(
            "cmd", CURRENT(pi_comm)); // Full command line can be found /proc/PID/cmdline with zeros in it!
        m_pOutput->plong("pid", CURRENT(pi_pid));
        m_pOutput->plong("ppid", CURRENT(pi_ppid));
        m_pOutput->plong("pgrp", CURRENT(pi_pgrp));
        m_pOutput->plong("priority", CURRENT(pi_priority));
        m_pOutput->plong("nice", CURRENT(pi_nice));
        m_pOutput->plong("session", CURRENT(pi_session));
        m_pOutput->plong("tty_nr", CURRENT(pi_tty_nr));
        // m_pOutput->phex("flags", CURRENT(pi_flags));
        m_pOutput->pstring("state", get_state(CURRENT(pi_state)));
        m_pOutput->plong("threads", CURRENT(pi_num_threads));
        m_pOutput->pdouble("start_time_secs", (double)(CURRENT(pi_start_time)) / ticks);
        m_pOutput->plong("uid", CURRENT(uid));
        if (strlen(CURRENT(username)) > 0)
            m_pOutput->pstring("username", CURRENT(username));

        /*
         * CPU fields
         * NOTE: all CPU fields specify amount of time, measured in units of USER_HZ
                 (1/100ths of a second on most architectures); this means that if the
                 _delta_ CPU value reported is 60 in USR/SYSTEM mode, then that mode took 60% of the CPU!
                 IOW there is no need to do any math to produce a percentage, just taking
                 the delta of the absolute, monotonic-increasing value and divide by the elapsed time
        */
        m_pOutput->pdouble("cpu_tot", (double)(TIMEDELTA(pi_utime) + TIMEDELTA(pi_stime)) / elapsed_sec);
        m_pOutput->pdouble("cpu_usr", (double)TIMEDELTA(pi_utime) / elapsed_sec);
        m_pOutput->pdouble("cpu_sys", (double)TIMEDELTA(pi_stime) / elapsed_sec);

        // provide also the total, monotonically-increasing CPU time:
        // this is used by chart script to produce the "top of the topper" chart
        m_pOutput->pdouble("cpu_usr_total_secs", (double)CURRENT(pi_utime) / ticks);
        m_pOutput->pdouble("cpu_sys_total_secs", (double)CURRENT(pi_stime) / ticks);
        m_pOutput->plong("cpu_last", CURRENT(pi_last_cpu));

        /*
         * Memory fields
         */
        if (output_opts == PF_ALL) {
            m_pOutput->plong("mem_size_kb", CURRENT(statm_size) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_resident_kb", CURRENT(statm_resident) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_restext_kb", CURRENT(statm_trs) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_resdata_kb", CURRENT(statm_drs) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_share_kb", CURRENT(statm_share) * PAGESIZE_BYTES / 1024);
        }
        m_pOutput->pdouble("mem_minor_fault", COUNTDELTA(pi_minflt) / elapsed_sec);
        m_pOutput->pdouble("mem_major_fault", COUNTDELTA(pi_majflt) / elapsed_sec);
        m_pOutput->plong("mem_virtual_bytes", CURRENT(pi_vsize));
        m_pOutput->plong("mem_rss_bytes", CURRENT(pi_rss) * PAGESIZE_BYTES);
        m_pOutput->plong("mem_rss_limit", CURRENT(pi_rsslimit));

        /*
         * Signal fields
         */
#if PROCESS_DEBUGGING_ADDRESSES_SIGNALS
        /* NOT INCLUDED AS THEY ARE FOR DEBUGGING AND NOT PERFORMANCE TUNING */
        m_pOutput->phex("start_code", CURRENT(pi_start_code));
        m_pOutput->phex("end_code", CURRENT(pi_end_code));
        m_pOutput->phex("start_stack", CURRENT(pi_start_stack));
        m_pOutput->phex("esp_stack_pointer", CURRENT(pi_esp));
        m_pOutput->phex("eip_instruction_pointer", CURRENT(pi_eip));
        m_pOutput->phex("signal_pending", CURRENT(pi_signal_pending));
        m_pOutput->phex("signal_blocked", CURRENT(pi_signal_blocked));
        m_pOutput->phex("signal_ignore", CURRENT(pi_signal_ignore));
        m_pOutput->phex("signal_catch", CURRENT(pi_signal_catch));
        m_pOutput->phex("signal_exit", CURRENT(pi_signal_exit));
        m_pOutput->phex("wchan", CURRENT(pi_wchan));
        /* NOT INCLUDED AS THEY ARE FOR DEBUGGING AND NOT PERFORMANCE TUNING */
#endif
        if (output_opts == PF_ALL) {
            m_pOutput->plong("swap_pages", CURRENT(pi_swap_pages));
            m_pOutput->plong("child_swap_pages", CURRENT(pi_child_swap_pages));
            m_pOutput->plong("realtime_priority", CURRENT(pi_realtime_priority));
            m_pOutput->plong("sched_policy", CURRENT(pi_sched_policy));
        }

        /*
         * I/O fields
         */
        m_pOutput->pdouble("io_delayacct_blkio_secs", (double)CURRENT(pi_delayacct_blkio_ticks) / ticks);
        m_pOutput->plong("io_rchar", TIMEDELTA(io_rchar) / elapsed_sec);
        m_pOutput->plong("io_wchar", TIMEDELTA(io_wchar) / elapsed_sec);
        m_pOutput->plong("io_read_bytes", TIMEDELTA(io_read_bytes) / elapsed_sec);
        m_pOutput->plong("io_write_bytes", TIMEDELTA(io_write_bytes) / elapsed_sec);

        // provide also the total, monotonically-increasing I/O time:
        // this is used by chart script to produce the "top of the topper" chart
        m_pOutput->plong("io_total_read", CURRENT(io_rchar));
        m_pOutput->plong("io_total_write", CURRENT(io_wchar));

        m_pOutput->psubsection_end();
        nProcsOverThreshold++;
    }
    m_pOutput->psection_end();

    CMonitorLogger::instance()->LogDebug("%zu processes found over score threshold", nProcsOverThreshold);
    m_topper_procs.clear();
}
