/*
 * cgroups_cpuacct.cpp -- code for collecting CGROUP CPUACCT statistics
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
#include "prometheus.h"
#include "utils_files.h"
#include "utils_string.h"
#include <assert.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

// ----------------------------------------------------------------------------------
// CMonitorCgroups - private helpers
// ----------------------------------------------------------------------------------

bool CMonitorCgroups::read_cpuset_cpus(std::string kernelPath, std::set<uint64_t>& cpus)
{
    std::set<uint64_t> empty_set;
    return read_integers_with_range_validation(kernelPath + "/cpuset.cpus", 0, INT32_MAX, cpus);
}

bool CMonitorCgroups::read_cpuacct_line(FastFileReader& reader, std::vector<uint64_t>& valuesINT /* OUT */)
{
    // CMonitorLogger::instance()->LogDebug("reading %s", reader.get_file().c_str());

    if (!reader.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", reader.get_file().c_str());
        return false;
    }

    std::string line = reader.get_next_line();
    std::vector<std::string> values = split_string_in_array(line, ' ');
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

bool CMonitorCgroups::sample_cpuacct_v1_counters_by_cpu(
    bool print, double elapsed_sec, cpuacct_utilisation_t& total_cpu_usage)
{
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

    bool bValidData = true;
    if (m_cgroup_cpuacct_v1_supports_split_user_and_system_time) {

        // this system supports per-cpu system/user stats:

        std::vector<uint64_t> counter_nsec_sys_mode, counter_nsec_user_mode;
        if (!read_cpuacct_line(m_cgroup_cpuacct_v1_reader_sys_stat, counter_nsec_sys_mode))
            bValidData = false;
        if (!read_cpuacct_line(m_cgroup_cpuacct_v1_reader_user_stat, counter_nsec_user_mode))
            bValidData = false;

        if (counter_nsec_sys_mode.size() != counter_nsec_user_mode.size() || counter_nsec_sys_mode.empty())
            bValidData = false;

        if (bValidData) {
            CMonitorLogger::instance()->LogDebug("Found cpuacct.usage_percpu_sys/user cgroups; computing CPU usage "
                                                 "for %.2fsec delta time and %zu CPUs (print=%d)\n",
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
                if (is_allowed_cpu(i) && print && elapsed_sec > MIN_ELAPSED_SECS) {
                    double cpuUserPercent = // force newline
                        100 * ((double)(counter_nsec_user_mode[i] - m_cpuacct_prev_values[i].counter_nsec_user_mode))
                        / (elapsed_sec * 1E9);
                    double cpuSysPercent = // force newline
                        100 * ((double)(counter_nsec_sys_mode[i] - m_cpuacct_prev_values[i].counter_nsec_sys_mode))
                        / (elapsed_sec * 1E9);

                    // output JSON counter
                    m_pOutput->psubsection_start(fmt::format("cpu{}", i).c_str());
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
        // just get the per-cpu total (system+user space):

        std::vector<uint64_t> counter_nsec_user_mode;
        if (!read_cpuacct_line(m_cgroup_cpuacct_v1_reader_combined_stat, counter_nsec_user_mode))
            bValidData = false;
        if (counter_nsec_user_mode.empty())
            bValidData = false;

        if (bValidData) {
            CMonitorLogger::instance()->LogDebug("Found data from cgroup cpuacct.usage_percpu");

            for (size_t i = 0; i < counter_nsec_user_mode.size(); i++) {

                /*
                 * Same comments for USER/SYS computations done above apply here!
                 */
                if (is_allowed_cpu(i) && print && elapsed_sec > MIN_ELAPSED_SECS) {
                    double cpuUserPercent = // force newline
                        100 * ((double)(counter_nsec_user_mode[i] - m_cpuacct_prev_values[i].counter_nsec_user_mode))
                        / (elapsed_sec * 1E9);

                    // output JSON counter
                    m_pOutput->psubsection_start(fmt::format("cpu{}", i).c_str());
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

    // Provide (DELTA OF) throttling statistics; in cgroups v1 these are:
    //   nr_periods <num>
    //   nr_throttled <num>
    //   throttled_time <num_in_nanosecs>
    // Interesting reads:
    //   https://medium.com/indeed-engineering/unthrottled-fixing-cpu-limits-in-the-cloud-a0995ede8e89

    if (m_cgroup_cpuacct_v1_reader_total_cpu_stat.open_or_rewind()) {
        if (bValidData) {
            const char* pline = m_cgroup_cpuacct_v1_reader_total_cpu_stat.get_next_line();
            char label[512];
            uint64_t value;
            cpuacct_throttling_t counter_throttling;
            while (pline) {
                sscanf(pline, "%s %lu", label, &value);
                if (strcmp(label, "nr_periods") == 0)
                    counter_throttling.nr_periods = value;
                else if (strcmp(label, "nr_throttled") == 0)
                    counter_throttling.nr_throttled = value;
                else if (strcmp(label, "throttled_time") == 0)
                    counter_throttling.throttled_time_nsec = value;
                pline = m_cgroup_cpuacct_v1_reader_total_cpu_stat.get_next_line();
            }

            if (print) {
                m_pOutput->psubsection_start("throttling");
                m_pOutput->plong(
                    "nr_periods", counter_throttling.nr_periods - m_cpuacct_prev_values_for_throttling.nr_periods);
                m_pOutput->plong("nr_throttled",
                    counter_throttling.nr_throttled - m_cpuacct_prev_values_for_throttling.nr_throttled);
                m_pOutput->plong("throttled_time",
                    counter_throttling.throttled_time_nsec - m_cpuacct_prev_values_for_throttling.throttled_time_nsec);
                m_pOutput->psubsection_end();
            }

            // save for next cycle
            m_cpuacct_prev_values_for_throttling = counter_throttling;
        }
    } else {
        CMonitorLogger::instance()->LogError(
            "failed to open %s", m_cgroup_cpuacct_v1_reader_total_cpu_stat.get_file().c_str());
    }

    return bValidData;
}

bool CMonitorCgroups::sample_cpuacct_v2_counters(bool print, double elapsed_sec, cpuacct_utilisation_t& total_cpu_usage)
{
    // see https://www.kernel.org/doc/Documentation/cgroup-v2.txt

    if (!m_cgroup_cpuacct_v2_reader_total_cpu_stat.open_or_rewind()) {
        CMonitorLogger::instance()->LogError(
            "failed to open %s", m_cgroup_cpuacct_v2_reader_total_cpu_stat.get_file().c_str());
        return false;
    }

    unsigned int nFoundCpuUsageValues = 0;
    std::string label;
    uint64_t value;
    cpuacct_throttling_t counter_throttling = { 0 };
    const char* pline = m_cgroup_cpuacct_v2_reader_total_cpu_stat.get_next_line();
    while (pline) {

        std::string line(pline);
        if (line.back() == '\n')
            line.pop_back();

        if (split_label_value(line, ' ', label, value)) {
            // save for later any info about CPU usage...
            if (label == "usage_usec") {
                // skip this, it's obtained as user_usec+system_usec
            } else if (label == "user_usec") {
                total_cpu_usage.counter_nsec_user_mode = value * 1000;
                nFoundCpuUsageValues++;
            } else if (label == "system_usec") {
                total_cpu_usage.counter_nsec_sys_mode = value * 1000;
                nFoundCpuUsageValues++;
            } else if (label == "nr_periods") {
                counter_throttling.nr_periods = value;
            } else if (label == "nr_throttled") {
                counter_throttling.nr_throttled = value;
            } else if (label == "throttled_usec") {
                counter_throttling.throttled_time_nsec = value * 1000;
            }
        }

        pline = m_cgroup_cpuacct_v2_reader_total_cpu_stat.get_next_line();
    }

    if (print) {
        m_pOutput->psubsection_start("throttling");
        m_pOutput->plong("nr_periods", counter_throttling.nr_periods - m_cpuacct_prev_values_for_throttling.nr_periods);
        m_pOutput->plong(
            "nr_throttled", counter_throttling.nr_throttled - m_cpuacct_prev_values_for_throttling.nr_throttled);
        m_pOutput->plong("throttled_time",
            counter_throttling.throttled_time_nsec - m_cpuacct_prev_values_for_throttling.throttled_time_nsec);
        m_pOutput->psubsection_end();
    }

    // save for next cycle
    m_cpuacct_prev_values_for_throttling = counter_throttling;

    return nFoundCpuUsageValues == 2;
}

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

bool CMonitorCgroups::is_allowed_cpu(int cpu)
{
    if (m_nCGroupsFound == CG_NONE)
        return true; // allowed
    return m_cgroup_cpus.find(cpu) != m_cgroup_cpus.end();
}

void CMonitorCgroups::init_cpuacct(const std::string& cgroup_prefix_for_test)
{
    // when unit testing, we ask the FastFileReader to actually be not-so-fast and reopen each time the file;
    // that's because during unit testing the actual inode of the statistic file changes on every sample.
    // Of course this does not happen in normal mode
    bool reopen_each_time = !cgroup_prefix_for_test.empty();

    std::string main_file;
    bool main_file_opened = false;
    switch (m_nCGroupsFound) {
    case CG_VERSION1: {
        std::string cgroup_stat_file = m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_sys";
        if (file_or_dir_exists(cgroup_stat_file.c_str())) {
            // current kernel supports reporting in 2 different counters system and user CPU usage
            m_cgroup_cpuacct_v1_supports_split_user_and_system_time = true;
            m_cgroup_cpuacct_v1_reader_sys_stat.set_file(
                m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_sys", reopen_each_time);
            m_cgroup_cpuacct_v1_reader_user_stat.set_file(
                m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_user", reopen_each_time);
        } else {
            // current kernel reports combined (user+system) per-CPU usage
            m_cgroup_cpuacct_v1_reader_combined_stat.set_file(
                m_cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu", reopen_each_time);
        }

        m_cgroup_cpuacct_v1_reader_total_cpu_stat.set_file(
            m_cgroup_cpuacct_kernel_path + "/cpu.stat", reopen_each_time);
        main_file_opened = m_cgroup_cpuacct_v1_reader_total_cpu_stat.open_or_rewind();
        main_file = m_cgroup_cpuacct_v1_reader_total_cpu_stat.get_file();
    } break;

    case CG_VERSION2:
        m_cgroup_cpuacct_v2_reader_total_cpu_stat.set_file(
            m_cgroup_cpuacct_kernel_path + "/cpu.stat", reopen_each_time);
        main_file_opened = m_cgroup_cpuacct_v2_reader_total_cpu_stat.open_or_rewind();
        main_file = m_cgroup_cpuacct_v2_reader_total_cpu_stat.get_file();
        break;

    case CG_NONE:
        assert(0);
        return;
    }

    if (!main_file_opened) {
        m_pCfg->m_nCollectFlags &= ~PK_CGROUP_CPU_ACCT;
        CMonitorLogger::instance()->LogError(
            "Could not read the CPU statistics file '%s'. Disabling monitoring of cpuacct cgroup.\n",
            main_file.c_str());
        return;
    }
    CMonitorLogger::instance()->LogDebug("Successfully initialized cpuacct cgroup monitoring.\n");
}

void CMonitorCgroups::sample_cpuacct(double elapsed_sec)
{
    if (m_nCGroupsFound == CG_NONE)
        return;
    if ((m_pCfg->m_nCollectFlags & PK_CGROUP_CPU_ACCT) == 0)
        return;

    DEBUGLOG_FUNCTION_START();

    bool print = (m_num_cpuacct_samples_collected > 0);
    m_num_cpuacct_samples_collected++;

    if (print)
        m_pOutput->psection_start("cgroup_cpuacct_stats");

    cpuacct_utilisation_t total_cpu_usage = { 0 };
    bool bValidData = false;
    switch (m_nCGroupsFound) {
    case CG_VERSION1:
        // emit per-CPU information since cgroups v1 provide such granularity
        // from these, we also compute the TOTAL cpu usages
        bValidData = sample_cpuacct_v1_counters_by_cpu(print, elapsed_sec, total_cpu_usage);
        break;

    case CG_VERSION2:
        // with cgroups v2, there is no more per-cpu usage reported, we just have
        // the total aggregated cpu usage break down in kernel/user space
        bValidData = sample_cpuacct_v2_counters(print, elapsed_sec, total_cpu_usage);
        break;

    case CG_NONE:
        assert(0);
        return;
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
    }

    if (print)
        m_pOutput->psection_end();
}
