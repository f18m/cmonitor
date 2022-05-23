/*
 * system_network.cpp - code for collecting SYSTEM-level network-statistics (i.e. not cgroup-aware)
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

#include "logger.h"
#include "output_frontend.h"
#include "system.h"
#include "utils_string.h"
#include <arpa/inet.h>
#include <assert.h>
#include <ifaddrs.h>
#include <net/if.h>

/*
 read /proc/net/dev
 */
void CMonitorSystem::sample_net_dev(double elapsed_sec, OutputFields output_opts)
{
    static bool first_time = true;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_NETWORK) == 0)
        return;

    if (!m_pCfg->m_strPrometheusPort.empty() && !m_pCfg->m_strPrometheusAddress.empty()) {
        for (size_t i = 0; i < sizeof(prometheus_kpi_network) / sizeof(prometheus_kpi_network[0]); i++) {
            m_pOutput->init_prometheus_kpi(prometheus_kpi_network[i]);
        }
    }

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

        switch (output_opts) {
        case PF_NONE:
            assert(0);
            break;

        case PF_ALL:
            m_pOutput->plong("ibytes", DELTA_NET_STAT(if_ibytes));
            m_pOutput->plong("ipackets", DELTA_NET_STAT(if_ipackets));
            m_pOutput->plong("ierrs", DELTA_NET_STAT(if_ierrs));
            m_pOutput->plong("idrop", DELTA_NET_STAT(if_idrop));
            m_pOutput->plong("ififo", DELTA_NET_STAT(if_ififo));
            m_pOutput->plong("iframe", DELTA_NET_STAT(if_iframe));

            m_pOutput->plong("obytes", DELTA_NET_STAT(if_obytes));
            m_pOutput->plong("opackets", DELTA_NET_STAT(if_opackets));
            m_pOutput->plong("oerrs", DELTA_NET_STAT(if_oerrs));
            m_pOutput->plong("odrop", DELTA_NET_STAT(if_odrop));
            m_pOutput->plong("ofifo", DELTA_NET_STAT(if_ofifo));

            m_pOutput->plong("ocolls", DELTA_NET_STAT(if_ocolls));
            m_pOutput->plong("ocarrier", DELTA_NET_STAT(if_ocarrier));
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