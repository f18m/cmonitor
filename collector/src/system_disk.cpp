/*
 * system_disk.cpp - code for collecting SYSTEM-level disk-statistics (i.e. not cgroup-aware)
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

/*
read /proc/diskstats
*/
void CMonitorSystem::sample_diskstats(double elapsed_sec, OutputFields output_opts)
{
    static bool first_time = true;
    int dk_stats;

    if ((m_pCfg->m_nCollectFlags & PK_BAREMETAL_DISK) == 0)
        return;

    if (!m_pCfg->m_strPrometheusPort.empty() && !m_pCfg->m_strPrometheusAddress.empty()) {
        for (size_t i = 0; i < sizeof(prometheus_kpi_disk) / sizeof(prometheus_kpi_disk[0]); i++) {
            m_pOutput->init_prometheus_kpi(prometheus_kpi_disk[i]);
        }
    }

    DEBUGLOG_FUNCTION_START();

    if (first_time) {
        /* popen variables */
        FILE* pop;
        char tmpstr[1024 + 1];
        long i;
        long j;
        long len;

        pop = popen("lsblk --nodeps --output NAME,TYPE --raw 2>/dev/null", "r");
        if (pop != NULL) {
            /* throw away the headerline */
            if (fgets(tmpstr, 70, pop)) {
                for (i = 0;; i++) {
                    tmpstr[0] = 0;
                    if (fgets(tmpstr, 70, pop) == NULL)
                        break;
                    tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
                    len = strlen(tmpstr);
                    for (j = 0; j < len; j++)
                        if (tmpstr[j] == ' ')
                            tmpstr[j] = 0;

                    if (strncmp(tmpstr, "loop", 4) != 0) {
                        // CMonitorLogger::instance()->LogDebug("DEBUG saved %ld %s disk name\n", i,
                        // previous[i].dk_name);
                        m_disks.insert(tmpstr);
                    } else {
                        CMonitorLogger::instance()->LogDebug("Discarding disk %s\n", tmpstr);
                        /* loop**** disks are not real */
                    }
                }
            }
            pclose(pop);
        }

        CMonitorLogger::instance()->LogDebug("Found %zu disks to monitor\n", m_disks.size());
        first_time = false;
    }

    if (!m_disk_stat.open_or_rewind()) {
        CMonitorLogger::instance()->LogError("failed to re-open %s", m_disk_stat.get_file().c_str());
        return;
    }

    // FIXME: break in 2 parts the parsing of the stat file and the output of measurements just like done for CPU and
    // net stats

    if (output_opts != PF_NONE)
        m_pOutput->psection_start("disks");

    diskinfo_t current;
    const char* buf = m_disk_stat.get_next_line();
    while (buf) {
        // char* pbuf = const_cast<char*>(buf);
        // pbuf[strlen(buf) - 1] = 0; /* remove newline */ // unnecessary???

        // CMonitorLogger::instance()->LogDebug("DISKSTATS: \"%s\"", buf);

        /* zero the data ready for reading */
        bzero(&current, sizeof(diskinfo_t));

        // try to read all the 14 fields
        dk_stats = sscanf(buf, "%ld %ld %s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", &current.dk_major,
            &current.dk_minor, // force newline
            &current.dk_name[0], // force newline
            &current.dk_reads, &current.dk_rmerge, &current.dk_rkb, &current.dk_rmsec, // force newline
            &current.dk_writes, &current.dk_wmerge, &current.dk_wkb, &current.dk_wmsec, // force newline
            &current.dk_inflight, &current.dk_time, &current.dk_backlog);

        if (dk_stats == 7) {
            /* shuffle the data around due to missing columns for partitions */
            current.dk_wkb = current.dk_rmsec;
            current.dk_writes = current.dk_rkb;
            current.dk_rkb = current.dk_rmerge;
            current.dk_rmsec = 0;
            current.dk_rmerge = 0;
        } else if (dk_stats != 14)
            CMonitorLogger::instance()->LogError("disk sscanf wanted 14 but returned=%d line=%s\n", dk_stats, buf);

        current.dk_rkb /= 2; /* convert from sectors to Kbyte, keeping in mind that 1 sector = 512 bytes = 1/2 Kbyte */
        current.dk_wkb /= 2;
        current.dk_xfers = current.dk_reads + current.dk_writes;
        if (current.dk_xfers == 0)
            current.dk_bsize = 0;
        else
            current.dk_bsize = ((current.dk_rkb + current.dk_wkb) / current.dk_xfers) * 1024;

        // f18m: not really sure this is correct... assumes that this field is updated 10 times per second
        current.dk_time /= 10.0; /* in milli-seconds to make it up to 100%, 1000/100 = 10 */

        const auto it_prev = m_previous_diskinfo.find(current.dk_name);
        if (it_prev != m_previous_diskinfo.end()) {
            const diskinfo_t& previous = it_prev->second;

            if (output_opts != PF_NONE) {
                m_pOutput->psubsection_start(current.dk_name);

#define DELTA_DISK_STAT(member) ((double)(current.member - previous.member) / elapsed_sec)

                switch (output_opts) {
                case PF_NONE:
                    assert(0);
                    break;

                case PF_ALL:
                    m_pOutput->pdouble("reads", DELTA_DISK_STAT(dk_reads));
                    m_pOutput->pdouble("rmerge", DELTA_DISK_STAT(dk_rmerge));
                    m_pOutput->pdouble("rkb", DELTA_DISK_STAT(dk_rkb));
                    m_pOutput->pdouble("rmsec", DELTA_DISK_STAT(dk_rmsec));

                    m_pOutput->pdouble("writes", DELTA_DISK_STAT(dk_writes));
                    m_pOutput->pdouble("wmerge", DELTA_DISK_STAT(dk_wmerge));
                    m_pOutput->pdouble("wkb", DELTA_DISK_STAT(dk_wkb));
                    m_pOutput->pdouble("wmsec", DELTA_DISK_STAT(dk_wmsec));

                    m_pOutput->plong("inflight", current.dk_inflight);
                    m_pOutput->pdouble("time", DELTA_DISK_STAT(dk_time));
                    m_pOutput->pdouble("backlog", DELTA_DISK_STAT(dk_backlog));
                    m_pOutput->pdouble("xfers", DELTA_DISK_STAT(dk_xfers));
                    m_pOutput->plong("bsize", current.dk_bsize);
                    break;

                case PF_USED_BY_CHART_SCRIPT_ONLY:
                    m_pOutput->pdouble("rkb", DELTA_DISK_STAT(dk_rkb));
                    m_pOutput->pdouble("wkb", DELTA_DISK_STAT(dk_wkb));
                    break;
                }

                m_pOutput->psubsection_end();
            }
        }

        m_previous_diskinfo[current.dk_name] = current;
        buf = m_disk_stat.get_next_line();
    }
    if (output_opts != PF_NONE)
        m_pOutput->psection_end();
}