/*
 * system_cpu.cpp - code for collecting SYSTEM-level cpu-statistics (i.e. not cgroup-aware)
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
#include <assert.h>

// ----------------------------------------------------------------------------------
// Macros
// ----------------------------------------------------------------------------------

#define DELTA_TOTAL(stat) ((float)(stat - total_cpu.stat) / (float)elapsed_sec / ((float)(max_cpu_count + 1.0)))

#if 0 // currently unused
void CMonitorSystem::proc_stat_cpu_total(
    const char* cpu_data, double elapsed_sec, OutputFields output_opts, cpu_specs_t& total_cpu, int max_cpu_count)
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

    // see http://man7.org/linux/man-pages/man5/proc.5.html
    // Look for "/proc/stat"

    int count = sscanf(cpu_data, /* cpu USER */
        "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", // force newline
        &user, &nice, &sys, &idle, &iowait, &hardirq, &softirq, &steal, &guest, &guestnice);
    if (count != 10)
        return;

    if (output_opts != PF_NONE) {
        m_pOutput->psubsection_start("cpu_total");
        switch (output_opts) {
        case PF_NONE:
            assert(0);
            break;
        case PF_ALL:
        case PF_USED_BY_CHART_SCRIPT_ONLY:
            m_pOutput->pdouble("user", DELTA_TOTAL(user)); /* incrementing counter */
            m_pOutput->pdouble("nice", DELTA_TOTAL(nice)); /* incrementing counter */
            m_pOutput->pdouble("sys", DELTA_TOTAL(sys)); /* incrementing counter */
            m_pOutput->pdouble("idle", DELTA_TOTAL(idle)); /* incrementing counter */
            /*			m_pOutput->pdouble("DEBUG IDLE idle: %lld %lld %lld\n", total_cpu.idle,
             * idle, idle-total_cpu.idle); */
            m_pOutput->pdouble("iowait", DELTA_TOTAL(iowait)); /* incrementing counter */
            m_pOutput->pdouble("hardirq", DELTA_TOTAL(hardirq)); /* incrementing counter */
            m_pOutput->pdouble("softirq", DELTA_TOTAL(softirq)); /* incrementing counter */
            m_pOutput->pdouble("steal", DELTA_TOTAL(steal)); /* incrementing counter */
            m_pOutput->pdouble("guest", DELTA_TOTAL(guest)); /* incrementing counter */
            m_pOutput->pdouble("guestnice", DELTA_TOTAL(guestnice)); /* incrementing counter */
            break;
        }
        m_pOutput->psubsection_end();
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
}
#endif

int CMonitorSystem::proc_stat_cpu_index(const char* cpu_data, cpu_specs_t* cpu_values_out)
{
    int cpuno;

    // see http://man7.org/linux/man-pages/man5/proc.5.html
    // Look for "/proc/stat"

    /* cpu_data must be a pointer immediately after the 'cpu' chars of:
         cpuNNN ...lots of counters
    */
    int count = sscanf(cpu_data, // force newline
        "%d %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", // force newline
        &cpuno, &cpu_values_out->user, &cpu_values_out->nice, &cpu_values_out->sys, &cpu_values_out->idle,
        &cpu_values_out->iowait, &cpu_values_out->hardirq, &cpu_values_out->softirq, &cpu_values_out->steal,
        &cpu_values_out->guest, &cpu_values_out->guestnice);
    if (count != 11)
        return -1;

    if (cpuno >= MAX_LOGICAL_CPU)
        return -1;
    if (!is_monitored_cpu(cpuno))
        return -1;
    return cpuno;
}

/*
read /proc/stat
*/
void CMonitorSystem::sample_cpu_stat(double elapsed_sec, OutputFields output_opts)
{
    long long new_ctx, btime, new_processes, procs_running, procs_blocked;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_CPU) == 0)
        return;

#ifdef PROMETHEUS_SUPPORT
    if (m_pOutput->is_prometheus_enabled()) {
        size_t size = sizeof(prometheus_kpi_cpu) / sizeof(prometheus_kpi_cpu[0]);
        m_pOutput->init_prometheus_kpi(prometheus_kpi_cpu, size);
    }
#endif

    DEBUGLOG_FUNCTION_START();

    CMonitorLogger::instance()->LogDebug("proc_stat(%.4f) max_cpu_count=%d\n", elapsed_sec, m_cpu_count);
    if (!m_cpu_stat.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_cpu_stat.get_file().c_str());
        return;
    }

    cpu_specs_t new_values[MAX_LOGICAL_CPU];
    cpu_specs_t tmp_values;
    const char* line = m_cpu_stat.get_next_line();
    while (line) {
        if (strncmp(line, "cpu", 3) == 0) {
            if (line[3] == ' ') {
                // found the summary line for ALL cpus together, e.g.:
                //     cpu  265510448 66285 143983783 14772309342 4657946 0 16861124 0 0 0
                // skip it
                line = m_cpu_stat.get_next_line();
                continue;
            } else {
                // found a line for a specific CPU like:
                //    cpu1 90470 3217 30294 291392 17250 0 3242 0 0 0
                // process it
                int cpuno = proc_stat_cpu_index(&line[3], &tmp_values);
                if (cpuno > m_cpu_count)
                    m_cpu_count = cpuno;
                if (cpuno >= 0)
                    new_values[cpuno] = tmp_values;
            }
        } else if (!strncmp(line, "ctxt", 4)) {
            new_ctx = 0;
            sscanf(&line[5], "%lld", &new_ctx); /* counter */
        } else if (!strncmp(line, "btime", 5)) {
            btime = 0;
            sscanf(&line[6], "%lld", &btime); /* seconds since boot */
        } else if (!strncmp(line, "processes", 9)) {
            new_processes = 0;
            sscanf(&line[10], "%lld", &new_processes); /* counter  actually forks */
        } else if (!strncmp(line, "procs_running", 13)) {
            procs_running = 0;
            sscanf(&line[14], "%lld", &procs_running);
        } else if (!strncmp(line, "procs_blocked", 13)) {
            procs_blocked = 0;
            sscanf(&line[14], "%lld", &procs_blocked);
        }

        line = m_cpu_stat.get_next_line();
    }

    if (output_opts != PF_NONE) {
        m_pOutput->psection_start("stat");
        for (int i = 0; i <= m_cpu_count; i++) {
            if (!is_monitored_cpu(i))
                continue;

#define DELTA_CPU_STAT(stat) ((double)(new_values[i].stat - m_cpu_stat_prev_values[i].stat) / elapsed_sec)

            m_pOutput->psubsection_start(fmt::format("cpu{:d}", i).c_str());
            switch (output_opts) {
            case PF_NONE:
                assert(0);
                break;
            case PF_ALL:
            case PF_USED_BY_CHART_SCRIPT_ONLY:
                m_pOutput->pdouble("user", DELTA_CPU_STAT(user)); /* counter */
                m_pOutput->pdouble("nice", DELTA_CPU_STAT(nice)); /* counter */
                m_pOutput->pdouble("sys", DELTA_CPU_STAT(sys)); /* counter */
                m_pOutput->pdouble("idle", DELTA_CPU_STAT(idle)); /* counter */
                m_pOutput->pdouble("iowait", DELTA_CPU_STAT(iowait)); /* counter */
                m_pOutput->pdouble("hardirq", DELTA_CPU_STAT(hardirq)); /* counter */
                m_pOutput->pdouble("softirq", DELTA_CPU_STAT(softirq)); /* counter */
                m_pOutput->pdouble("steal", DELTA_CPU_STAT(steal)); /* counter */
                m_pOutput->pdouble("guest", DELTA_CPU_STAT(guest)); /* counter */
                m_pOutput->pdouble("guestnice", DELTA_CPU_STAT(guestnice)); /* counter */
                break;
            }
            m_pOutput->psubsection_end();
        }

        m_pOutput->psubsection_start("counters");
        m_pOutput->pdouble("ctxt", ((double)(new_ctx - m_cpu_stat_old_ctxt) / elapsed_sec));
        m_pOutput->plong("btime", btime);
        m_pOutput->pdouble("processes_forks", ((double)(new_processes - m_cpu_stat_old_processes) / elapsed_sec));
        m_pOutput->plong("procs_running", procs_running);
        m_pOutput->plong("procs_blocked", procs_blocked);
        m_pOutput->psubsection_end();

        m_pOutput->psection_end();
    }

    m_cpu_stat_old_ctxt = new_ctx;
    m_cpu_stat_old_processes = new_processes;
    for (int i = 0; i <= m_cpu_count; i++)
        m_cpu_stat_prev_values[i] = new_values[i];
}

/* static */
unsigned int CMonitorSystem::get_all_cpus(std::set<uint64_t>& cpu_indexes, const std::string& stat_file)
{
    FastFileReader cpu_stat(stat_file);

    cpu_indexes.clear();

    if (!cpu_stat.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", cpu_stat.get_file().c_str());
        return 0;
    }

    const char* line = cpu_stat.get_next_line();
    while (line) {
        if (strncmp(line, "cpu", 3) == 0) {
            if (line[3] == ' ') {
                // found the summary line for ALL cpus together, e.g.:
                //     cpu  265510448 66285 143983783 14772309342 4657946 0 16861124 0 0 0
                // skip it
                line = cpu_stat.get_next_line();
                continue;
            } else {
                // found a line for a specific CPU like:
                //    cpu1 90470 3217 30294 291392 17250 0 3242 0 0 0
                // process it
                const char* pStart = &line[3];

                /*
                from the manpage:
                Since 0 can legitimately be returned on both success and failure, the calling program should set errno
                to 0 before the call, and then determine if an error occurred by checking whether errno has a nonzero
                value after the call.
                */
                errno = 0;
                unsigned long cpuno = strtoul(pStart, NULL, 10);
                if (errno == 0)
                    cpu_indexes.insert(cpuno);
            }
        }

        line = cpu_stat.get_next_line();
    }

    return cpu_indexes.size();
}