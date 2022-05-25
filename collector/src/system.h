/*
 * system.h -- code for collecting SYSTEM-level statistics (i.e. not cgroup-aware)
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

#pragma once

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------

#include "cmonitor.h"
#include "fast_file_reader.h"
#include <map>
#include <set>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "PrometheusKpi.h"

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

static const prometheus_kpi_descriptor prometheus_kpi_disk[] = {
    // baremetal : disk
    { "disks_reads", KPI_TYPE::Gauge, "total reads completed successfully" },
    { "disks_rmerge", KPI_TYPE::Gauge, "total reads merged" },
    { "disks_rkb", KPI_TYPE::Gauge, "total number of sectors read from disk" },
    { "disks_rmsec", KPI_TYPE::Gauge, "total time spent reading (ms)" },
    { "disks_writes", KPI_TYPE::Gauge, "total writes completed successfully" },
    { "disks_wmerge", KPI_TYPE::Gauge, "total writes merged" },
    { "disks_wmsec", KPI_TYPE::Gauge, "total time spent writting (ms)" },
    { "disks_wkb", KPI_TYPE::Gauge, "total number of sectors writeen to disk" },
    { "disks_inflight", KPI_TYPE::Gauge, "I/Os currently in progress" },
    { "disks_time", KPI_TYPE::Gauge, "time spent doing I/Os (ms)" },
    { "disks_backlog", KPI_TYPE::Gauge, "weighted time spent doing I/Os (ms)" },
    { "disks_xfers", KPI_TYPE::Gauge, "total reads/writes in Kbyte" },
    { "disks_bsize", KPI_TYPE::Gauge, "total I/Os in Kbyte" },
};

static const prometheus_kpi_descriptor prometheus_kpi_network[] = {
    // baremetal : network
    { "network_interfaces_ibytes", KPI_TYPE::Gauge, "total number of bytes of data received by the interface" },
    { "network_interfaces_ipackets", KPI_TYPE::Gauge, "total number of packets of data received by the interface" },
    { "network_interfaces_ierrs", KPI_TYPE::Gauge, "total number of receive errors detected by the device driver" },
    { "network_interfaces_idrop", KPI_TYPE::Gauge, "total number of packets dropped by the device driver" },
    { "network_interfaces_ififo", KPI_TYPE::Gauge, "number of FIFO buffer errors" },
    { "network_interfaces_iframe", KPI_TYPE::Gauge, "number of packet framing errors" },
    { "network_interfaces_obytes", KPI_TYPE::Gauge, "total number of bytes of data transmitted by the interface" },
    { "network_interfaces_opackets", KPI_TYPE::Gauge,
        "The total number of packets of data transmitted by the interface" },
    { "network_interfaces_oerrs", KPI_TYPE::Gauge, "total number of transmitted errors detected by the device driver" },
    { "network_interfaces_odrop", KPI_TYPE::Gauge, "total number of packets dropped by the interface" },
    { "network_interfaces_ofifo", KPI_TYPE::Gauge, "total number of FIFO buffer errors" },
    { "network_interfaces_ocolls", KPI_TYPE::Gauge, "number of collisions detected on the interface" },
    { "network_interfaces_ocarrier", KPI_TYPE::Gauge, "number of carrier losses detected by the device driver" },
};

static const prometheus_kpi_descriptor prometheus_kpi_cpu[] = {
    // baremetal : cpu
    { "stat_user", KPI_TYPE::Gauge, "time spent in user mode" },
    { "stat_nice", KPI_TYPE::Gauge, "Time spent in user mode with low priority (nice)" },
    { "stat_sys", KPI_TYPE::Gauge, "Time spent in system mode" },
    { "stat_idle", KPI_TYPE::Gauge, "Time spent in the idle task" },
    { "stat_iowait", KPI_TYPE::Gauge, "Time waiting for I/O to complete" },
    { "stat_hardirq", KPI_TYPE::Gauge, "Time servicing interrupts" },
    { "stat_softirq", KPI_TYPE::Gauge, "Time servicing softirqs" },
    { "stat_steal", KPI_TYPE::Gauge,
        "Stolen time, which is the time spent in other operating systems when running in a virtualized environment " },
    { "stat_guest", KPI_TYPE::Gauge, "Time spent running a virtual CPU for guest operating systems" },
    { "stat_guestnice", KPI_TYPE::Gauge, "Time spent running a niced guest (virtual CPU for guest operating systems" },
};

typedef std::map<std::string /* interface name */, std::string /* address */> netdevices_map_t;

typedef struct {
    uint64_t if_ibytes;
    uint64_t if_ipackets;
    uint64_t if_ierrs;
    uint64_t if_idrop;
    uint64_t if_ififo;
    uint64_t if_iframe;
    uint64_t if_obytes;
    uint64_t if_opackets;
    uint64_t if_oerrs;
    uint64_t if_odrop;
    uint64_t if_ofifo;
    uint64_t if_ocolls;
    uint64_t if_ocarrier;
} netinfo_t;

typedef std::map<std::string /* interface name */, netinfo_t /* stats */> netinfo_map_t;

/*
 * Structure to store CPU usage specs as reported by Linux kernel
 * NOTE: all fields specify amount of time, measured in units of USER_HZ
         (1/100ths of a second on most architectures); this means that if the
         _delta_ CPU value reported is 60 in mode X, then that mode took 60% of the CPU!
         IOW there is no need to do any math to produce a percentage, just taking
         the delta of the absolute, monotonic-increasing value and divide by the time
*/
typedef struct cpu_specs_s {
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
} cpu_specs_t;

#define MAX_LOGICAL_CPU (256)

// please refer https://www.kernel.org/doc/Documentation/iostats.txt

typedef struct {
    long dk_major;
    long dk_minor;
    char dk_name[128];

    // reads
    long long dk_reads; // Field 1: This is the total number of reads completed successfully.
    long long dk_rmerge; // Field 2: Reads and writes which are adjacent to each other may be merged for efficiency.
    long long dk_rkb; // Field 3: This is the total number of Kbytes read successfully. [converted by us from sectors]
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
} diskinfo_t;

typedef std::map<std::string /* disk name */, diskinfo_t> diskinfo_map_t;

//------------------------------------------------------------------------------
// CMonitorSystem
//------------------------------------------------------------------------------

class CMonitorSystem : public CMonitorAppHelper {
public:
    CMonitorSystem(CMonitorCollectorAppConfig* pCfg, CMonitorOutputFrontend* pOutput)
        : CMonitorAppHelper(pCfg, pOutput)
    {
        memset(&m_cpu_stat_prev_values[0], 0, MAX_LOGICAL_CPU * sizeof(cpu_specs_t));
    }

    void init();
    void set_monitored_cpus(const std::set<uint64_t>& cpus) { m_monitored_cpus = cpus; }
    void get_list_monitored_files(std::set<std::string>& list);

    //------------------------------------------------------------------------------
    // Functions to collect /proc stats (baremetal), invoked by main app
    //------------------------------------------------------------------------------

    void sample_loadavg();
    void sample_uptime();
    void sample_cpu_stat(double elapsed, OutputFields output_opts);
    void sample_memory(const std::set<std::string>& allowedStatsNames);
    void sample_net_dev(double elapsed, OutputFields output_opts);
    void sample_diskstats(double elapsed, OutputFields output_opts);
    void sample_filesystems();

    //------------------------------------------------------------------------------
    // Utilities shared with CMonitorCgroups
    //------------------------------------------------------------------------------

    static unsigned int get_all_cpus(std::set<uint64_t>& cpu_indexes, const std::string& stat_file = "/proc/stat");

    static bool get_net_dev_list(netdevices_map_t& out_map, bool include_only_interfaces_up);
    static bool read_net_dev_stats(
        const std::string& filename, const std::set<std::string>& net_iface_whitelist, netinfo_map_t& out_infos);
    static bool output_net_dev_stats(CMonitorOutputFrontend* pOutput, double elapsed_sec,
        const netinfo_map_t& new_stats, const netinfo_map_t& prev_stats, OutputFields output_opts);

    //------------------------------------------------------------------------------
    // Utilities shared with CMonitorHeaderInfo
    //------------------------------------------------------------------------------

    static bool output_meminfo_stats(CMonitorOutputFrontend* pOutput, const std::set<std::string>& allowedStatsNames)
    {
        FastFileReader tmp_reader("/proc/meminfo");
        numeric_parser_stats_t dummy;
        return read_meminfo_stats(tmp_reader, allowedStatsNames, pOutput, dummy);
    }

private:
    bool is_monitored_cpu(int cpu)
    {
        if (m_monitored_cpus.empty())
            return true; // allowed
        return m_monitored_cpus.find(cpu) != m_monitored_cpus.end();
    }

    int proc_stat_cpu_index(const char* cpu_data, cpu_specs_t* cpu_values_out);
    // void proc_stat_cpu_total(const char* cpu_data, double elapsed_sec, OutputFields output_opts, cpu_specs_t&
    // total_cpu,
    //    int max_cpu_count); // utility of proc_stat()

    static bool read_meminfo_stats(FastFileReader& reader, const std::set<std::string>& allowedStatsNames,
        CMonitorOutputFrontend* pOutput, numeric_parser_stats_t& out_stats);

private:
    std::set<uint64_t> m_monitored_cpus;

    // last-sampled CPU stats:
    FastFileReader m_cpu_stat;
    long long m_cpu_stat_old_ctxt = 0;
    long long m_cpu_stat_old_processes = 0;
    cpu_specs_t m_cpu_stat_prev_values[MAX_LOGICAL_CPU] = {};
    int m_cpu_count = 0;

    // memory stats
    FastFileReader m_meminfo;
    FastFileReader m_vmstat;

    // disk stats
    FastFileReader m_disk_stat;
    std::set<std::string> m_disks;
    diskinfo_map_t m_previous_diskinfo;

    // network stats
    std::set<std::string> m_network_interfaces_up;
    netinfo_map_t m_previous_netinfo;

    // uptime
    FastFileReader m_uptime;

    // loadavg
    FastFileReader m_loadavg;
};
