/*
 * cgroups_processes.cpp -- code for collecting PROCESS/THREAD statistics
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

#define PAGESIZE_BYTES (1024 * 4)

// ----------------------------------------------------------------------------------
// C++ Helper functions
// ----------------------------------------------------------------------------------

uint64_t compute_proc_score(const procsinfo_t* current_stats, const procsinfo_t* prev_stats, double elapsed_secs)
{
    static double ticks_per_sec = (double)sysconf(_SC_CLK_TCK); // clock ticks per second

    // take the total time this process/task/thread has been scheduled in both USER and KERNEL space:
    uint64_t cputime_clock_ticks = 0;
    if (current_stats->pi_utime >= prev_stats->pi_utime && // force newline
        current_stats->pi_stime >= prev_stats->pi_stime) {
        cputime_clock_ticks = // force newline
            (current_stats->pi_utime - prev_stats->pi_utime) + // userspace
            (current_stats->pi_stime - prev_stats->pi_stime); // kernelspace
    }

    // give a score which is linear in both CPU time and virtual memory size:
    // return (cputime_clock_ticks * ticks_per_sec) * current_stats->pi_vsize;

    // prefer a score based only on CPU time:
    // FIXME: it would be nice to have the "score policy" configurable and not hardcoded like that
    return cputime_clock_ticks * ticks_per_sec;
}

/* Lookup the right process state string */
const char* get_state(char n)
{
    static char duff[64];
    switch (n) {
    case 'R':
        return "Running";
    case 'S':
        return "Sleeping-interruptible";
    case 'D':
        return "Waiting-uninterruptible";
    case 'Z':
        return "Zombie";
    case 'T':
        return "Stopped";
    case 't':
        return "Tracing";
    case 'W':
        return "Paging-or-Waking";
    case 'X':
        return "Dead";
    case 'x':
        return "dead";
    case 'K':
        return "Wakekill";
    case 'P':
        return "Parked";
    default:
        snprintf(duff, 64, "State=%d(%c)", n, n);
        return duff;
    }
}

bool CMonitorCgroups::get_process_infos(
    pid_t pid, bool include_threads, procsinfo_t* pout, OutputFields output_opts, bool output_tgid)
{
#define MAX_STAT_FILE_PREFIX_LEN 1000
#define MAX_PROC_FILENAME_LEN (MAX_STAT_FILE_PREFIX_LEN + 64)
#define MAX_PROC_CONTENT_LEN 4096

    FILE* fp = NULL;
    char buf[MAX_PROC_CONTENT_LEN] = { '\0' };

    memset(pout, 0, sizeof(procsinfo_t));

    /* the statistic directory for the process */
    auto pid_dir = fmt::format("{}/proc/{}", m_proc_prefix, pid);
    struct stat statbuf;
    if (stat(pid_dir.c_str(), &statbuf) != 0) {
        // IMPORTANT: cmonitor_collector first reads all PIDs and then invokes, sequentially, this function;
        //            this means that by the time we get here, a PID may has ceased to exist. So do not generate
        //            any error line for this condition and consider it to be just something that can happen.
        // CMonitorLogger::instance()->LogErrorWithErrno("ERROR: failed to stat file %s", pid_dir.c_str());
        return false;
    }

    // by looking at the owner of the directory we know which user is running it:
    pout->uid = statbuf.st_uid;
    struct passwd* pw = getpwuid(statbuf.st_uid);
    if (pw) {
        strncpy(pout->username, pw->pw_name, 63);
        pout->username[63] = 0;
    }

    /*
        ABOUT STATISTIC FILES CONSIDERED IN THIS FUNCTION:
        For multithreaded application it might be tricky to understand /proc file organization.
        Consider a single process with PID=TID=A having 2 secondary threads with TID=B and TID=C.
        The kernel "stat" files layout (but the same applies to all other files considered below
        like "statm", "status" and "io") will look like:
            /proc/A
                  +-- stat            contains statistics about the whole process having PID=TID=A
                  +-- task/A/stat     contains statistics about the main thread (PID=TID=A)
                  +-- task/B/stat     contains statistics about the secondary thread TID=B
                  +-- task/C/stat     contains statistics about the secondary thread TID=C
         So far so good, here comes the tricky part:
            /proc/B                   it exists even if B is just a secondary thread of PID=A
                  +
                  +-- stat            this is the tricky part... you would expect this to contain stats of TID=B
                  +                   but instead it contains statistics about the whole process (PID=TID=A)
                  +-- task/A/stat     contains statistics about the main thread (PID=TID=A)
                  +-- task/B/stat     contains statistics about the secondary thread TID=B
                  +-- task/C/stat     contains statistics about the secondary thread TID=C

         This means that:
         a) WHEN COLLECTING PER-THREAD STATISTICS:
         To make sure we always get statistics for the thread identified by PID=pid, regardless of the fact it's
         the main thread or a secondary one, we always look at /proc/<pid>/task/<pid>/<statistics-file>

         b) WHEN COLLECTING PER-PROCESS STATISTICS:
         To make sure we collect the stats for the whole process identified by PID=pid (and not just its main thread),
         we look at /proc/<pid>/<statistics-file>
    */
    std::string stat_file_prefix;
    if (include_threads)
        stat_file_prefix = fmt::format("{}/proc/{}/task/{}", m_proc_prefix, pid, pid);
    else
        stat_file_prefix = fmt::format("{}/proc/{}", m_proc_prefix, pid);

    { /* process the statistic file for the process/thread */
        std::string filename = stat_file_prefix + "/stat";
        if ((fp = fopen(filename.c_str(), "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("ERROR: failed to open file %s", filename.c_str());
            return false;
        }

        size_t size = fread(buf, 1, MAX_PROC_CONTENT_LEN, fp);
        bool io_error = ferror(fp);
        bool reached_eof = feof(fp);
        fclose(fp); // regardless of what happened, always close the file
        if (size == 0 || size >= MAX_PROC_CONTENT_LEN || io_error) {
            CMonitorLogger::instance()->LogError(
                "ERROR: procsinfo read returned = %zu assuming process stopped pid=%d errno=%d\n", size, pid, errno);
            return false;
        }
        if (!reached_eof) {
            CMonitorLogger::instance()->LogError(
                "ERROR: procsinfo read returned = %zu for pid=%d but did not reach EOF\n", size, pid);
            return false;
        }

        // make sure the buffer is always NUL-terminated
        buf[size - 1] = '\0';

        // read columns (1) and (2):   "pid" and "comm"
        // see http://man7.org/linux/man-pages/man5/proc.5.html, search for /proc/[pid]/stat
        int ret = sscanf(buf, "%d (%s)", &pout->pi_pid, &pout->pi_comm[0]);
        if (ret != 2) {
            CMonitorLogger::instance()->LogError("procsinfo sscanf returned = %d line=%s\n", ret, buf);
            return false;
        }
        pout->pi_comm[strlen(pout->pi_comm) - 1] = 0;

        // never seen a case where inside /proc/<pid>/task/<pid>/stat you find mention of a pid != <pid>
        if (pout->pi_pid != pid) {
            CMonitorLogger::instance()->LogError(
                "ERROR: found pid=%d inside the filename=%s... unexpected mismatch\n", pout->pi_pid, filename.c_str());
            return false;
        }

        /* now look for ") " as dumb Infiniband driver includes "()" */
        size_t count = 0;
        for (count = 0; count < size; count++)
            if (buf[count] == ')' && buf[count + 1] == ' ')
                break;
        if (count >= size - 2) {
            CMonitorLogger::instance()->LogError("procsinfo failed to find end of command buf=%s\n", buf);
            return false;
        }
        count++; // skip ')'
        count++; // skip space after parentheses

        // see http://man7.org/linux/man-pages/man5/proc.5.html, search for /proc/[pid]/stat
        /* column 1 and 2 are handled above */
        long junk;
        ret = sscanf(&buf[count],
            "%c %d %d %d %d %d %lu %lu %lu %lu " /* from 3 to 13 */
            "%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu " /* from 14 to 23 */
            "%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu " /* from 24 to 33 */
            "%lu %lu %lu %lu %lu %d %d %lu %lu %llu", /* from 34 to 42 */
            &pout->pi_state, /*3 - these numbers are taken from "man proc" */
            &pout->pi_ppid, /*4*/
            &pout->pi_pgrp, /*5*/
            &pout->pi_session, /*6*/
            &pout->pi_tty_nr, /*7*/
            &pout->pi_tty_pgrp, /*8*/
            &pout->pi_flags, /*9*/
            &pout->pi_minflt, /*10*/
            &pout->pi_child_min_flt, /*11*/
            &pout->pi_majflt, /*12*/
            &pout->pi_child_maj_flt, /*13*/
            &pout->pi_utime, /*14*/ // CPU time spent in user space
                             &pout->pi_stime,
            /*15*/ // CPU time spent in kernel space
            &pout->pi_child_utime, /*16*/
            &pout->pi_child_stime, /*17*/
            &pout->pi_priority, /*18*/
            &pout->pi_nice, /*19*/
            &pout->pi_num_threads, /*20*/
            &junk, /*21*/
            &pout->pi_start_time, /*22*/
            &pout->pi_vsize, /*23*/
            &pout->pi_rss, /*24*/
            &pout->pi_rsslimit, /*25*/
            &pout->pi_start_code, /*26*/
            &pout->pi_end_code, /*27*/
            &pout->pi_start_stack, /*28*/
            &pout->pi_esp, /*29*/
            &pout->pi_eip, /*30*/
            &pout->pi_signal_pending, /*31*/
            &pout->pi_signal_blocked, /*32*/
            &pout->pi_signal_ignore, /*33*/
            &pout->pi_signal_catch, /*34*/
            &pout->pi_wchan, /*35*/
            &pout->pi_swap_pages, /*36*/
            &pout->pi_child_swap_pages, /*37*/
            &pout->pi_signal_exit, /*38*/
            &pout->pi_last_cpu, /*39*/
            &pout->pi_realtime_priority, /*40*/
            &pout->pi_sched_policy, /*41*/
            &pout->pi_delayacct_blkio_ticks /*42*/
        );
        if (ret != 40) {
            CMonitorLogger::instance()->LogError(
                "procsinfo sscanf wanted 40 returned = %d pid=%d line=%s\n", ret, pid, buf);
            return false;
        }
    }

    if (output_opts == PF_ALL) { /* process the statm file for the process/thread */

        std::string filename = stat_file_prefix + "/statm";
        if ((fp = fopen(filename.c_str(), "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("failed to open file %s", filename.c_str());
            return false;
        }
        size_t size = fread(buf, 1, MAX_PROC_CONTENT_LEN - 1, fp);
        fclose(fp); /* close it even if the read failed, the file could have been removed
                    between open & read i.e. the device driver does not behave like a file */
        if (size == 0) {
            CMonitorLogger::instance()->LogError("failed to read file %s", filename.c_str());
            return false;
        }

        int ret = sscanf(&buf[0], "%lu %lu %lu %lu %lu %lu %lu", // force newline
            &pout->statm_size, &pout->statm_resident, &pout->statm_share, &pout->statm_trs, &pout->statm_lrs,
            &pout->statm_drs, &pout->statm_dt);
        if (ret != 7) {
            CMonitorLogger::instance()->LogError("sscanf wanted 7 returned = %d line=%s\n", ret, buf);
            return false;
        }
    }

    if (output_tgid) { /* process the status file for the process/thread */

        std::string filename = stat_file_prefix + "/status";
        if ((fp = fopen(filename.c_str(), "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("failed to open file %s", filename.c_str());
            return false;
        }
        for (int i = 0;; i++) {
            if (fgets(buf, 1024, fp) == NULL) {
                break;
            }
            if (strncmp("Tgid:", buf, 5) == 0) {
                // this info is only available from the /status file apparently and not from /stat
                // and indicates whether this PID is the main thread (TGID==PID) or a secondary thread (TGID!=PID)
                sscanf(&buf[6], "%d", &pout->pi_tgid);
            }
        }
        fclose(fp);
    }

    { /* process the I/O file for the process/thread */
        pout->io_read_bytes = 0;
        pout->io_write_bytes = 0;

        std::string filename = stat_file_prefix + "/io";
        if ((fp = fopen(filename.c_str(), "r")) == NULL) {
            CMonitorLogger::instance()->LogErrorWithErrno("failed to open file %s", filename.c_str());
            return false;
        }
        for (int i = 0; i < 6; i++) {
            if (fgets(buf, 1024, fp) == NULL) {
                break;
            }
            /*
                from https://man7.org/linux/man-pages/man5/proc.5.html

                rchar: characters read
                        The number of bytes which this task has caused to
                        be read from storage.  This is simply the sum of
                        bytes which this process passed to read(2) and
                        similar system calls.  It includes things such as
                        terminal I/O and is unaffected by whether or not
                        actual physical disk I/O was required (the read
                        might have been satisfied from pagecache).

            */
            if (strncmp("rchar:", buf, 6) == 0)
                sscanf(&buf[7], "%lld", &pout->io_rchar);
            if (strncmp("wchar:", buf, 6) == 0)
                sscanf(&buf[7], "%lld", &pout->io_wchar);
            if (strncmp("read_bytes:", buf, 11) == 0)
                sscanf(&buf[12], "%lld", &pout->io_read_bytes);
            if (strncmp("write_bytes:", buf, 12) == 0)
                sscanf(&buf[13], "%lld", &pout->io_write_bytes);
        }
        fclose(fp);
    }
    return true;
}

bool CMonitorCgroups::collect_pids(const std::string& path, std::vector<pid_t>& pids)
{
    CMonitorLogger::instance()->LogDebug("Trying to read tasks inside the monitored cgroup from %s.\n", path.c_str());
    if (!file_or_dir_exists(path.c_str()))
        return false;

    std::ifstream inputf(path);
    if (!inputf.is_open())
        return false; // cannot read the cgroup information!

    std::string line;
    while (std::getline(inputf, line)) {
        uint64_t pid;
        // this PID is actually a TID (thread ID) most of the time... because in the kernel process/thread
        // distinction is much less strong than userspace: they're all tasks
        if (string2int(line.c_str(), pid))
            pids.push_back((pid_t)pid);
    }

    CMonitorLogger::instance()->LogDebug(
        "Found %zu PIDs/TIDs to monitor: %s.\n", pids.size(), stl_container2string(pids, ",").c_str());

    return true;
}

bool CMonitorCgroups::collect_pids(FastFileReader& reader, std::vector<pid_t>& pids)
{
    if (!reader.open_or_rewind()) {
        CMonitorLogger::instance()->LogDebug("Cannot open file [%s]", reader.get_file().c_str());
        return false;
    }

    const char* pline = reader.get_next_line();
    while (pline) {
        uint64_t pid;
        // this PID is actually a TID (thread ID) most of the time... because in the kernel process/thread
        // distinction is much less strong than userspace: they're all tasks
        if (string2int(pline, pid))
            pids.push_back((pid_t)pid);

        pline = reader.get_next_line();
    }

    CMonitorLogger::instance()->LogDebug("Found %zu PIDs/TIDs to monitor [%s] inside %s.\n", pids.size(),
        stl_container2string(pids, ",").c_str(), reader.get_file().c_str());

    return true;
}

// ----------------------------------------------------------------------------------
// CMonitorCgroups - Functions used by the cmonitor_collector engine
// ----------------------------------------------------------------------------------

void CMonitorCgroups::init_processes(const std::string& cgroup_prefix_for_test)
{
    // when unit testing, we ask the FastFileReader to actually be not-so-fast and reopen each time the file;
    // that's because during unit testing the actual inode of the statistic file changes on every sample.
    // Of course this does not happen in normal mode
    // bool reopen_each_time = !cgroup_prefix_for_test.empty();

    /*
     FIXME FIXME: for some reason we need to reopen the 'tasks' file each time (at least on Centos7 )
     or otherwise we will read always the same contents over and over:
    */
    bool reopen_each_time = true;

    switch (m_nCGroupsFound) {
    case CG_VERSION1:
        // in cgroups v1 all TIDs are available in the cgroup file named "tasks"
        // of course here we're assuming that the "tasks" under the "memory" cgroup are the ones
        // the user is interested to monitor... in theory the "tasks" under other controllers like "cpuacct"
        // might be different; in practice with Docker/LXC/Kube that does not happen
        m_cgroup_processes_reader_pids.set_file(m_cgroup_processes_path + "/tasks", reopen_each_time);
        break;

    case CG_VERSION2:
        // with cgroups v2, there are 2 different files that contain PIDs/TIDs so we can just
        // read the right file up-front based on 'include_threads':
        if (m_cgroup_processes_include_threads)
            m_cgroup_processes_reader_pids.set_file(m_cgroup_processes_path + "/cgroup.threads", reopen_each_time);
        else
            m_cgroup_processes_reader_pids.set_file(m_cgroup_processes_path + "/cgroup.procs", reopen_each_time);
        break;

    case CG_NONE:
        assert(0);
        return;
    }

    if (!m_cgroup_processes_reader_pids.open_or_rewind()) {
        m_pCfg->m_nCollectFlags &= ~PK_CGROUP_PROCESSES;
        m_pCfg->m_nCollectFlags &= ~PK_CGROUP_THREADS;
        CMonitorLogger::instance()->LogError("Could not read the cgroup with list of pids from file '%s'. Disabling "
                                             "monitoring of processes/threads inside cgroup.\n",
            m_cgroup_processes_reader_pids.get_file().c_str());
        return;
    }

    CMonitorLogger::instance()->LogDebug("Successfully initialized cgroup processes monitoring.\n");
}

void CMonitorCgroups::sample_process_list()
{
    if (m_nCGroupsFound == CG_NONE)
        return;

    // this function is shared between
    // * cgroup process stats
    // * cgroup network stats
    // processors; so it must execute if any of the 2 stat collector is enabled
    if ((m_pCfg->m_nCollectFlags & PK_CGROUP_PROCESSES) == 0 && // fn
        (m_pCfg->m_nCollectFlags & PK_CGROUP_THREADS) == 0 && // fn
        (m_pCfg->m_nCollectFlags & PK_CGROUP_NETWORK_INTERFACES) == 0)
        return;

    DEBUGLOG_FUNCTION_START();

    // collect all PIDs for current cgroup
    m_cgroup_all_pids.clear();
    !collect_pids(m_cgroup_processes_reader_pids, m_cgroup_all_pids);
}

void CMonitorCgroups::sample_processes(double elapsed_sec, OutputFields output_opts)
{
    if (m_nCGroupsFound == CG_NONE)
        return;
    if ((m_pCfg->m_nCollectFlags & PK_CGROUP_PROCESSES) == 0 && (m_pCfg->m_nCollectFlags & PK_CGROUP_THREADS) == 0)
        return;

    DEBUGLOG_FUNCTION_START();

    if (m_num_tasks_samples_collected == 0)
        output_opts = PF_NONE; // the first sample is used as bootstrap: we cannot generate any meaningful delta and
                               // thus any meaningful output
    m_num_tasks_samples_collected++;

    // swap databases
    m_pid_database_current_index = !m_pid_database_current_index;
    std::map<pid_t, procsinfo_t>& currDB = m_pid_databases[m_pid_database_current_index];
    std::map<pid_t, procsinfo_t>& prevDB = m_pid_databases[!m_pid_database_current_index];

    // get new fresh processes data and update current database:
    currDB.clear();
    bool needsToFilterOutThreads = (m_nCGroupsFound == CG_VERSION1) && !m_cgroup_processes_include_threads;
    size_t nfailed_sampling = 0, nthreads_discarded = 0;
    for (size_t i = 0; i < m_cgroup_all_pids.size(); i++) {

        // acquire all possible informations on this PID (or TID)
        // NOTE: getting the Tgid is expensive (requires opening a dedicated file) but we want to provide Tgid in output
        //       since it's the only way to provide to the data consumer a realiable criteria to distinguish between
        //       secondary threads and main threads
        procsinfo_t procData;
        if (get_process_infos(m_cgroup_all_pids[i], m_cgroup_processes_include_threads, &procData, output_opts,
                true /* output_tgid */)) {

            if (needsToFilterOutThreads) {
                // only the main thread has its PID == TGID...
                if (procData.pi_pid == procData.pi_tgid)
                    // this is the main thread of current PID... insert it into the database
                    currDB.insert(std::make_pair(m_cgroup_all_pids[i], procData));
                else
                    nthreads_discarded++;
            } else {
                // we can simply take into account all PIDs/TIDs collected
                currDB.insert(std::make_pair(m_cgroup_all_pids[i], procData));
            }
        } else
            nfailed_sampling++;
    }

    if (output_opts == PF_NONE) {
        CMonitorLogger::instance()->LogDebug(
            "Initialized process DB with %lu entries on this first sample. Not generating any output.\n",
            currDB.size());
        return;
    }

    // Sort the processes by their "score" by inserting them into an ordered map
    assert(m_topper_procs.empty());
    CMonitorLogger::instance()->LogDebug(
        "The current process DB now has %lu entries (failed to sample %zu processes; %lu threads discarded), "
        "the DB storing previous statuses has %lu entries.\n",
        currDB.size(), nfailed_sampling, nthreads_discarded, prevDB.size());

    for (const auto& current_entry : currDB) {
        pid_t current_pid = current_entry.first;
        const procsinfo_t* pcurrent_status = &current_entry.second;

        // find the previous stats for this PID:
        auto itPrevStatus = prevDB.find(current_pid);
        if (itPrevStatus == prevDB.end())
            // this process apparently is a new-born (we have no records for it in previous sample!); we cannot
            // consider it yet for "topper" computations since we are unable to compute CPU utilization
            // (we need at least 2 samples)
            continue;

        const procsinfo_t* pprev_status = &itPrevStatus->second;

        // compute the score
        uint64_t score = compute_proc_score(pcurrent_status, pprev_status, elapsed_sec);
        proc_topper_t newEntry = { .current = pcurrent_status, .prev = pprev_status };
        m_topper_procs.insert(std::make_pair(score, newEntry));

        // of the 40 fields of procsinfo_t we're mostly interested in user and system time:
        CMonitorLogger::instance()->LogDebug(
            "pid=%d: %s: utime=%lu, stime=%lu, prev_utime=%lu, prev_stime=%lu, score=%lu", // force newline
            pcurrent_status->pi_pid, pcurrent_status->pi_comm, // force newline
            pcurrent_status->pi_utime, pcurrent_status->pi_stime, // force newline
            pprev_status->pi_utime, pprev_status->pi_stime, score);
        // CMonitorLogger::instance()->LogDebug("PID=%lu -> score=%lu", current_entry.first, score);
    }

    if (m_topper_procs.empty()) {
        // just produce an empty section to have all samples structured in the same way, then return
        m_pOutput->psection_start("cgroup_tasks");
        m_pOutput->psection_end();
        return;
    }

    CMonitorLogger::instance()->LogDebug(
        "Tracking %zu/%zu processes/threads (include_threads=%d); min/max score found: %lu/%lu", // force
                                                                                                 // newline
        currDB.size(), m_cgroup_all_pids.size(), m_cgroup_processes_include_threads, m_topper_procs.begin()->first,
        m_topper_procs.rbegin()->first);

    // Now output all data for each process, starting from the minimal score PROCESS_SCORE_IGNORE_THRESHOLD
    static double ticks = (double)sysconf(_SC_CLK_TCK); // clock ticks per second
    size_t nProcsOverThreshold = 0;
    m_pOutput->psection_start("cgroup_tasks");
    for (auto entry = m_topper_procs.lower_bound(m_pCfg->m_nProcessScoreThreshold); entry != m_topper_procs.end();
         entry++) {
        uint64_t score = (*entry).first;

        // note that the m_topper_procs map contains pointers to "currDB" and "prevDB"
        const procsinfo_t* p = (*entry).second.current;
        const procsinfo_t* q = (*entry).second.prev;

#define CURRENT(member) (p->member)
#define PREVIOUS(member) (q->member)
#define DELTA(member) (CURRENT(member) - PREVIOUS(member))
#define COUNTDELTA(member) ((PREVIOUS(member) > CURRENT(member)) ? 0 : (CURRENT(member) - PREVIOUS(member)))

        m_pOutput->psubsection_start(fmt::format("pid_{}", (unsigned long)CURRENT(pi_pid)).c_str());
        m_pOutput->plong("cmon_score", score);

        /*
         * Process fields
         *
         * NOTE: "process groups" and "sessions" are feature from 1980s mostly related to shell implementations
         *       see https://www.gnu.org/software/libc/manual/html_node/Concepts-of-Job-Control.html
         *           https://en.wikipedia.org/wiki/Process_group
         *       to avoid confusing the consumer of data, they're left out of the data stream
         */
        m_pOutput->pstring(
            "cmd", CURRENT(pi_comm)); // Full command line can be found /proc/PID/cmdline with zeros in it!
        m_pOutput->plong("pid", CURRENT(pi_pid));
        m_pOutput->plong("ppid", CURRENT(pi_ppid));
        m_pOutput->plong("tgid", CURRENT(pi_tgid));
        m_pOutput->plong("priority", CURRENT(pi_priority));
        m_pOutput->plong("nice", CURRENT(pi_nice));
        m_pOutput->pstring("state", get_state(CURRENT(pi_state)));
        m_pOutput->plong("uid", CURRENT(uid));
        if (output_opts == PF_ALL) {
            // seldomly used fields:
            m_pOutput->plong("tty_nr", CURRENT(pi_tty_nr));
            m_pOutput->plong("threads", CURRENT(pi_num_threads));
            m_pOutput->plong("pgrp", CURRENT(pi_pgrp)); // see NOTE above
            m_pOutput->plong("session", CURRENT(pi_session)); // see NOTE above
            if (strlen(CURRENT(username)) > 0)
                m_pOutput->pstring("username", CURRENT(username));
            m_pOutput->pdouble("start_time_secs", (double)(CURRENT(pi_start_time)) / ticks);
        }

        /*
         * CPU fields
         * NOTE: all CPU fields specify amount of time, measured in units of USER_HZ
                 (1/100ths of a second on most architectures); this means that if the
                 _delta_ CPU value reported is 60 in USR/SYSTEM mode it indicates the process
                 used for 60% of the last 10ms of CPU!
                 IOW there is no need to do any math to produce a percentage, just taking
                 the delta of the absolute, monotonic-increasing value and divide by the elapsed time
        */
        m_pOutput->plong("cpu_last", CURRENT(pi_last_cpu));
        m_pOutput->pdouble(
            "cpu_usr", std::min(100.0, (double)DELTA(pi_utime) / elapsed_sec)); // percentage between 0-100
        m_pOutput->pdouble(
            "cpu_sys", std::min(100.0, (double)DELTA(pi_stime) / elapsed_sec)); // percentage between 0-100

        // provide also the total, monotonically-increasing CPU time:
        // this is used by chart script to produce the "top of the topper" chart
        m_pOutput->pdouble("cpu_usr_total_secs", (double)CURRENT(pi_utime) / ticks);
        m_pOutput->pdouble("cpu_sys_total_secs", (double)CURRENT(pi_stime) / ticks);

        /*
         * Memory fields
         */
        if (output_opts == PF_ALL) {
            m_pOutput->plong("mem_size_kb", CURRENT(statm_size) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_resident_kb", CURRENT(statm_resident) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_restext_kb", CURRENT(statm_trs) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_resdata_kb", CURRENT(statm_drs) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_share_kb", CURRENT(statm_share) * PAGESIZE_BYTES / 1024);
            m_pOutput->plong("mem_rss_limit_bytes", CURRENT(pi_rsslimit));
        }
        m_pOutput->pdouble("mem_minor_fault", COUNTDELTA(pi_minflt) / elapsed_sec);
        m_pOutput->pdouble("mem_major_fault", COUNTDELTA(pi_majflt) / elapsed_sec);
        m_pOutput->plong("mem_virtual_bytes", CURRENT(pi_vsize));
        m_pOutput->plong("mem_rss_bytes", CURRENT(pi_rss) * PAGESIZE_BYTES);

        /*
         * Signal fields
         */
#if PROCESS_DEBUGGING_ADDRESSES_SIGNALS
        /* NOT INCLUDED AS THEY ARE FOR DEBUGGING AND NOT PERFORMANCE TUNING */
        m_pOutput->phex("start_code", CURRENT(pi_start_code));
        m_pOutput->phex("end_code", CURRENT(pi_end_code));
        m_pOutput->phex("start_stack", CURRENT(pi_start_stack));
        m_pOutput->phex("esp_stack_pointer", CURRENT(pi_esp));
        m_pOutput->phex("eip_instruction_pointer", CURRENT(pi_eip));
        m_pOutput->phex("signal_pending", CURRENT(pi_signal_pending));
        m_pOutput->phex("signal_blocked", CURRENT(pi_signal_blocked));
        m_pOutput->phex("signal_ignore", CURRENT(pi_signal_ignore));
        m_pOutput->phex("signal_catch", CURRENT(pi_signal_catch));
        m_pOutput->phex("signal_exit", CURRENT(pi_signal_exit));
        m_pOutput->phex("wchan", CURRENT(pi_wchan));
        /* NOT INCLUDED AS THEY ARE FOR DEBUGGING AND NOT PERFORMANCE TUNING */
#endif
        if (output_opts == PF_ALL) {
            m_pOutput->plong("swap_pages", CURRENT(pi_swap_pages));
            m_pOutput->plong("child_swap_pages", CURRENT(pi_child_swap_pages));
            m_pOutput->plong("realtime_priority", CURRENT(pi_realtime_priority));
            m_pOutput->plong("sched_policy", CURRENT(pi_sched_policy));
        }

        /*
         * I/O fields
         */
        m_pOutput->pdouble("io_delayacct_blkio_secs", (double)CURRENT(pi_delayacct_blkio_ticks) / ticks);
        m_pOutput->plong("io_rchar", DELTA(io_rchar) / elapsed_sec);
        m_pOutput->plong("io_wchar", DELTA(io_wchar) / elapsed_sec);
        if (output_opts == PF_ALL) {
            m_pOutput->plong("io_read_bytes", DELTA(io_read_bytes) / elapsed_sec);
            m_pOutput->plong("io_write_bytes", DELTA(io_write_bytes) / elapsed_sec);
        }

        // provide also the total, monotonically-increasing I/O time:
        // this is used by chart script to produce the "top of the topper" chart
        m_pOutput->plong("io_total_read", CURRENT(io_rchar));
        m_pOutput->plong("io_total_write", CURRENT(io_wchar));

        m_pOutput->psubsection_end();
        nProcsOverThreshold++;
    }
    m_pOutput->psection_end();

    CMonitorLogger::instance()->LogDebug("%zu processes found over score threshold", nProcsOverThreshold);
    m_topper_procs.clear();
}
