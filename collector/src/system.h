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

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

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

typedef std::map<std::string /* interface name */, netinfo_t> netinfo_map_t;

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

    //------------------------------------------------------------------------------
    // Functions to collect /proc stats (baremetal), invoked by main app
    //------------------------------------------------------------------------------

    void sample_cpu_stat(double elapsed, OutputFields output_opts);
    void sample_diskstats(double elapsed, OutputFields output_opts);
    void sample_net_dev(double elapsed, OutputFields output_opts);
    void sample_loadavg();
    void sample_filesystems();
    void sample_uptime();

    //------------------------------------------------------------------------------
    // Utility shared with CMonitorCgroups
    //------------------------------------------------------------------------------

    static unsigned int get_all_cpus(std::set<uint64_t>& cpu_indexes, const std::string& stat_file = "/proc/stat");

    static bool read_net_dev(
        const std::string& filename, const std::set<std::string>& net_iface_whitelist, netinfo_map_t& out_infos);
    static bool output_net_dev_stats(CMonitorOutputFrontend* m_pOutput, double elapsed_sec,
        const netinfo_map_t& new_stats, const netinfo_map_t& prev_stats, OutputFields output_opts);

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

private:
    std::set<uint64_t> m_monitored_cpus;

    // last-sampled CPU stats:
    FastFileReader m_cpu_stat;
    long long m_cpu_stat_old_ctxt = 0;
    long long m_cpu_stat_old_processes = 0;
    cpu_specs_t m_cpu_stat_prev_values[MAX_LOGICAL_CPU] = {};
    int m_cpu_count = 0;

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
