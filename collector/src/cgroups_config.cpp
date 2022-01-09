/*
 * cgroups_config.cpp -- code for getting CGROUP configuration
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
#include "utils_files.h"
#include "utils_string.h"
#include <assert.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

// ----------------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------------

#define GIGABYTE (1000ul * 1000ul * 1000ul)
#define MEMORY_LIMIT_MAX_VALUE (1000 * 1000 * GIGABYTE)

// ----------------------------------------------------------------------------------
// C++ Helper functions
// ----------------------------------------------------------------------------------

std::string CGroupDetected2string(CGroupDetected k)
{
    switch (k) {
    case CG_NONE:
        return "none";
    case CG_VERSION1:
        return "1";
    case CG_VERSION2:
        return "2";
    default:
        return "";
    }
}

// ----------------------------------------------------------------------------------
// Path Helper functions
// ----------------------------------------------------------------------------------

bool CMonitorCgroups::get_cgroup_paths_for_this_pid(cgroup_paths_map_t& cgroup_pathsOUT)
{
    /*
     * ABOUT /proc/%d/cgroup:
     *   See http://man7.org/linux/man-pages/man7/cgroups.7.html, look for "/proc/[pid]/cgroup (since Linux 2.6.24)"
     *   Each line is composed by:
     *                     hierarchy-ID:controller-list:cgroup-path
     *   where the line with EMPTY controller-list seems to be always the indicative name of the whole cgroup.
     *   Example contents on a baremetal process using systemd cgroups v1:
             $ cat /proc/self/cgroup
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

     *   Example contents on a baremetal process using systemd or docker cgroups v2:
            $ cat /proc/self/cgroup
                        0::/user.slice/user-0.slice/session-1.scope

        See https://github.com/systemd/systemd/blob/main/docs/CGROUP_DELEGATION.md
     */
    CMonitorLogger::instance()->LogDebug("Inspecting file %s\n", m_proc_self_cgroup.c_str());

    std::ifstream inputf(m_proc_self_cgroup);
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

bool CMonitorCgroups::are_cgroups_v2_enabled(std::string& cgroup_pathOUT)
{
    /*
     * ABOUT /proc/%d/mounts:
     *   See http://man7.org/linux/man-pages/man5/fstab.5.html
     *   Each line is composed by:
     *                     fs_spec  fs_file  fs_vfstype  fs_mntops  fs_freq  fs_passno
     *   We are interested into the lines that provide the "cgroup" or "cgroup2" fs_spec
     *
     *   Example contents for proc/%d/mounts when using cgroups v2:
     *     under Docker:
     *      cgroup2 /sys/fs/cgroup cgroup2 rw,seclabel,nosuid,nodev,noexec,relatime,nsdelegate 0 0
     *
     *   See https://github.com/systemd/systemd/blob/main/docs/CGROUP_DELEGATION.md
     */

    CMonitorLogger::instance()->LogDebug("Inspecting file %s\n", m_proc_self_mounts.c_str());
    cgroup_pathOUT = "";

    std::ifstream inputf(m_proc_self_mounts);
    if (!inputf.is_open())
        return false; // cannot read the cgroup information!

    std::string line;
    unsigned int nline = 0, ncgroups_v2 = 0, ncgroups_v1 = 0;
    while (std::getline(inputf, line)) {
        // cout << line << '\n';
        std::vector<std::string> tuple = split_string_in_array(line, ' ');
        if (tuple.size() != 6) {
            CMonitorLogger::instance()->LogDebug("Invalid mount format found at line %d: [%s]\n", nline, line.c_str());
            return false; // invalid format
        }

        std::string fs_spec = tuple[0];
        std::string fs_file = tuple[1];
        std::string fs_vfstype = tuple[2];
        // std::string fs_mntops = tuple[3];

        /*
         NOTE: strangely enoug the "fs_spec" seems to be sometime "cgroup" (e.g. checkout
         fedora35-Linux-5.14.17-x86_64-docker unit test data) and sometimes "cgroup2" (e.g. checkout
         fedora35-Linux-5.14.17-x86_64-systemd unit test data)... regardless of that the "fs_vfstype" is always
         "cgroup2" for v2 of cgroups!
        */
        if ((fs_spec == "cgroup" || fs_spec == "cgroup2") && fs_vfstype == "cgroup2") {
            // found the "cgroup type" that belongs to cgroups v2... note that in this "if" branch the "cgroup_type"
            // is not used: cgroupsv2, also known as "unified cgroup hierarchy", do have a single path for the whole
            // cgroup, instead of having multiple ones for each different "cgroup_type"
            cgroup_pathOUT = fs_file;
            ncgroups_v2++;
        } else if (fs_vfstype == "cgroup") {
            ncgroups_v1++;
        }

        nline++;
    }

    if (ncgroups_v1 == 0 && ncgroups_v2 >= 1) {
        /*
            As described here:
           https://github.com/systemd/systemd/blob/main/docs/CGROUP_DELEGATION.md#three-different-tree-setups- systemd
           has 3 working modes; in "hybrid" mode (used e.g. on Ubuntu 20.04) we want to monitor cgroups v1 only; for
           this reason we return true (v2 version) only if we are sure we're not in "hybrid" mode (ncgroups_v1 == 0)!
        */
        return true;
    }

    return false; // cgroup name not found
}

bool CMonitorCgroups::get_cgroup_v1_abs_path_prefix_for_this_pid(
    const std::string& cgroup_type, std::string& cgroup_pathOUT)
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
    CMonitorLogger::instance()->LogDebug("Inspecting file %s\n", m_proc_self_mounts.c_str());

    std::ifstream inputf(m_proc_self_mounts);
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

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCgroups::init( // force newline
    bool include_threads, // force newline
    const std::string& cgroup_prefix_for_test, // force newline
    const std::string& proc_prefix_for_test, // force newline
    uint64_t my_own_pid_for_test)
{
    DEBUGLOG_FUNCTION_START();

    m_nCGroupsFound = CG_NONE;
    m_cgroup_systemd_name = "N/A";
    m_cgroup_processes_include_threads = include_threads;
    m_proc_prefix = proc_prefix_for_test;

    /*
     Run our heuristic logic to get full paths to all the cgroup controllers we want to monitor.
     This logic is tricky because of:
      * different possible cgroup managers: systemd (no container case), Docker, LXC, Kubelet, etc
      * different cgroup versions: 1 and 2
      * different Linux distribution policies
      * different Linux kernels
    */
    if (!detect_cgroup_ver_and_paths_from_myself(cgroup_prefix_for_test, my_own_pid_for_test))
        return; // the function has already logged errors
    if (m_pCfg->m_strCGroupName.empty() || m_pCfg->m_strCGroupName == "self") {
        if (!detect_my_own_cgroup())
            return; // the function has already logged errors
    } else {
        if (!detect_user_provided_cgroup())
            return; // the function has already logged errors
    }

    /*
     Once cgroups have been fully defined, read the RESOURCE LIMITS
    */
    switch (m_nCGroupsFound) {
    case CG_NONE:
        return;
    case CG_VERSION1:
        v1_read_limits();
        break;
    case CG_VERSION2:
        v2_read_limits();
        break;
    }

    // if reading limits fail, we might have disabled cgroup monitoring:
    if (m_nCGroupsFound == CG_NONE)
        return; // the functions above have already logged errors

    init_cpuacct(cgroup_prefix_for_test);
    init_memory(cgroup_prefix_for_test);
    init_network(cgroup_prefix_for_test);
    init_processes(cgroup_prefix_for_test);
}

bool CMonitorCgroups::detect_cgroup_ver_and_paths_from_myself(
    const std::string& cgroup_prefix_for_test, uint64_t my_own_pid_for_test)
{
    // This function will set ABSOLUTE CGROUP PATH PREFIXES
    // Typical examples (cgroup v1)
    //    m_cgroup_memory_kernel_path   = /sys/fs/cgroup/memory/
    //    m_cgroup_cpuacct_kernel_path  = /sys/fs/cgroup/cpu,cpuacct/     or     /sys/fs/cgroup/cpuacct,cpu/
    //    m_cgroup_cpuset_kernel_path   = /sys/fs/cgroup/cpuset/
    // Typical examples (cgroup v2)
    //    m_cgroup_memory_kernel_path = m_cgroup_cpuacct_kernel_path = m_cgroup_cpuset_kernel_path =
    //        /sys/fs/cgroup/system.slice/

    if (my_own_pid_for_test == UINT64_MAX) {
        // normal behavior (no unit tests)
        m_my_pid = getpid();
        m_proc_self_cgroup = "/proc/self/cgroup";
        m_proc_self_mounts = "/proc/self/mounts";
    } else {
        // read all information to a) understand cgroup version and b) find out cgroup paths
        // from a unit testing file instead of looking at our own PID kernel files!!
        m_my_pid = my_own_pid_for_test;
        std::string t = m_proc_prefix + "/proc/" + fmt::format("{}", my_own_pid_for_test);
        m_proc_self_cgroup = t + "/cgroup";
        m_proc_self_mounts = t + "/mounts";
    }

    CMonitorLogger::instance()->LogDebug("My own PID is %d; self cgroup file is %s; self mounts file is %s\n", m_my_pid,
        m_proc_self_cgroup.c_str(), m_proc_self_mounts.c_str());

    // unit testing support:
    if (!cgroup_prefix_for_test.empty())
        assert(file_or_dir_exists(cgroup_prefix_for_test.c_str()));

    /*
        {
            // assume we're unit testing cgroups v1
            m_cgroup_memory_kernel_path = cgroup_prefix_for_test + "/sys/fs/cgroup/memory"; // force newline
            m_cgroup_cpuacct_kernel_path = cgroup_prefix_for_test + "/sys/fs/cgroup/cpu,cpuacct"; // force newline
            m_cgroup_cpuset_kernel_path = cgroup_prefix_for_test + "/sys/fs/cgroup/cpuset"; // force newline
            m_nCGroupsFound = CG_VERSION1;
        }
        else
        {
            // assume we're unit testing cgroups v2
            m_cgroup_memory_kernel_path = cgroup_prefix_for_test;
            m_cgroup_cpuacct_kernel_path = cgroup_prefix_for_test;
            m_cgroup_cpuset_kernel_path = cgroup_prefix_for_test;
            m_nCGroupsFound = CG_VERSION2;
        }

    } else*/

    std::string cgroupsv2_basepath;
    m_cpuacct_controller_name = "cpu,cpuacct";

    if (are_cgroups_v2_enabled(cgroupsv2_basepath)) {
        m_nCGroupsFound = CG_VERSION2;

        // the unified hierarchy of cgroups used in v2 means that all cgroup controllers share the same path:
        m_cgroup_memory_kernel_path = cgroup_prefix_for_test + cgroupsv2_basepath;
        m_cgroup_cpuacct_kernel_path = cgroup_prefix_for_test + cgroupsv2_basepath;
        m_cgroup_cpuset_kernel_path = cgroup_prefix_for_test + cgroupsv2_basepath;

        CMonitorLogger::instance()->LogDebug("Detected cgroups v2 with path %s\n", m_cgroup_memory_kernel_path.c_str());
    } else {
        // try to detect cgroups v1
        m_nCGroupsFound = CG_VERSION1;

        if (!get_cgroup_v1_abs_path_prefix_for_this_pid("memory", m_cgroup_memory_kernel_path)) {
            CMonitorLogger::instance()->LogError(
                "Could not find the 'memory' cgroup path prefix. CGroup mode disabled.\n");
            m_nCGroupsFound = CG_NONE;
            return false;
        }

        if (!get_cgroup_v1_abs_path_prefix_for_this_pid(m_cpuacct_controller_name, m_cgroup_cpuacct_kernel_path)) {

            // on some Linux distributions, the name of the cgroup has the "cpu" and "cpuacct" names inverted..
            // retry inverting the order:
            m_cpuacct_controller_name = "cpuacct,cpu";

            if (!get_cgroup_v1_abs_path_prefix_for_this_pid(m_cpuacct_controller_name, m_cgroup_cpuacct_kernel_path)) {
                CMonitorLogger::instance()->LogError(
                    "Could not find the 'cpuacct' cgroup path prefix. CGroup mode disabled.\n");
                m_nCGroupsFound = CG_NONE;
                return false;
            }
        }

        if (!get_cgroup_v1_abs_path_prefix_for_this_pid("cpuset", m_cgroup_cpuset_kernel_path)) {
            CMonitorLogger::instance()->LogError(
                "Could not find the 'cpuset' cgroup path prefix. CGroup mode disabled.\n");
            m_nCGroupsFound = CG_NONE;
            return false;
        }

        // add unit-testing special prefix if any:
        m_cgroup_memory_kernel_path = cgroup_prefix_for_test + m_cgroup_memory_kernel_path;
        m_cgroup_cpuacct_kernel_path = cgroup_prefix_for_test + m_cgroup_cpuacct_kernel_path;
        m_cgroup_cpuset_kernel_path = cgroup_prefix_for_test + m_cgroup_cpuset_kernel_path;
    }

    CMonitorLogger::instance()->LogDebug(
        "Detected cgroup version %s\n", CGroupDetected2string(m_nCGroupsFound).c_str());
    CMonitorLogger::instance()->LogDebug("Detected cpuset cgroup mounted at %s\n", m_cgroup_cpuset_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug(
        "Detected cpuacct cgroup mounted at %s\n", m_cgroup_cpuacct_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Detected memory cgroup mounted at %s\n", m_cgroup_memory_kernel_path.c_str());

    return true;
}

bool CMonitorCgroups::detect_my_own_cgroup()
{
    // assume the user wants to monitor the same cgroup where cmonitor_collector is running:
    // (presumably a systemd-created cgroup outside of any container)

    CMonitorLogger::instance()->LogDebug("No cgroup name provided; defaulting to 'self' cgroup monitoring; thus "
                                         "trying to autodetect my own cgroup.");

    cgroup_paths_map_t cgroup_paths;
    if (!get_cgroup_paths_for_this_pid(cgroup_paths)) {
        CMonitorLogger::instance()->LogDebug(
            "Could not get the cgroup paths for cmonitor_collector itself. CGroup mode disabled.\n");
        m_nCGroupsFound = CG_NONE;
        return false;
    }

    switch (m_nCGroupsFound) {
    case CG_NONE:
        assert(0);
        break;
    case CG_VERSION1:
        if (cgroup_paths.find("name=systemd") == cgroup_paths.end()) {
            CMonitorLogger::instance()->LogError("Could not find the cgroup controller 'name=systemd' inside "
                                                 "'%s'. CGroup mode disabled.\n",
                m_proc_self_cgroup.c_str());
            m_nCGroupsFound = CG_NONE;
            return false;
        }
        m_cgroup_systemd_name = cgroup_paths["name=systemd"];

        CMonitorLogger::instance()->LogDebug("Detected as cgroup name: %s", m_cgroup_systemd_name.c_str());

        if (search_my_pid_in_cgroups()) { // also updates m_cgroup_processes_path
            // in this case we're likely inside a Docker or LXC container so we were able to find our own PID in one
            // of the "undecorated" absolute paths detected previously by detect_cgroup_ver_and_paths_from_myself()
        } else {

            if (cgroup_paths.find("memory") == cgroup_paths.end() || // fn
                cgroup_paths.find("cpuset") == cgroup_paths.end() || // fn
                cgroup_paths.find(m_cpuacct_controller_name) == cgroup_paths.end()) {
                CMonitorLogger::instance()->LogError("Could not find one the required cgroup controllers 'memory', "
                                                     "'cpuset' or '%s' inside '%s'. CGroup mode disabled.\n",
                    m_cpuacct_controller_name.c_str(), m_proc_self_cgroup.c_str());
                m_nCGroupsFound = CG_NONE;
                return false;
            }

            // try to adjust the full cgroup paths by adding the cgroup paths read from /proc/self/cgroup
            // to the absolute prefixes obtained from /proc/self/mounts: this is typically necessary when running
            // outside Docker/LXC and thus just inside a cgroup created by systemd:
            m_cgroup_memory_kernel_path += "/" + cgroup_paths["memory"];
            m_cgroup_cpuacct_kernel_path += "/" + cgroup_paths[m_cpuacct_controller_name];
            m_cgroup_cpuset_kernel_path += "/" + cgroup_paths["cpuset"];
            CMonitorLogger::instance()->LogDebug(
                "Adjusting cpuset cgroup path to %s\n", m_cgroup_cpuset_kernel_path.c_str());
            CMonitorLogger::instance()->LogDebug(
                "Adjusting cpuacct cgroup path to %s\n", m_cgroup_cpuacct_kernel_path.c_str());
            CMonitorLogger::instance()->LogDebug(
                "Adjusting memory cgroup path to %s\n", m_cgroup_memory_kernel_path.c_str());
            if (search_my_pid_in_cgroups()) // also updates m_cgroup_processes_path
            {
                // successfully found our PID...

            } else {
                CMonitorLogger::instance()->LogError(
                    "Could not find the cgroup where my own PID %d is located. CGroup mode disabled.\n", m_my_pid);
                m_nCGroupsFound = CG_NONE;
                return false;
            }
        }
        break;
    case CG_VERSION2:
        if (cgroup_paths.find("") == cgroup_paths.end()) {
            CMonitorLogger::instance()->LogError("Could not find the cgroup controller 'name=systemd' inside "
                                                 "'%s'. CGroup mode disabled.\n",
                m_proc_self_cgroup.c_str());
            m_nCGroupsFound = CG_NONE;
            return false;
        }
        m_cgroup_systemd_name = cgroup_paths[""];

        CMonitorLogger::instance()->LogDebug("Detected as cgroup name: %s", m_cgroup_systemd_name.c_str());

        if (search_my_pid_in_cgroups()) { // also updates m_cgroup_processes_path
            // in this case we're likely inside a Docker or LXC container so we were able to find our own PID in one
            // of the "undecorated" absolute paths detected previously by detect_cgroup_ver_and_paths_from_myself()
        } else {

            // try to adjust the full cgroup paths by adding the cgroup paths read from /proc/self/cgroup
            // to the absolute prefixes obtained from /proc/self/mounts: this is typically necessary when running
            // outside Docker/LXC and thus just inside a cgroup created by systemd:
            m_cgroup_memory_kernel_path += "/" + m_cgroup_systemd_name;
            m_cgroup_cpuacct_kernel_path += "/" + m_cgroup_systemd_name;
            m_cgroup_cpuset_kernel_path += "/" + m_cgroup_systemd_name;
            CMonitorLogger::instance()->LogDebug(
                "Adjusting cpuset cgroup path to %s\n", m_cgroup_cpuset_kernel_path.c_str());
            CMonitorLogger::instance()->LogDebug(
                "Adjusting cpuacct cgroup path to %s\n", m_cgroup_cpuacct_kernel_path.c_str());
            CMonitorLogger::instance()->LogDebug(
                "Adjusting memory cgroup path to %s\n", m_cgroup_memory_kernel_path.c_str());
            if (search_my_pid_in_cgroups()) // also updates m_cgroup_processes_path
            {
                // successfully found our PID...

            } else {
                CMonitorLogger::instance()->LogError(
                    "Could not find the cgroup where my own PID %d is located. CGroup mode disabled.\n", m_my_pid);
                m_nCGroupsFound = CG_NONE;
                return false;
            }
        }
        break;
    }

    return true;
}

bool CMonitorCgroups::detect_user_provided_cgroup()
{
    // NOW DETECT ACTUAL CGROUP PATHS TO MONITOR
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
        return false;
    }

    m_cgroup_systemd_name = m_pCfg->m_strCGroupName;

    // set m_cgroup_processes_path
    search_processes_cgroup_path();

    CMonitorLogger::instance()->LogDebug("Set cpuset cgroup path to %s\n", m_cgroup_cpuset_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Set cpuacct cgroup path to %s\n", m_cgroup_cpuacct_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Set memory cgroup path to %s\n", m_cgroup_memory_kernel_path.c_str());
    CMonitorLogger::instance()->LogDebug("Set processes cgroup path to %s\n", m_cgroup_processes_path.c_str());

    return true;
}

void CMonitorCgroups::v1_read_limits()
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

    if (!read_cpuset_cpus(m_cgroup_cpuset_kernel_path, m_cgroup_cpus)) {
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

void CMonitorCgroups::v2_read_limits()
{
    // READ LIMITS IMPOSED BY CGROUPS
    // see https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html

    // NOTE: m_cgroup_memory_limit_bytes might assume the special value "max" reported by the
    // cgroup controller... it just means "no limit"... we set UINT64_MAX in that case
    if (!read_cgroupv2_integer_or_max(m_cgroup_memory_kernel_path + "/memory.max", m_cgroup_memory_limit_bytes)) {
        CMonitorLogger::instance()->LogError(
            "Could not read the memory limit from 'memory' cgroup. Assuming no memory limit.\n");
        m_cgroup_memory_limit_bytes = UINT64_MAX;
    } else
        CMonitorLogger::instance()->LogDebug("Found memory cgroup limiting to %luB, mounted at %s\n",
            m_cgroup_memory_limit_bytes, m_cgroup_memory_kernel_path.c_str());

    if (!read_cpuset_cpus(m_cgroup_cpuset_kernel_path, m_cgroup_cpus)) {
        CMonitorSystem::get_all_cpus(m_cgroup_cpus, m_proc_prefix + "/proc/stat");
        CMonitorLogger::instance()->LogError(
            "Could not read the CPUs from 'cpuset' cgroup. Assuming all cpus are available: %s.\n",
            stl_container2string(m_cgroup_cpus, ",").c_str());
    } else
        CMonitorLogger::instance()->LogDebug("Found cpuset cgroup limiting to CPUs %s, mounted at %s\n",
            stl_container2string(m_cgroup_cpus, ",").c_str(), m_cgroup_cpuset_kernel_path.c_str());

    // FIXME: m_cgroup_cpuacct_quota_us might assume the special value "max" reported by the
    // cgroup controller... it just means "no limit"... we need to handle that using UINT64_MAX
    if (!read_two_integers(
            m_cgroup_cpuacct_kernel_path + "/cpu.max", m_cgroup_cpuacct_quota_us, m_cgroup_cpuacct_period_us)) {
        CMonitorLogger::instance()->LogError(
            "Could not read the CPU period from 'cpuacct' cgroup. Assuming no CPU limit.\n");
        m_cgroup_cpuacct_quota_us = UINT64_MAX;
        m_cgroup_cpuacct_period_us = 100000; // standard values
    } else
        CMonitorLogger::instance()->LogDebug("Found cpuacct cgroup limiting at %lu/%lu usecs mounted at %s\n",
            m_cgroup_cpuacct_quota_us, m_cgroup_cpuacct_period_us, m_cgroup_cpuacct_kernel_path.c_str());

    // cpuset and memory cgroups found:
    CMonitorLogger::instance()->LogDebug(
        "CGroup monitoring successfully enabled. CGroup name is %s\n", m_cgroup_systemd_name.c_str());
}

bool CMonitorCgroups::search_my_pid_in_cgroups()
{
    // CGROUP CHECKS
    // now if we got the right paths, we should be able to find our pid in all these cgroups
    // NOTE: depending on the container technology (Docker, LXC or LXD) or on the absence of containers
    //       but presence of cgroups (like those created by systemd) we may have a masquerated PID
    //       (e.g. PID '8' inside a Docker container while the real PID is 2348 on baremetal)

    bool found = true;

    switch (m_nCGroupsFound) {
    case CG_NONE:
        found = false;
        break;
    case CG_VERSION1:
        if (search_integer(m_cgroup_memory_kernel_path + "/tasks", uint64_t(m_my_pid)))
            CMonitorLogger::instance()->LogDebug("Successfully found our PID %d in the 'memory' cgroup.\n", m_my_pid);
        else {
            CMonitorLogger::instance()->LogDebug("Could not find our PID %d in the 'memory' cgroup.\n", m_my_pid);
            found = false;
        }

        if (search_integer(m_cgroup_cpuacct_kernel_path + "/tasks", uint64_t(m_my_pid)))
            CMonitorLogger::instance()->LogDebug("Successfully found our PID %d in the 'cpuacct' cgroup.\n", m_my_pid);
        else {
            CMonitorLogger::instance()->LogDebug("Could not find our PID %d in the 'cpuacct' cgroup.\n", m_my_pid);
            found = false;
        }

        if (search_integer(m_cgroup_cpuset_kernel_path + "/tasks", uint64_t(m_my_pid)))
            CMonitorLogger::instance()->LogDebug("Successfully found our PID %d in the 'cpuset' cgroup.\n", m_my_pid);
        else {
            CMonitorLogger::instance()->LogDebug("Could not find our PID %d in the 'cpuset' cgroup.\n", m_my_pid);
            found = false;
        }

        if (found) {
            // to set the path for the "tasks" file we can use whatever of the 3 paths above:
            m_cgroup_processes_path = m_cgroup_memory_kernel_path;
        }
        break;
    case CG_VERSION2:
        // cgroups v2 use an unified hierarchy, so there's a single file to check:
        // see https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html
        // NOTE:
        //  at least on Ubuntu 20.04 even with cgroupsv2, the various paths
        //    m_cgroup_cpuset_kernel_path
        //    m_cgroup_cpuacct_kernel_path
        //    m_cgroup_cpuset_kernel_path so
        //  when running outside a Docker container (directly under systemd) are different
        //  despite the "unified" hierarchy... try them one by one:
        std::string paths[]
            = { m_cgroup_memory_kernel_path, m_cgroup_cpuacct_kernel_path, m_cgroup_cpuset_kernel_path };

        found = false;
        for (unsigned int i = 0; i < 3; i++) {
            if (search_integer(paths[i] + "/cgroup.procs", uint64_t(m_my_pid))) {
                found = true;
                m_cgroup_processes_path = paths[i];
                CMonitorLogger::instance()->LogDebug(
                    "Successfully found our PID %d in the cgroup v2 at '%s'.\n", m_my_pid, paths[i].c_str());
                break;
            } else {
                CMonitorLogger::instance()->LogDebug(
                    "Could not find our PID %d in the cgroup v2 at '%s'.\n", m_my_pid, paths[i].c_str());
            }
        }
        break;
    }

    return found;
}

bool CMonitorCgroups::search_processes_cgroup_path() // to be used in alternative to search_my_pid_in_cgroups() to
                                                     // set m_cgroup_processes_path
{
    // set m_cgroup_processes_path
    std::string paths[] = { m_cgroup_memory_kernel_path, m_cgroup_cpuacct_kernel_path, m_cgroup_cpuset_kernel_path };

    std::string proc_file_name;
    switch (m_nCGroupsFound) {
    case CG_NONE:
        return false;

    case CG_VERSION1:
        proc_file_name = "/tasks";
        break;

    case CG_VERSION2:
        proc_file_name = "/cgroup.procs";
        break;
    }

    for (unsigned int i = 0; i < 3; i++) {
        std::string attempt = paths[i] + proc_file_name;
        if (file_or_dir_exists(attempt.c_str())) {
            m_cgroup_processes_path = paths[i];
            CMonitorLogger::instance()->LogDebug("Successfully found list of PIDs/TIDs at '%s'.\n", attempt.c_str());
            break;
        } else {
            CMonitorLogger::instance()->LogDebug("Could not find list of PIDs/TIDs at '%s'.\n", attempt.c_str());
        }
    }

    return !m_cgroup_processes_path.empty();
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
        // FIXME we should add 100* to actually get a percentage, e.g. if we start a docker with --cpu-limit=2 we
        //       should see "200" as "cpu_quota_perc" not just "2". Or we rename to "cpu_quota" only
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
