/*
 * cgroups_network.cpp -- code for collecting BY-NETWORK-INTERFACE statistics
                          for PIDs/TIDs inside the monitored cgroup
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

// ----------------------------------------------------------------------------------
// C++ Helper functions
// ----------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCgroups::cgroup_proc_network_interfaces(double elapsed_sec, OutputFields output_opts)
{
    char str[256];

    if (m_nCGroupsFound == CG_NONE)
        return;

    DEBUGLOG_FUNCTION_START();

    if (m_num_network_samples_collected == 0)
        output_opts = PF_NONE; // the first sample is used as bootstrap: we cannot generate any meaningful delta and
                               // thus any meaningful output

    m_num_network_samples_collected++;

    // collect all PIDs for current cgroup
    std::vector<pid_t> all_pids;
    switch (m_nCGroupsFound) {
    case CG_VERSION1:
        // in cgroups v1 all TIDs are available in the cgroup file named "tasks"
        // of course here we're assuming that the "tasks" under the cpuacct cgroup are the ones
        // the user is interested to monitor... in theory the "tasks" under other controllers like "memory"
        // might be different; in practice with Docker/LXC/Kube that does not happen
        if (!cgroup_collect_pids(m_cgroup_cpuacct_kernel_path + "/tasks", all_pids))
            return;
        break;

    case CG_VERSION2:
        if (!cgroup_collect_pids(m_cgroup_cpuacct_kernel_path + "/cgroup.procs", all_pids))
            return;
        break;

    case CG_NONE:
        assert(0);
        return;
    }

    // now take the first PID and assume its network namespace is the one the user is interested about;
    // we will monitor that network namespace (and only that) for the entire lifetime of its cgroup;
    // in theory each PID inside a cgroup can have its own network namespace (and PIDs can enter/leave cgroups at any
    // time) but in practice with typical container technologies like Docker, LXC and Kubernetes, all processes inside a
    // cgroup share the same, fixed network namespace; so that this assumption should be OK.
    if (all_pids.empty()) {
        CMonitorLogger::instance()->LogError("ERROR: could not find any PID in cgroup");
        return;
    }

    pid_t first = all_pids[0];

    /*
        IMPORTANT: there are at least two methods to monitor network statistics of a particular network namespace:

        FIRST METHOD: for each sample:
            * open the /proc/<pid>/ns/net file where <pid> is the PID of a process inside that network namespace
            * use setns(fd, 0) to enter the network namespace
            * read /proc/net/dev
            * use setns(fd_our_own_ns, 0) to exit the ns

        SECOND METHOD:
            * just read /proc/<pid>/net/dev file where <pid> is the PID of a process inside that network namespace

        The two methods look identical in my tests, with Linux 3.10.0-1160.36.2.el7.x86_64 and Docker version 19.03.15:
             PID_OF_A_PROCESS_INSIDE_DOCKER=3468718
             nsenter --target $PID_OF_A_PROCESS_INSIDE_DOCKER -n cat /proc/net/dev ; cat /proc/$PID_OF_A_PROCESS_INSIDE_DOCKER/net/dev
        yields:
            Inter-|   Receive                                                |  Transmit
            face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
            eth0: 1180404195 21807303    0    0    0     0          0         0 1624659358 21969100    0    0    0     0       0          0
                lo: 5402517   80034    0    0    0     0          0         0  5402517   80034    0    0    0     0       0          0
            Inter-|   Receive                                                |  Transmit
            face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
            eth0: 1180404195 21807303    0    0    0     0          0         0 1624659358 21969100    0    0    0     0       0          0
                lo: 5402517   80034    0    0    0     0          0         0  5402517   80034    0    0    0     0       0          0
        the numbers are identical... so we go with SECOND METHOD which of course is way simpler
    */
    // FIXME refactor code from
    //    proc_net_dev();
    // to take a FD and fill an output structure

    m_pOutput->psection_start("cgroup_network");
    for (auto entry = m_topper_procs.lower_bound(m_pCfg->m_nProcessScoreThreshold); entry != m_topper_procs.end();
         entry++) {

        m_pOutput->plong("cmon_score", score);
    }
    m_pOutput->psection_end();

    // CMonitorLogger::instance()->LogDebug("%zu processes found over score threshold", nProcsOverThreshold);
}
