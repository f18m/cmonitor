/*
 * cgroups.cpp -- interacts with Linux control groups to allow
 *                njmon to monitor only the CPU/memory/disk resources
 *                that the current cgroup allows to use.
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

#include "cmonitor.h"
#include "output_frontend.h"
#include <assert.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

// ----------------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------------

#define MAX_LOGICAL_CPU (256)
#define MIN_ELAPSED_SECS (0.1)
#define PAGESIZE_BYTES (1024 * 4)

/* Put a threshold on the CPU percentage basically:
 */
//#define PROCESS_SCORE_IGNORE_THRESHOLD (1000)
#define PROCESS_SCORE_IGNORE_THRESHOLD (1)

typedef std::map<std::string /* controller type */, std::string /* path */> cgroup_paths_map_t;

// ----------------------------------------------------------------------------------
// C++ Helper functions
// ----------------------------------------------------------------------------------

uint64_t compute_proc_score(const procsinfo_t* current_stats, const procsinfo_t* prev_stats, double elapsed_secs)
{
    static double ticks_per_sec = (double)sysconf(_SC_CLK_TCK); // clock ticks per second

    uint64_t cputime_clock_ticks = // force newline
        (current_stats->pi_utime - prev_stats->pi_utime) + // force newline
        (current_stats->pi_stime - prev_stats->pi_stime);

    // give a score which is linear in both CPU time and virtual memory size:
    // return (cputime_clock_ticks * ticks_per_sec) * current_stats->pi_vsize;

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

bool cgroup_proc_procsinfo(pid_t pid, procsinfo_t* p, OutputFields output_opts)
{
    FILE* fp;
    char filename[64];
    char buf[1024 * 4];
    int size = 0;
    int ret = 0;
    int count = 0;
    int i;
    struct stat statbuf;
    struct passwd* pw;

    /* the statistic directory for the process */
    snprintf(filename, 64, "/proc/%d", pid);
    if (stat(filename, &statbuf) != 0) {
        g_logger.LogError("ERROR: failed to stat file %s", filename);
        return false;
    }

    // by looking at the owner of the directory we know which user is running it:
    p->uid = statbuf.st_uid;
    pw = getpwuid(statbuf.st_uid);
    if (pw) {
        strncpy(p->username, pw->pw_name, 63);
        p->username[63] = 0;
    }

    { /* the statistic file for the process */
        snprintf(filename, 64, "/proc/%d/stat", pid);
        if ((fp = fopen(filename, "r")) == NULL) {
            g_logger.LogError("ERROR: failed to open file %s", filename);
            return false;
        }

        size = fread(buf, 1, 1024 - 1, fp);
        fclose(fp);
        if (size == -1) {
            g_logger.LogError(
                "ERROR: procsinfo read returned = %d assuming process stopped pid=%d errno=%d\n", ret, pid, errno);
            return false;
        }
        ret = sscanf(buf, "%d (%s)", &p->pi_pid, &p->pi_comm[0]);
        if (ret != 2) {
            g_logger.LogError("procsinfo sscanf returned = %d line=%s\n", ret, buf);
            return false;
        }
        p->pi_comm[strlen(p->pi_comm) - 1] = 0;

        /* now look for ") " as dumb Infiniband driver includes "()" */
        for (count = 0; count < size; count++)
            if (buf[count] == ')' && buf[count + 1] == ' ')
                break;
        if (count == size) {
            g_logger.LogError("procsinfo failed to find end of command buf=%s\n", buf);
            return false;
        }
        count++;
        count++;

        // see http://man7.org/linux/man-pages/man5/proc.5.html, search for /proc/[pid]/stat
        /* column 1 and 2 are handled above */
        long junk;
        ret = sscanf(&buf[count],
            "%c %d %d %d %d %d %lu %lu %lu %lu " /* from 3 to 13 */
            "%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu " /* from 14 to 23 */
            "%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu " /* from 24 to 33 */
            "%lu %lu %lu %lu %lu %d %d %lu %lu %llu", /* from 34 to 42 */
            &p->pi_state, /*3 - these numbers are taken from "man proc" */
            &p->pi_ppid, /*4*/
            &p->pi_pgrp, /*5*/
            &p->pi_session, /*6*/
            &p->pi_tty_nr, /*7*/
            &p->pi_tty_pgrp, /*8*/
            &p->pi_flags, /*9*/
            &p->pi_minflt, /*10*/
            &p->pi_child_min_flt, /*11*/
            &p->pi_majflt, /*12*/
            &p->pi_child_maj_flt, /*13*/
            &p->pi_utime, /*14*/
            &p->pi_stime, /*15*/
            &p->pi_child_utime, /*16*/
            &p->pi_child_stime, /*17*/
            &p->pi_priority, /*18*/
            &p->pi_nice, /*19*/
            &p->pi_num_threads, /*20*/
            &junk, /*21*/
            &p->pi_start_time, /*22*/
            &p->pi_vsize, /*23*/
            &p->pi_rss, /*24*/
            &p->pi_rsslimit, /*25*/
            &p->pi_start_code, /*26*/
            &p->pi_end_code, /*27*/
            &p->pi_start_stack, /*28*/
            &p->pi_esp, /*29*/
            &p->pi_eip, /*30*/
            &p->pi_signal_pending, /*31*/
            &p->pi_signal_blocked, /*32*/
            &p->pi_signal_ignore, /*33*/
            &p->pi_signal_catch, /*34*/
            &p->pi_wchan, /*35*/
            &p->pi_swap_pages, /*36*/
            &p->pi_child_swap_pages, /*37*/
            &p->pi_signal_exit, /*38*/
            &p->pi_last_cpu, /*39*/
            &p->pi_realtime_priority, /*40*/
            &p->pi_sched_policy, /*41*/
            &p->pi_delayacct_blkio_ticks /*42*/
        );
        if (ret != 40) {
            g_logger.LogError("procsinfo2 sscanf wanted 40 returned = %d pid=%d line=%s\n", ret, pid, buf);
            return false;
        }
    }

    if (output_opts == PF_ALL) { /* the statm file for the process */

        snprintf(filename, 64, "/proc/%d/statm", pid);
        if ((fp = fopen(filename, "r")) == NULL) {
            g_logger.LogError("failed to open file %s", filename);
            return false;
        }
        size = fread(buf, 1, 1024 * 4 - 1, fp);
        fclose(fp); /* close it even if the read failed, the file could have been removed
                    between open & read i.e. the device driver does not behave like a file */
        if (size == -1) {
            g_logger.LogError("failed to read file %s", filename);
            return false;
        }

        ret = sscanf(&buf[0], "%lu %lu %lu %lu %lu %lu %lu", // force newline
            &p->statm_size, &p->statm_resident, &p->statm_share, &p->statm_trs, &p->statm_lrs, &p->statm_drs,
            &p->statm_dt);
        if (ret != 7) {
            g_logger.LogError("sscanf wanted 7 returned = %d line=%s\n", ret, buf);
            return false;
        }
    }

    { /* the status file for the process */

        snprintf(filename, 64, "/proc/%d/status", pid);
        if ((fp = fopen(filename, "r")) == NULL) {
            g_logger.LogError("failed to open file %s", filename);
            return false;
        }
        for (i = 0;; i++) {
            if (fgets(buf, 1024, fp) == NULL) {
                break;
            }
            if (strncmp("Tgid:", buf, 5) == 0) {
                // this info is only available from the /status file apparently and not from /stat
                sscanf(&buf[6], "%d", &p->pi_tgid);
            }
        }
        fclose(fp);
    }

    /*if (uid == (uid_t)0)*/
    { /* the io file for the process */
        p->io_read_bytes = 0;
        p->io_write_bytes = 0;
        sprintf(filename, "/proc/%d/io", pid);
        if ((fp = fopen(filename, "r")) != NULL) {
            for (i = 0; i < 6; i++) {
                if (fgets(buf, 1024, fp) == NULL) {
                    break;
                }
                if (strncmp("rchar:", buf, 6) == 0)
                    sscanf(&buf[7], "%lld", &p->io_rchar);
                if (strncmp("wchar:", buf, 6) == 0)
                    sscanf(&buf[7], "%lld", &p->io_wchar);
                if (strncmp("read_bytes:", buf, 11) == 0)
                    sscanf(&buf[12], "%lld", &p->io_read_bytes);
                if (strncmp("write_bytes:", buf, 12) == 0)
                    sscanf(&buf[13], "%lld", &p->io_write_bytes);
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
bool get_cgroup_abs_path_prefix_for_this_pid(const std::string& cgroup_type, std::string& cgroup_pathOUT)
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
     *   We are interested into the lines that indicate a specific CGROUP TYPE mountpoint like:
     *   under LXC:
     *    cgroup /sys/fs/cgroup/cpuset/lxc/container1-main cgroup rw,nosuid,nodev,noexec,relatime,cpuset 0 0
     *   under Docker:
     *    cgroup /sys/fs/cgroup/cpuset cgroup ro,nosuid,nodev,noexec,relatime,cpuset 0 0
     *
     * the second string fs_file (/sys/fs/cgroup/cpuset/lxc/container1-main or /sys/fs/cgroup/cpuset) tells you where to
     * find all the current value of that cgroup; the fourth string fs_mntops contains the indication of the cgroup type
     * (e.g. cpuset)
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
        std::string fs_mntops = tuple[3];

        if (fs_spec == "cgroup" && fs_mntops.find(cgroup_type) != std::string::npos) {
            // found the right "cgroup type"

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

bool read_from_system_cpu_for_current_cgroup(std::string kernelPath, std::set<int>& cpus)
{
    std::set<int> empty_set;
    return read_integers_with_range_validation(kernelPath + "/cpuset.cpus", 0, INT32_MAX, cpus);
}

bool read_cpuacct_line(const std::string& path, std::vector<uint64_t>& valuesINT /* OUT */)
{
    static unsigned int num_cpus = 0;
    char line[8192];

    FILE* fp1 = 0;
    if ((fp1 = fopen(path.c_str(), "r")) == NULL)
        return false;

    if (fgets(line, 1000, fp1) == NULL) {
        fclose(fp1);
        return false;
    }

    fclose(fp1);

    std::vector<std::string> values = split_string_in_array(line, ' ');
    if (num_cpus == 0) {
        // first time we read the CPU stats
        num_cpus = values.size();
    } else {
        if (values.size() != num_cpus) {
            // error: we read a different number of CPUs compared to previous read
            num_cpus = 0;
            return false;
        }
    }

    valuesINT.resize(num_cpus);
    for (unsigned int i = 0; i < num_cpus; i++)
        string2int(values[i], valuesINT[i]);

    return true;
}

// ----------------------------------------------------------------------------------
// CMonitorCollectorApp - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCollectorApp::cgroup_init()
{
    m_bCGroupsFound = false;
    m_cgroup_systemd_name = "N/A";

    // ABSOLUTE PATH PREFIXES

    if (!get_cgroup_abs_path_prefix_for_this_pid("memory", m_cgroup_memory_kernel_path)) {
        g_logger.LogDebug("Could not find the 'memory' cgroup path prefix. CGroup mode disabled.\n");
        return;
    }

    std::string cpuacct_controller_name = "cpu,cpuacct";
    if (!get_cgroup_abs_path_prefix_for_this_pid(cpuacct_controller_name, m_cgroup_cpuacct_kernel_path)) {

        // on some Linux distributions, the name of the cgroup has the "cpu" and "cpuacct" names inverted..
        // retry inverting the order:
        cpuacct_controller_name = "cpuacct,cpu";

        if (!get_cgroup_abs_path_prefix_for_this_pid(cpuacct_controller_name, m_cgroup_cpuacct_kernel_path)) {
            g_logger.LogDebug("Could not find the 'cpuacct' cgroup path prefix. CGroup mode disabled.\n");
            return;
        }
    }
    if (!get_cgroup_abs_path_prefix_for_this_pid("cpuset", m_cgroup_cpuset_kernel_path)) {
        g_logger.LogDebug("Could not find the 'cpuset' cgroup path prefix. CGroup mode disabled.\n");
        return;
    }

    // ACTUAL CGROUP PATHS

    if (g_cfg.m_strCGroupName.empty() || g_cfg.m_strCGroupName == "self") {

        // assume the user wants to monitor the same cgroup where cmonitor_collector is running:

        g_logger.LogDebug("No cgroup name provided. Trying to autodetect my own cgroup.");

        cgroup_paths_map_t cgroup_paths;
        if (!get_cgroup_paths_for_this_pid(cgroup_paths)) {
            g_logger.LogDebug("Could not get the cgroup paths. CGroup mode disabled.\n");
            return;
        }

        g_logger.LogDebug("Found cpuset cgroup mounted at %s\n", m_cgroup_cpuset_kernel_path.c_str());
        g_logger.LogDebug("Found cpuacct cgroup mounted at %s\n", m_cgroup_cpuacct_kernel_path.c_str());
        g_logger.LogDebug("Found memory cgroup mounted at %s\n", m_cgroup_memory_kernel_path.c_str());

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
            g_logger.LogDebug("Adjusting cpuset cgroup path to %s\n", m_cgroup_cpuset_kernel_path.c_str());
            g_logger.LogDebug("Adjusting cpuacct cgroup path to %s\n", m_cgroup_cpuacct_kernel_path.c_str());
            g_logger.LogDebug("Adjusting memory cgroup path to %s\n", m_cgroup_memory_kernel_path.c_str());
            if (cgroup_init_check_for_our_pid())
                m_cgroup_systemd_name = cgroup_paths["name=systemd"];
        }
    } else {
        // verify the provided cgroup name is actually existing on disk:
        g_logger.LogDebug("Cgroup name [%s] provided. Trying to detect the paths for the actual cgroups to monitor.",
            m_cgroup_memory_kernel_path.c_str());
        m_cgroup_memory_kernel_path += "/" + g_cfg.m_strCGroupName;
        m_cgroup_cpuacct_kernel_path += "/" + g_cfg.m_strCGroupName;
        m_cgroup_cpuset_kernel_path += "/" + g_cfg.m_strCGroupName;
        if (!file_or_dir_exists(m_cgroup_memory_kernel_path.c_str())) {
            g_logger.LogError("Cannot find the cgroup directory corresponding to the provided cgroup name: directory "
                              "[%s] does not exist. CGroup mode disabled.\n",
                m_cgroup_memory_kernel_path.c_str());
            return;
        }

        m_cgroup_systemd_name = g_cfg.m_strCGroupName;
    }

    // READ LIMITS IMPOSED BY CGROUPS

    if (!read_integer(m_cgroup_memory_kernel_path + "/memory.limit_in_bytes", m_cgroup_memory_limit_bytes)) {
        g_logger.LogDebug("Could not read the memory limit from 'memory' cgroup. CGroup mode disabled.\n");
        return;
    }
    if (!read_from_system_cpu_for_current_cgroup(m_cgroup_cpuset_kernel_path, m_cgroup_cpus)) {
        g_logger.LogDebug("Could not read the CPUs from 'cpuset' cgroup. CGroup mode disabled.\n");
        return;
    }
    if (m_cgroup_memory_limit_bytes == 0) {
        g_logger.LogDebug("Could not read the memory limit from 'memory' cgroup. CGroup mode disabled.\n");
        return;
    }

    // cpuset and memory cgroups found:
    m_bCGroupsFound = true;
    g_logger.LogDebug("CGroup monitoring successfully enabled. CGroup name is %s\n", m_cgroup_systemd_name.c_str());
    g_logger.LogDebug("Found cpuset cgroup limiting to CPUs: %s, mounted at %s\n",
        stl_container2string(m_cgroup_cpus, ",").c_str(), m_cgroup_cpuset_kernel_path.c_str());
    g_logger.LogDebug("Found cpuacct cgroup mounted at %s\n", m_cgroup_cpuacct_kernel_path.c_str());
    g_logger.LogDebug("Found memory cgroup limiting to Bytes: %lu, mounted at %s\n", m_cgroup_memory_limit_bytes,
        m_cgroup_memory_kernel_path.c_str());
}

bool CMonitorCollectorApp::cgroup_init_check_for_our_pid()
{
    // CGROUP CHECKS
    // now if we got the right paths, we should be able to find our pid in all these cgroups
    // NOTE: depending on the container technology (Docker, LXC or LXD) or on the absence of containers
    //       but presence of cgroups (like those of e.g. systemd) we may have a masquerated PID
    //       (e.g. PID '8' inside a Docker container while the real PID is 2348 on baremetal)

    pid_t ourPid = getpid();
    bool found = true;

    if (search_integer(m_cgroup_memory_kernel_path + "/tasks", uint64_t(ourPid)))
        g_logger.LogDebug("Successfully found our PID %d in the 'memory' cgroup.\n", ourPid);
    else {
        g_logger.LogDebug("Could not find our PID %d in the 'memory' cgroup.\n", ourPid);
        found = false;
    }

    if (search_integer(m_cgroup_cpuacct_kernel_path + "/tasks", uint64_t(ourPid)))
        g_logger.LogDebug("Successfully found our PID %d in the 'cpuacct' cgroup.\n", ourPid);
    else {
        g_logger.LogDebug("Could not find our PID %d in the 'cpuacct' cgroup.\n", ourPid);
        found = false;
    }

    if (search_integer(m_cgroup_cpuset_kernel_path + "/tasks", uint64_t(ourPid)))
        g_logger.LogDebug("Successfully found our PID %d in the 'cpuset' cgroup.\n", ourPid);
    else {
        g_logger.LogDebug("Could not find our PID %d in the 'cpuset' cgroup.\n", ourPid);
        found = false;
    }

    return found;
}

void CMonitorCollectorApp::cgroup_config()
{
    if (!m_bCGroupsFound)
        return;

    g_output.psection_start("cgroup_config");
    g_output.pstring("name", m_cgroup_systemd_name.c_str());
    g_output.pstring("memory_path", &m_cgroup_memory_kernel_path[0]);
    g_output.pstring("cpuacct_path", &m_cgroup_cpuacct_kernel_path[0]);
    g_output.pstring("cpuset_path", &m_cgroup_cpuset_kernel_path[0]);

    std::string tmp = stl_container2string(m_cgroup_cpus, ",");
    g_output.pstring("cpus", &tmp[0]);
    g_output.plong("memory_limit_bytes", m_cgroup_memory_limit_bytes);
    g_output.psection_end();
}

bool CMonitorCollectorApp::cgroup_is_allowed_cpu(int cpu)
{
    if (!m_bCGroupsFound)
        return true; // allowed
    return m_cgroup_cpus.find(cpu) != m_cgroup_cpus.end();
}

void CMonitorCollectorApp::cgroup_proc_memory(const std::set<std::string>& allowedStatsNames)
{
    if (!m_bCGroupsFound)
        return;

    // See
    //   https://lwn.net/Articles/529927/
    //   https://www.kernel.org/doc/Documentation/cgroup-v1/memory.txt
    //   https://www.kernel.org/doc/Documentation/cgroup-v2.txt
    int i, len;
    uint64_t value;

    /* Static data */
    static FILE* fp = 0;
    static char line[8192];
    char label[512];

    if (fp == 0) {
        std::string path = m_cgroup_memory_kernel_path + "/memory.stat";
        if ((fp = fopen(path.c_str(), "r")) == NULL) {
            fp = 0;
            return;
        }
    } else
        rewind(fp);

    g_output.psection_start("cgroup_memory_stats");
    while (fgets(line, 1000, fp) != NULL) {
        len = strlen(line);
        if (strncmp(line, "total_", 6) != 0)
            continue; // skip NON-totals: collect only cgroup-total values

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
        value = 0;
        sscanf(line, "%s %lu", label, &value);

        if (allowedStatsNames.empty() /* all stats must be put in output */
            || allowedStatsNames.find(label) != allowedStatsNames.end())
            g_output.plong(label, value);
    }

    if (read_integer(m_cgroup_memory_kernel_path + "/memory.failcnt", value))
        g_output.plong("failcnt", value);

    g_output.psection_end();
}

void CMonitorCollectorApp::cgroup_proc_cpuacct(double elapsed_sec, bool print)
{
    if (!m_bCGroupsFound)
        return;

    /* NOTE: newer distros have stats like
     *
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

    /* structure to recall previous values */
    struct cpuacct_utilisation {
        uint64_t counter_nsec_user_mode;
        uint64_t counter_nsec_sys_mode;
    };
    static struct cpuacct_utilisation prev_values[MAX_LOGICAL_CPU] = { 0 };

    // non-static data:
    char label[512];

    std::string path = m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_sys";
    if (file_or_dir_exists(path.c_str())) {

        // this system supports per-cpu system/user stats:

        std::vector<uint64_t> counter_nsec_sys_mode;
        if (!read_cpuacct_line(path, counter_nsec_sys_mode))
            return;

        std::vector<uint64_t> counter_nsec_user_mode;
        if (!read_cpuacct_line(m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_user", counter_nsec_user_mode))
            return;

        if (counter_nsec_sys_mode.size() != counter_nsec_user_mode.size())
            return;
        if (counter_nsec_sys_mode.empty())
            return;

        g_logger.LogDebug(
            "Found cpuacct.usage_percpu_sys/user cgroups; computing CPU usage for %.2fsec delta time and %zu CPUs "
            "(print=%d)\n",
            elapsed_sec, counter_nsec_user_mode.size(), print);

        if (print)
            g_output.psection_start("cgroup_cpuacct_stats");
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
            g_logger.LogDebug("CPU %d, prev user=%lu, prev sys=%lu", i, prev_values[i].counter_nsec_user_mode,
                prev_values[i].counter_nsec_sys_mode);
            if (cgroup_is_allowed_cpu(i) && print && elapsed_sec > MIN_ELAPSED_SECS) {
                double cpuUserPercent = // force newline
                    100 * ((double)(counter_nsec_user_mode[i] - prev_values[i].counter_nsec_user_mode))
                    / (elapsed_sec * 1E9);
                double cpuSysPercent = // force newline
                    100 * ((double)(counter_nsec_sys_mode[i] - prev_values[i].counter_nsec_sys_mode))
                    / (elapsed_sec * 1E9);

                // output JSON counter
                sprintf(label, "cpu%zu", i);
                g_output.psubsection_start(label);
                g_output.pdouble("user", cpuUserPercent);
                g_output.pdouble("sys", cpuSysPercent);
                g_output.psubsection_end();
            }

            // save for next cycle
            prev_values[i].counter_nsec_user_mode = counter_nsec_user_mode[i];
            prev_values[i].counter_nsec_sys_mode = counter_nsec_sys_mode[i];
        }
        if (print)
            g_output.psection_end();
    } else {

        // just get the per-cpu total:

        std::vector<uint64_t> counter_nsec_user_mode;
        if (!read_cpuacct_line(m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu", counter_nsec_user_mode))
            return;
        if (counter_nsec_user_mode.empty())
            return;

        g_logger.LogDebug("Reading data from cgroup cpuacct.usage_percpu");

        if (print)
            g_output.psection_start("cgroup_cpuacct_stats");
        for (size_t i = 0; i < counter_nsec_user_mode.size(); i++) {

            /*
             * Same comments for USER/SYS computations done above apply here!
             */
            if (cgroup_is_allowed_cpu(i) && print && elapsed_sec > MIN_ELAPSED_SECS) {
                double cpuUserPercent = // force newline
                    100 * ((double)(counter_nsec_user_mode[i] - prev_values[i].counter_nsec_user_mode))
                    / (elapsed_sec * 1E9);

                // output JSON counter
                sprintf(label, "cpu%zu", i);
                g_output.psubsection_start(label);
                g_output.pdouble("user", cpuUserPercent);
                g_output.psubsection_end();
            }

            // save for next cycle
            prev_values[i].counter_nsec_user_mode = counter_nsec_user_mode[i];
        }
        if (print)
            g_output.psection_end();
    }
}

bool CMonitorCollectorApp::cgroup_collect_pids(std::vector<pid_t>& pids)
{
    std::string path = m_cgroup_cpuacct_kernel_path + "/tasks";
    g_logger.LogDebug("Trying to read tasks for my cgroup from %s.\n", path.c_str());
    if (!file_or_dir_exists(path.c_str()))
        return false;

    std::ifstream inputf(path);
    if (!inputf.is_open())
        return false; // cannot read the cgroup information!

    DEBUGLOG_FUNCTION_START();

    std::string line;
    while (std::getline(inputf, line)) {
        uint64_t pid;
        if (string2int(line, pid))
            pids.push_back((pid_t)pid);
    }

    return true;
}

void CMonitorCollectorApp::cgroup_proc_tasks(double elapsed_sec, OutputFields output_opts)
{
    char str[256];

    if (!m_bCGroupsFound)
        return;

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
        procsinfo_t procData;
        memset(&procData, 0, sizeof(procData));
        cgroup_proc_procsinfo(all_pids[i], &procData, output_opts);

        // FIXME: allow to process threads
        if (procData.pi_pid == procData.pi_tgid)
            currDB.insert(std::make_pair(all_pids[i], procData));
        // else
        // g_logger.LogDebug("Found thread %d %d", procData.pi_pid, procData.pi_tgid);
    }

    if (output_opts == PF_NONE)
        return;

    // Sort the processes by their "score" by inserting them into an ordered map
    assert(m_topper.empty());
    for (const auto& current_entry : currDB) {
        const procsinfo_t* pcurrent = &current_entry.second;

        // find the previous stats for this PID:
        const procsinfo_t* pprev = &prevDB[current_entry.first /* pid */];

        uint64_t score = compute_proc_score(pcurrent, pprev, elapsed_sec);
        proc_topper_t newEntry = { .current = pcurrent, .prev = pprev };
        m_topper.insert(std::make_pair(score, newEntry));
    }

    if (m_topper.empty())
        return;

    g_logger.LogDebug("Tracking %zu/%zu processes/threads; min/max score found: %lu/%lu", // force newline
        currDB.size(), all_pids.size(), m_topper.begin()->first, m_topper.rbegin()->first);

    // Now output all data for each process, starting from the minimal score PROCESS_SCORE_IGNORE_THRESHOLD
    static double ticks = (double)sysconf(_SC_CLK_TCK); // clock ticks per second
    size_t nProcsOverThreshold = 0;
    g_output.psection_start("cgroup_tasks");
    for (auto entry = m_topper.lower_bound(PROCESS_SCORE_IGNORE_THRESHOLD); entry != m_topper.end(); entry++) {
        uint64_t score = (*entry).first;
        const procsinfo_t* p = (*entry).second.current;
        const procsinfo_t* q = (*entry).second.prev;

#define CURRENT(member) (p->member)
#define PREVIOUS(member) (q->member)
#define TIMEDELTA(member) (CURRENT(member) - PREVIOUS(member))
#define COUNTDELTA(member) ((PREVIOUS(member) > CURRENT(member)) ? 0 : (CURRENT(member) - PREVIOUS(member)))

        sprintf(str, "pid_%ld", (long)CURRENT(pi_pid));
        g_output.psubsection_start(str);
        g_output.plong("cmon_score", score);

        /*
         * Process fields
         */
        g_output.pstring("cmd", CURRENT(pi_comm)); // Full command line can be found /proc/PID/cmdline with zeros in it!
        g_output.plong("pid", CURRENT(pi_pid));
        g_output.plong("ppid", CURRENT(pi_ppid));
        g_output.plong("pgrp", CURRENT(pi_pgrp));
        g_output.plong("priority", CURRENT(pi_priority));
        g_output.plong("nice", CURRENT(pi_nice));
        g_output.plong("session", CURRENT(pi_session));
        g_output.plong("tty_nr", CURRENT(pi_tty_nr));
        // g_output.phex("flags", CURRENT(pi_flags));
        g_output.pstring("state", get_state(CURRENT(pi_state)));
        g_output.plong("threads", CURRENT(pi_num_threads));
        g_output.pdouble("start_time_secs", (double)(CURRENT(pi_start_time)) / ticks);
        g_output.plong("uid", CURRENT(uid));
        if (strlen(CURRENT(username)) > 0)
            g_output.pstring("username", CURRENT(username));

        /*
         * CPU fields
         * NOTE: all CPU fields specify amount of time, measured in units of USER_HZ
                 (1/100ths of a second on most architectures); this means that if the
                 _delta_ CPU value reported is 60 in mode X, then that mode took 60% of the CPU!
                 IOW there is no need to do any math to produce a percentage, just taking
                 the delta of the absolute, monotonic-increasing value and divide by the time
        */
        g_output.pdouble("cpu_tot", (TIMEDELTA(pi_utime) + TIMEDELTA(pi_stime)) / elapsed_sec);
        g_output.pdouble("cpu_usr", TIMEDELTA(pi_utime) / elapsed_sec);
        g_output.pdouble("cpu_sys", TIMEDELTA(pi_stime) / elapsed_sec);

        // provide also the total, monotonically-increasing CPU time:
        // this is used by chart script to produce the "top of the topper" chart
        g_output.pdouble("cpu_usr_total_secs", CURRENT(pi_utime) / ticks);
        g_output.pdouble("cpu_sys_total_secs", CURRENT(pi_stime) / ticks);
        g_output.plong("cpu_last", CURRENT(pi_last_cpu));

        /*
         * Memory fields
         */
        if (output_opts == PF_ALL) {
            g_output.plong("mem_size_kb", CURRENT(statm_size) * PAGESIZE_BYTES / 1024);
            g_output.plong("mem_resident_kb", CURRENT(statm_resident) * PAGESIZE_BYTES / 1024);
            g_output.plong("mem_restext_kb", CURRENT(statm_trs) * PAGESIZE_BYTES / 1024);
            g_output.plong("mem_resdata_kb", CURRENT(statm_drs) * PAGESIZE_BYTES / 1024);
            g_output.plong("mem_share_kb", CURRENT(statm_share) * PAGESIZE_BYTES / 1024);
        }
        g_output.pdouble("mem_minor_fault", COUNTDELTA(pi_minflt) / elapsed_sec);
        g_output.pdouble("mem_major_fault", COUNTDELTA(pi_majflt) / elapsed_sec);
        g_output.plong("mem_virtual_bytes", CURRENT(pi_vsize));
        g_output.plong("mem_rss_bytes", CURRENT(pi_rss) * PAGESIZE_BYTES);
        g_output.plong("mem_rss_limit", CURRENT(pi_rsslimit));

        /*
         * Signal fields
         */
#if PROCESS_DEBUGGING_ADDRESSES_SIGNALS
        /* NOT INCLUDED AS THEY ARE FOR DEBUGGING AND NOT PERFORMANCE TUNING */
        g_output.phex("start_code", CURRENT(pi_start_code));
        g_output.phex("end_code", CURRENT(pi_end_code));
        g_output.phex("start_stack", CURRENT(pi_start_stack));
        g_output.phex("esp_stack_pointer", CURRENT(pi_esp));
        g_output.phex("eip_instruction_pointer", CURRENT(pi_eip));
        g_output.phex("signal_pending", CURRENT(pi_signal_pending));
        g_output.phex("signal_blocked", CURRENT(pi_signal_blocked));
        g_output.phex("signal_ignore", CURRENT(pi_signal_ignore));
        g_output.phex("signal_catch", CURRENT(pi_signal_catch));
        g_output.phex("signal_exit", CURRENT(pi_signal_exit));
        g_output.phex("wchan", CURRENT(pi_wchan));
        /* NOT INCLUDED AS THEY ARE FOR DEBUGGING AND NOT PERFORMANCE TUNING */
#endif
        if (output_opts == PF_ALL) {
            g_output.plong("swap_pages", CURRENT(pi_swap_pages));
            g_output.plong("child_swap_pages", CURRENT(pi_child_swap_pages));
            g_output.plong("realtime_priority", CURRENT(pi_realtime_priority));
            g_output.plong("sched_policy", CURRENT(pi_sched_policy));
        }

        /*
         * I/O fields
         */
        g_output.pdouble("io_delayacct_blkio_secs", (double)CURRENT(pi_delayacct_blkio_ticks) / ticks);
        g_output.plong("io_rchar", TIMEDELTA(io_rchar) / elapsed_sec);
        g_output.plong("io_wchar", TIMEDELTA(io_wchar) / elapsed_sec);
        g_output.plong("io_read_bytes", TIMEDELTA(io_read_bytes) / elapsed_sec);
        g_output.plong("io_write_bytes", TIMEDELTA(io_write_bytes) / elapsed_sec);

        // provide also the total, monotonically-increasing I/O time:
        // this is used by chart script to produce the "top of the topper" chart
        g_output.plong("io_total_read", CURRENT(io_rchar));
        g_output.plong("io_total_write", CURRENT(io_wchar));

        g_output.psubsection_end();
        nProcsOverThreshold++;
    }
    g_output.psection_end();

    g_logger.LogDebug("%zu processes found over score threshold", nProcsOverThreshold);
    m_topper.clear();
}
