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

#ifdef PROMETHEUS_SUPPORT
#include "prometheus_kpi.h"
#endif

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

#ifdef PROMETHEUS_SUPPORT
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

static const prometheus_kpi_descriptor prometheus_kpi_proc_meminfo[] = {
    // baremetal : proc_meminfo
    { "proc_meminfo_MemTotal", KPI_TYPE::Gauge, "Total amount of usable RAM, in kibibytes" },
    { "proc_meminfo_MemAvailable", KPI_TYPE::Gauge,
        "An estimate of how much memory is available for starting new applications" },
    { "proc_meminfo_MemFree", KPI_TYPE::Gauge, "The amount of physical RAM, in kibibytes, left unused by the system" },
    { "proc_meminfo_Buffers", KPI_TYPE::Gauge, "The amount, in kibibytes, of temporary storage for raw disk blocks" },
    { "proc_meminfo_Cached", KPI_TYPE::Gauge, "The amount of physical RAM, in kibibytes, used as cache memory" },
    { "proc_meminfo_SwapCached", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, that has once been moved into swap, then back into the main memory, but "
        "still also remains in the swapfile" },
    { "proc_meminfo_Active", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, that has been used more recently and is usually not reclaimed unless "
        "absolutely necessary" },
    { "proc_meminfo_Inactive", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, that has been used less recently and is more eligible to be reclaimed for "
        "other purposes" },
    { "proc_meminfo_Active_anon", KPI_TYPE::Gauge,
        "The amount of anonymous and tmpfs/shmem memory that is in active use" },
    { "proc_meminfo_Inactive_anon", KPI_TYPE::Gauge,
        "The amount of anonymous and tmpfs/shmem memory that is a candidate for eviction" },
    { "proc_meminfo_Active_file", KPI_TYPE::Gauge,
        "The amount of file cache memory, in kibibytes, that is in active use" },
    { "proc_meminfo_Inactive_file", KPI_TYPE::Gauge,
        "The amount of file cache memory, in kibibytes, that is newly loaded from the disk, or is a candidate for "
        "reclaiming" },
    { "proc_meminfo_Unevictable", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, discovered by the pageout code, that is not evictable because it is "
        "locked into memory by user programs" },
    { "proc_meminfo_Mlocked", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, that is not evictable because it is locked into memory by user "
        "programs" },
    { "proc_meminfo_SwapTotal", KPI_TYPE::Gauge, "The total amount of swap available, in kibibytes" },
    { "proc_meminfo_SwapFree", KPI_TYPE::Gauge, "The total amount of swap free, in kibibytes" },
    { "proc_meminfo_Dirty", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, waiting to be written back to the disk" },
    { "proc_meminfo_Writeback", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, actively being written back to the disk" },
    { "proc_meminfo_AnonPages", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, used by pages that are not backed by files and are mapped into "
        "userspace page tables" },
    { "proc_meminfo_Mapped", KPI_TYPE::Gauge,
        "The memory, in kibibytes, used for files that have been mmaped, such as libraries" },
    { "proc_meminfo_Shmem", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, used by shared memory (shmem) and tmpfs" },
    { "proc_meminfo_Slab", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, used by the kernel to cache data structures for its own use" },
    { "proc_meminfo_SReclaimable", KPI_TYPE::Gauge, "The part of Slab that can be reclaimed, such as caches" },
    { "proc_meminfo_SUnreclaim", KPI_TYPE::Gauge,
        "The part of Slab that cannot be reclaimed even when lacking memory" },
    { "proc_meminfo_KernelStack", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, used by the kernel stack allocations done for each task in the system" },
    { "proc_meminfo_PageTables", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, dedicated to the lowest page table level" },
    { "proc_meminfo_NFS_Unstable", KPI_TYPE::Gauge,
        "The amount, in kibibytes, of NFS pages sent to the server but not yet committed to the stable storage" },
    { "proc_meminfo_Bounce", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, used for the block device bounce buffers" },
    { "proc_meminfo_WritebackTmp", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, used by FUSE for temporary writeback buffers" },
    { "proc_meminfo_CommitLimit", KPI_TYPE::Gauge,
        "The total amount of memory currently available to be allocated on the system based on the overcommit ratio "
        "(vm.overcommit_ratio)" },
    { "proc_meminfo_Committed_AS", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, estimated to complete the workload" },
    { "proc_meminfo_VmallocTotal", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, of total allocated virtual address space" },
    { "proc_meminfo_VmallocUsed", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, of used virtual address space" },
    { "proc_meminfo_VmallocChunk", KPI_TYPE::Gauge,
        "The largest contiguous block of memory, in kibibytes, of available virtual address space" },
    { "proc_meminfo_Percpu", KPI_TYPE::Gauge, "The amount of memory dedicated to per-cpu objects" },
    { "proc_meminfo_HardwareCorrupted", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, with physical memory corruption problems, identified by the hardware and "
        "set aside by the kernel so it does not get used" },
    { "proc_meminfo_AnonHugePages", KPI_TYPE::Gauge,
        "The total amount of memory, in kibibytes, used by huge pages that are not backed by files and are mapped into "
        "userspace page tables" },
    { "proc_meminfo_CmaTotal", KPI_TYPE::Gauge, "Total amount of Contiguous Memory Area reserved for current kernel" },
    { "proc_meminfo_CmaFree", KPI_TYPE::Gauge, "Contiguous Memory Area that is free to use by current kernel" },
    { "proc_meminfo_HugePages_Total", KPI_TYPE::Gauge, "The total number of hugepages for the system" },
    { "proc_meminfo_HugePages_Free", KPI_TYPE::Gauge, "The total number of hugepages available for the system" },
    { "proc_meminfo_HugePages_Rsvd", KPI_TYPE::Gauge, "The number of unused huge pages reserved for hugetlbfs" },
    { "proc_meminfo_HugePages_Surp", KPI_TYPE::Gauge, "The number of surplus huge pages" },
    { "proc_meminfo_Hugepagesize", KPI_TYPE::Gauge, "The size for each hugepages unit in kibibytes" },
    { "proc_meminfo_DirectMap4k", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, mapped into kernel address space with 4 kB page mappings" },
    { "proc_meminfo_DirectMap2M", KPI_TYPE::Gauge,
        "The amount of memory, in kibibytes, mapped into kernel address space with 2 MB page mappings" },
    { "proc_meminfo_DirectMap1G", KPI_TYPE::Gauge, "The amount of memory being mapped to hugepages 1GB in size" },
};

static const prometheus_kpi_descriptor prometheus_kpi_proc_vmstat[] = {
    // baremetal: proc_vmstat
    { "proc_vmstat_allocstall", KPI_TYPE::Gauge, "proc_vmstat_allocstall" },
    { "proc_vmstat_balloon_deflate", KPI_TYPE::Gauge, "proc_vmstat_balloon_deflate" },
    { "proc_vmstat_balloon_inflate", KPI_TYPE::Gauge, "proc_vmstat_balloon_inflate" },
    { "proc_vmstat_balloon_migrate", KPI_TYPE::Gauge, "proc_vmstat_balloon_migrate" },
    { "proc_vmstat_compact_fail", KPI_TYPE::Gauge, "proc_vmstat_compact_fail" },
    { "proc_vmstat_compact_free_scanned", KPI_TYPE::Gauge, "proc_vmstat_compact_free_scanned" },
    { "proc_vmstat_compact_isolated", KPI_TYPE::Gauge, "proc_vmstat_compact_isolated" },
    { "proc_vmstat_compact_migrate_scanned", KPI_TYPE::Gauge, "proc_vmstat_compact_migrate_scanned" },
    { "proc_vmstat_compact_stall", KPI_TYPE::Gauge, "proc_vmstat_compact_stall" },
    { "proc_vmstat_compact_success", KPI_TYPE::Gauge, "proc_vmstat_compact_success" },
    { "proc_vmstat_drop_pagecache", KPI_TYPE::Gauge, "proc_vmstat_drop_pagecache" },
    { "proc_vmstat_drop_slab", KPI_TYPE::Gauge, "proc_vmstat_drop_slab" },
    { "proc_vmstat_htlb_buddy_alloc_fail", KPI_TYPE::Gauge, "proc_vmstat_htlb_buddy_alloc_fail" },
    { "proc_vmstat_htlb_buddy_alloc_success", KPI_TYPE::Gauge, "proc_vmstat_htlb_buddy_alloc_success" },
    { "proc_vmstat_kswapd_high_wmark_hit_quickly", KPI_TYPE::Gauge, "proc_vmstat_kswapd_high_wmark_hit_quickly" },
    { "proc_vmstat_kswapd_inodesteal", KPI_TYPE::Gauge, "proc_vmstat_kswapd_inodesteal" },
    { "proc_vmstat_kswapd_low_wmark_hit_quickly", KPI_TYPE::Gauge, "proc_vmstat_kswapd_low_wmark_hit_quickly" },
    { "proc_vmstat_nr_active_anon", KPI_TYPE::Gauge, "proc_vmstat_nr_active_anon" },
    { "proc_vmstat_nr_active_file", KPI_TYPE::Gauge, "proc_vmstat_nr_active_file" },
    { "proc_vmstat_nr_alloc_batch", KPI_TYPE::Gauge, "proc_vmstat_nr_alloc_batch" },
    { "proc_vmstat_nr_anon_pages", KPI_TYPE::Gauge, "proc_vmstat_nr_anon_pages" },
    { "proc_vmstat_nr_anon_transparent_hugepages", KPI_TYPE::Gauge, "proc_vmstat_nr_anon_transparent_hugepages" },
    { "proc_vmstat_nr_bounce", KPI_TYPE::Gauge, "proc_vmstat_nr_bounce" },
    { "proc_vmstat_nr_dirtied", KPI_TYPE::Gauge, "proc_vmstat_nr_dirtied" },
    { "proc_vmstat_nr_dirty", KPI_TYPE::Gauge, "proc_vmstat_nr_dirty" },
    { "proc_vmstat_nr_dirty_background_threshold", KPI_TYPE::Gauge, "proc_vmstat_nr_dirty_background_threshold" },
    { "proc_vmstat_nr_dirty_threshold", KPI_TYPE::Gauge, "proc_vmstat_nr_dirty_threshold" },
    { "proc_vmstat_nr_file_pages", KPI_TYPE::Gauge, "proc_vmstat_nr_file_pages" },
    { "proc_vmstat_nr_free_cma", KPI_TYPE::Gauge, "proc_vmstat_nr_free_cma" },
    { "proc_vmstat_nr_free_pages", KPI_TYPE::Gauge, "proc_vmstat_nr_free_pages" },
    { "proc_vmstat_nr_inactive_anon", KPI_TYPE::Gauge, "proc_vmstat_nr_inactive_anon" },
    { "proc_vmstat_nr_inactive_file", KPI_TYPE::Gauge, "proc_vmstat_nr_inactive_file" },
    { "proc_vmstat_nr_isolated_anon", KPI_TYPE::Gauge, "proc_vmstat_nr_isolated_anon" },
    { "proc_vmstat_nr_isolated_file", KPI_TYPE::Gauge, "proc_vmstat_nr_isolated_file" },
    { "proc_vmstat_nr_kernel_stack", KPI_TYPE::Gauge, "proc_vmstat_nr_kernel_stack" },
    { "proc_vmstat_nr_mapped", KPI_TYPE::Gauge, "proc_vmstat_nr_mapped" },
    { "proc_vmstat_nr_mlock", KPI_TYPE::Gauge, "proc_vmstat_nr_mlock" },
    { "proc_vmstat_nr_page_table_pages", KPI_TYPE::Gauge, "proc_vmstat_nr_page_table_pages" },
    { "proc_vmstat_nr_shmem", KPI_TYPE::Gauge, "proc_vmstat_nr_shmem" },
    { "proc_vmstat_nr_slab_reclaimable", KPI_TYPE::Gauge, "proc_vmstat_nr_slab_reclaimable" },
    { "proc_vmstat_nr_slab_unreclaimable", KPI_TYPE::Gauge, "proc_vmstat_nr_slab_unreclaimable" },
    { "proc_vmstat_nr_unevictable", KPI_TYPE::Gauge, "proc_vmstat_nr_unevictable" },
    { "proc_vmstat_nr_unstable", KPI_TYPE::Gauge, "proc_vmstat_nr_unstable" },
    { "proc_vmstat_nr_vmscan_immediate_reclaim", KPI_TYPE::Gauge, "proc_vmstat_nr_vmscan_immediate_reclaim" },
    { "proc_vmstat_nr_vmscan_write", KPI_TYPE::Gauge, "proc_vmstat_nr_vmscan_write" },
    { "proc_vmstat_nr_writeback", KPI_TYPE::Gauge, "proc_vmstat_nr_writeback" },
    { "proc_vmstat_nr_writeback_temp", KPI_TYPE::Gauge, "proc_vmstat_nr_writeback_temp" },
    { "proc_vmstat_nr_written", KPI_TYPE::Gauge, "proc_vmstat_nr_written" },
    { "proc_vmstat_numa_foreign", KPI_TYPE::Gauge, "proc_vmstat_numa_foreign" },
    { "proc_vmstat_numa_hint_faults", KPI_TYPE::Gauge, "proc_vmstat_numa_hint_faults" },
    { "proc_vmstat_numa_hint_faults_local", KPI_TYPE::Gauge, "" },
    { "proc_vmstat_numa_hit", KPI_TYPE::Gauge, "proc_vmstat_numa_hit" },
    { "proc_vmstat_numa_huge_pte_updates", KPI_TYPE::Gauge, "proc_vmstat_numa_huge_pte_updates" },
    { "proc_vmstat_numa_interleave", KPI_TYPE::Gauge, "proc_vmstat_numa_interleave" },
    { "proc_vmstat_numa_local", KPI_TYPE::Gauge, "proc_vmstat_numa_local" },
    { "proc_vmstat_numa_miss", KPI_TYPE::Gauge, "proc_vmstat_numa_miss" },
    { "proc_vmstat_numa_other", KPI_TYPE::Gauge, "proc_vmstat_numa_other" },
    { "proc_vmstat_numa_pages_migrated", KPI_TYPE::Gauge, "proc_vmstat_numa_pages_migrated" },
    { "proc_vmstat_numa_pte_updates", KPI_TYPE::Gauge, "proc_vmstat_numa_pte_updates" },
    { "proc_vmstat_pageoutrun", KPI_TYPE::Gauge, "proc_vmstat_pageoutrun" },
    { "proc_vmstat_pgactivate", KPI_TYPE::Gauge, "proc_vmstat_pgactivate" },
    { "proc_vmstat_pgalloc_dma", KPI_TYPE::Gauge, "proc_vmstat_pgalloc_dma" },
    { "proc_vmstat_pgalloc_dma32", KPI_TYPE::Gauge, "proc_vmstat_pgalloc_dma32" },
    { "proc_vmstat_pgalloc_movable", KPI_TYPE::Gauge, "proc_vmstat_pgalloc_movable" },
    { "proc_vmstat_pgalloc_normal", KPI_TYPE::Gauge, "proc_vmstat_pgalloc_normal" },
    { "proc_vmstat_pgdeactivate", KPI_TYPE::Gauge, "proc_vmstat_pgdeactivate" },
    { "proc_vmstat_pgfault", KPI_TYPE::Gauge, "proc_vmstat_pgfault" },
    { "proc_vmstat_pgfree", KPI_TYPE::Gauge, "proc_vmstat_pgfree" },
    { "proc_vmstat_pginodesteal", KPI_TYPE::Gauge, "proc_vmstat_pginodesteal" },
    { "proc_vmstat_pglazyfreed", KPI_TYPE::Gauge, "proc_vmstat_pglazyfreed" },
    { "proc_vmstat_pgmajfault", KPI_TYPE::Gauge, "proc_vmstat_pgmajfault" },
    { "proc_vmstat_pgmigrate_fail", KPI_TYPE::Gauge, "proc_vmstat_pgmigrate_fail" },
    { "proc_vmstat_pgmigrate_success", KPI_TYPE::Gauge, "proc_vmstat_pgmigrate_success" },
    { "proc_vmstat_pgpgin", KPI_TYPE::Gauge, "proc_vmstat_pgpgin" },
    { "proc_vmstat_pgpgout", KPI_TYPE::Gauge, "proc_vmstat_pgpgout" },
    { "proc_vmstat_pgrefill_dma", KPI_TYPE::Gauge, "proc_vmstat_pgrefill_dma" },
    { "proc_vmstat_pgrefill_dma32", KPI_TYPE::Gauge, "proc_vmstat_pgrefill_dma32" },
    { "proc_vmstat_pgrefill_movable", KPI_TYPE::Gauge, "proc_vmstat_pgrefill_movable" },
    { "proc_vmstat_pgrefill_normal", KPI_TYPE::Gauge, "proc_vmstat_pgrefill_normal" },
    { "proc_vmstat_pgrotated", KPI_TYPE::Gauge, "proc_vmstat_pgrotated" },
    { "proc_vmstat_pgscan_direct_dma", KPI_TYPE::Gauge, "proc_vmstat_pgscan_direct_dma" },
    { "proc_vmstat_pgscan_direct_dma32", KPI_TYPE::Gauge, "proc_vmstat_pgscan_direct_dma32" },
    { "proc_vmstat_pgscan_direct_movable", KPI_TYPE::Gauge, "proc_vmstat_pgscan_direct_movable" },
    { "proc_vmstat_pgscan_direct_normal", KPI_TYPE::Gauge, "proc_vmstat_pgscan_direct_normal" },
    { "proc_vmstat_pgscan_direct_throttle", KPI_TYPE::Gauge, "proc_vmstat_pgscan_direct_throttle" },
    { "proc_vmstat_pgscan_kswapd_dma", KPI_TYPE::Gauge, "proc_vmstat_pgscan_kswapd_dma" },
    { "proc_vmstat_pgscan_kswapd_dma32", KPI_TYPE::Gauge, "proc_vmstat_pgscan_kswapd_dma32" },
    { "proc_vmstat_pgscan_kswapd_movable", KPI_TYPE::Gauge, "proc_vmstat_pgscan_kswapd_movable" },
    { "proc_vmstat_pgscan_kswapd_normal", KPI_TYPE::Gauge, "proc_vmstat_pgscan_kswapd_normal" },
    { "proc_vmstat_pgsteal_direct_dma", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_direct_dma" },
    { "proc_vmstat_pgsteal_direct_dma32", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_direct_dma32" },
    { "proc_vmstat_pgsteal_direct_movable", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_direct_movable" },
    { "proc_vmstat_pgsteal_direct_normal", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_direct_normal" },
    { "proc_vmstat_pgsteal_kswapd_dma", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_kswapd_dma" },
    { "proc_vmstat_pgsteal_kswapd_dma32", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_kswapd_dma32" },
    { "proc_vmstat_pgsteal_kswapd_movable", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_kswapd_movable" },
    { "proc_vmstat_pgsteal_kswapd_normal", KPI_TYPE::Gauge, "proc_vmstat_pgsteal_kswapd_normal" },
    { "proc_vmstat_pswpin", KPI_TYPE::Gauge, "proc_vmstat_pswpin" },
    { "proc_vmstat_pswpout", KPI_TYPE::Gauge, "proc_vmstat_pswpout" },
    { "proc_vmstat_slabs_scanned", KPI_TYPE::Gauge, "proc_vmstat_slabs_scanned" },
    { "proc_vmstat_swap_ra", KPI_TYPE::Gauge, "proc_vmstat_swap_ra" },
    { "proc_vmstat_swap_ra_hit", KPI_TYPE::Gauge, "proc_vmstat_swap_ra_hit" },
    { "proc_vmstat_thp_collapse_alloc", KPI_TYPE::Gauge, "proc_vmstat_thp_collapse_alloc" },
    { "proc_vmstat_thp_collapse_alloc_failed", KPI_TYPE::Gauge, "proc_vmstat_thp_collapse_alloc_failed" },
    { "proc_vmstat_thp_fault_alloc", KPI_TYPE::Gauge, "proc_vmstat_thp_fault_alloc" },
    { "proc_vmstat_thp_fault_fallback", KPI_TYPE::Gauge, "proc_vmstat_thp_fault_fallback" },
    { "proc_vmstat_thp_split", KPI_TYPE::Gauge, "proc_vmstat_thp_split" },
    { "proc_vmstat_thp_zero_page_alloc", KPI_TYPE::Gauge, "proc_vmstat_thp_zero_page_alloc" },
    { "proc_vmstat_thp_zero_page_alloc_failed", KPI_TYPE::Gauge, "proc_vmstat_thp_zero_page_alloc_failed" },
    { "proc_vmstat_unevictable_pgs_cleared", KPI_TYPE::Gauge, "proc_vmstat_unevictable_pgs_cleared" },
    { "proc_vmstat_unevictable_pgs_culled", KPI_TYPE::Gauge, "proc_vmstat_unevictable_pgs_culled" },
    { "proc_vmstat_unevictable_pgs_mlocked", KPI_TYPE::Gauge, "proc_vmstat_unevictable_pgs_mlocked" },
    { "proc_vmstat_unevictable_pgs_munlocked", KPI_TYPE::Gauge, "proc_vmstat_unevictable_pgs_munlocked" },
    { "proc_vmstat_unevictable_pgs_rescued", KPI_TYPE::Gauge, "proc_vmstat_unevictable_pgs_rescued" },
    { "proc_vmstat_unevictable_pgs_scanned", KPI_TYPE::Gauge, "proc_vmstat_unevictable_pgs_scanned" },
    { "proc_vmstat_unevictable_pgs_stranded", KPI_TYPE::Gauge, "proc_vmstat_unevictable_pgs_stranded" },
    { "proc_vmstat_workingset_activate", KPI_TYPE::Gauge, "proc_vmstat_workingset_activate" },
    { "proc_vmstat_workingset_nodereclaim", KPI_TYPE::Gauge, "proc_vmstat_workingset_nodereclaim" },
    { "proc_vmstat_workingset_refault", KPI_TYPE::Gauge, "proc_vmstat_workingset_refault" },
    { "proc_vmstat_zone_reclaim_failed", KPI_TYPE::Gauge, "proc_vmstat_zone_reclaim_failed" },
};
#endif

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
