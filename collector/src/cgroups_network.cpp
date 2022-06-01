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

// ----------------------------------------------------------------------------------
// C++ Helper functions
// ----------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCgroups::init_network(const std::string& cgroup_prefix_for_test)
{
    CMonitorLogger::instance()->LogDebug("Successfully initialized cgroup network monitoring.\n");
}

void CMonitorCgroups::sample_network_interfaces(double elapsed_sec, OutputFields output_opts)
{
    if (m_nCGroupsFound == CG_NONE)
        return;
    if ((m_pCfg->m_nCollectFlags & PK_CGROUP_NETWORK_INTERFACES) == 0)
        return;

#ifdef PROMETHEUS_SUPPORT
    if (m_pOutput->is_prometheus_enabled()) {
        size_t size = sizeof(prometheus_kpi_cgroup_network) / sizeof(prometheus_kpi_cgroup_network[0]);
        m_pOutput->init_prometheus_kpi(prometheus_kpi_cgroup_network, size);
    }
#endif
    DEBUGLOG_FUNCTION_START();

    if (m_num_network_samples_collected == 0)
        output_opts = PF_NONE; // the first sample is used as bootstrap: we cannot generate any meaningful delta and
                               // thus any meaningful output

    m_num_network_samples_collected++;

    // now take the first PID and assume its network namespace is the one the user is interested about;
    // we will monitor that network namespace (and only that) for the entire lifetime of its cgroup;
    // in theory each PID inside a cgroup can have its own network namespace (and PIDs can enter/leave cgroups at any
    // time) but in practice with typical container technologies like Docker, LXC and Kubernetes, all processes inside a
    // cgroup share the same, fixed network namespace; so that this assumption should be OK.
    if (m_cgroup_all_pids.empty()) {
        CMonitorLogger::instance()->LogError("ERROR: could not find any PID in cgroup");
        return;
    }

    pid_t first_pid = m_cgroup_all_pids[0];

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
             nsenter --target $PID_OF_A_PROCESS_INSIDE_DOCKER -n cat /proc/net/dev ; cat
       /proc/$PID_OF_A_PROCESS_INSIDE_DOCKER/net/dev yields: Inter-|   Receive |  Transmit face |bytes    packets errs
       drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed eth0: 1180404195
       21807303    0    0    0     0          0         0 1624659358 21969100    0    0    0     0       0          0
                lo: 5402517   80034    0    0    0     0          0         0  5402517   80034    0    0    0     0 0 0
            Inter-|   Receive                                                |  Transmit
            face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls
       carrier compressed eth0: 1180404195 21807303    0    0    0     0          0         0 1624659358 21969100    0
       0    0     0       0          0 lo: 5402517   80034    0    0    0     0          0         0  5402517   80034 0
       0    0     0       0          0 the numbers are identical... so we go with SECOND METHOD which of course is way
       simpler
    */

    // FIXME: use FastFileReader which optimizes the case where the PID chosen 1st time stays constant
    std::string filename = fmt::format("{}/proc/{}/net/dev", m_proc_prefix, first_pid);

    std::set<std::string> empty_whitelist;

    // read new stats
    netinfo_map_t new_stats;
    CMonitorSystem::read_net_dev_stats(filename, empty_whitelist, new_stats);

    // output delta stats
    if (output_opts != PF_NONE) {
        m_pOutput->psection_start("cgroup_network");
        CMonitorSystem::output_net_dev_stats(m_pOutput, elapsed_sec, new_stats, m_previous_netinfo, output_opts);
        m_pOutput->psection_end();
    }

    // finally remember the last sampled stats:
    m_previous_netinfo = new_stats;
}
