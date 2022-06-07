/*
 * system.cpp - code for collecting SYSTEM-level statistics (i.e. not cgroup-aware)
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

#include "system.h"
#include "logger.h"
#include "output_frontend.h"
#include <mntent.h>
#include <sys/vfs.h>

// ----------------------------------------------------------------------------------
// CMonitorSystem
// ----------------------------------------------------------------------------------

void CMonitorSystem::init()
{
    m_cpu_stat.set_file("/proc/stat");
    m_disk_stat.set_file("/proc/diskstats");
    m_uptime.set_file("/proc/uptime");
    m_loadavg.set_file("/proc/loadavg");
    m_meminfo.set_file("/proc/meminfo");
    m_vmstat.set_file("/proc/vmstat");

#ifdef PROMETHEUS_SUPPORT
    if (m_pOutput->is_prometheus_enabled() && (!(m_pCfg->m_nCollectFlags & PK_BAREMETAL_CPU) == 0)) {
        size_t size = sizeof(g_prometheus_kpi_cpu) / sizeof(g_prometheus_kpi_cpu[0]);
        m_pOutput->init_prometheus_kpi(g_prometheus_kpi_cpu, size);
    }

    if (m_pOutput->is_prometheus_enabled() && (!(m_pCfg->m_nCollectFlags & PK_BAREMETAL_DISK) == 0)) {
        size_t size = sizeof(g_prometheus_kpi_disk) / sizeof(g_prometheus_kpi_disk[0]);
        m_pOutput->init_prometheus_kpi(g_prometheus_kpi_disk, size);
    }

    if (m_pOutput->is_prometheus_enabled() && (!(m_pCfg->m_nCollectFlags & PK_BAREMETAL_MEMORY) == 0)) {
        size_t size = sizeof(g_prometheus_kpi_proc_meminfo) / sizeof(g_prometheus_kpi_proc_meminfo[0]);
        m_pOutput->init_prometheus_kpi(g_prometheus_kpi_proc_meminfo, size);
    }

    if (m_pOutput->is_prometheus_enabled() && (!(m_pCfg->m_nCollectFlags & PK_BAREMETAL_NETWORK) == 0)) {
        size_t size = sizeof(g_prometheus_kpi_network) / sizeof(g_prometheus_kpi_network[0]);
        m_pOutput->init_prometheus_kpi(g_prometheus_kpi_network, size);
    }
#endif
}

/*
 read /proc/uptime
*/
void CMonitorSystem::sample_uptime()
{
    DEBUGLOG_FUNCTION_START();

    if (!m_uptime.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_uptime.get_file().c_str());
        return;
    }

    const char* pline = m_uptime.get_next_line();
    if (!pline)
        return;

    long long value;
    long long days;
    long long hours;
    if (sscanf(pline, "%lld", &value) == 1) {
        days = value / 60 / 60 / 24;
        hours = (value - (days * 60 * 60 * 24)) / 60 / 60;

        m_pOutput->psection_start("proc_uptime");
        m_pOutput->plong("total_seconds", value);
        m_pOutput->plong("days", days);
        m_pOutput->plong("hours", hours);
        m_pOutput->psection_end();
    }
}

void CMonitorSystem::sample_loadavg()
{
    DEBUGLOG_FUNCTION_START();

    if (!m_loadavg.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_loadavg.get_file().c_str());
        return;
    }

    const char* pline = m_loadavg.get_next_line();
    if (!pline)
        return;

    /*
            /proc/loadavg
            The first three fields in this file are load average figures giving
            the  number  of jobs in the run queue (state R) or waiting for disk
            I/O (state D) averaged over 1, 5, and 15  minutes.
            They are the same as the load average numbers given by
            uptime(1) and other programs.  The fourth field consists of
            two numbers separated by a slash (/).  The first of these is
            the number of currently runnable kernel scheduling entities
            (processes, threads).  The value after the slash is the number
            of kernel scheduling entities that currently exist on the sys‐
            tem.  The fifth field is the PID of the process that was most
            recently created on the system.
     */

    float load_avg_1min;
    float load_avg_5min;
    float load_avg_15min;
    if (sscanf(pline, "%f %f %f", &load_avg_1min, &load_avg_5min, &load_avg_15min) == 3) {
        m_pOutput->psection_start("proc_loadavg");
        m_pOutput->pdouble("load_avg_1min", load_avg_1min);
        m_pOutput->pdouble("load_avg_5min", load_avg_5min);
        m_pOutput->pdouble("load_avg_15min", load_avg_15min);
        m_pOutput->psection_end();
    }
}

void CMonitorSystem::sample_filesystems()
{
    FILE* fp;
    struct mntent* fs;
    struct statfs vfs;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_DISK) == 0)
        return;

    DEBUGLOG_FUNCTION_START();
    if ((fp = setmntent("/etc/mtab", "r")) == NULL)
        CMonitorLogger::instance()->LogError("setmntent(\"/etc/mtab\", \"r\") failed");

    m_pOutput->psection_start("filesystems");
    while ((fs = getmntent(fp)) != NULL) {
        // NOTE: /dev/loop* filesystems are not real filesystems - e.g. on Ubuntu they are used for SNAPs
        if (fs->mnt_fsname[0] == '/' && strncmp(fs->mnt_fsname, "/dev/loop", 9) != 0) {
            if (statfs(fs->mnt_dir, &vfs) != 0) {
                CMonitorLogger::instance()->LogErrorWithErrno("%s: statfs failed: %d\n", fs->mnt_dir, errno);
            }
            // CMonitorLogger::instance()->LogDebug("%s, mounted on %s:\n", fs->mnt_dir, fs->mnt_fsname);

            m_pOutput->psubsection_start(fs->mnt_fsname);
            m_pOutput->pstring("fs_dir", fs->mnt_dir);
            m_pOutput->pstring("fs_type", fs->mnt_type);
            m_pOutput->pstring("fs_opts", fs->mnt_opts);

            m_pOutput->plong("fs_freqs", fs->mnt_freq);
            m_pOutput->plong("fs_passno", fs->mnt_passno);
            m_pOutput->plong("fs_bsize", vfs.f_bsize);
            m_pOutput->plong("fs_size_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->plong("fs_free_mb", (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->plong(
                "fs_used_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024 - (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->pdouble(
                "fs_full_percent", ((double)vfs.f_blocks - (double)vfs.f_bfree) / (double)vfs.f_blocks * (double)100.0);
            /*
             * m_pOutput->pdouble("fs_full_percent", ((vfs.f_blocks * vfs.f_bsize) - (vfs.f_bfree * vfs.f_bsize) ) /
             *					(vfs.f_blocks * vfs.f_bsize) * 100.00);
             */
            m_pOutput->plong("fs_avail", (vfs.f_bavail * vfs.f_bsize) / 1024 / 1024);
            m_pOutput->plong("fs_files", vfs.f_files);
            m_pOutput->plong("fs_files_free", vfs.f_ffree);
            m_pOutput->plong("fs_namelength", vfs.f_namelen);
            m_pOutput->psubsection_end();
        }
    }
    m_pOutput->psection_end();
    endmntent(fp);
}

void CMonitorSystem::get_list_monitored_files(std::set<std::string>& list)
{
    list.insert(m_uptime.get_file());
    list.insert(m_loadavg.get_file());
    if (m_pCfg->m_nCollectFlags & PK_BAREMETAL_CPU)
        list.insert(m_cpu_stat.get_file());
    if (m_pCfg->m_nCollectFlags & PK_BAREMETAL_MEMORY) {
        list.insert(m_meminfo.get_file());
        list.insert(m_vmstat.get_file());
    }
    if (m_pCfg->m_nCollectFlags & PK_BAREMETAL_DISK)
        list.insert(m_disk_stat.get_file());
}
