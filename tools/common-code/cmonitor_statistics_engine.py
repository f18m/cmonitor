#!/usr/bin/python3

#
# cmonitor_statistics_engine.py
#
# Author: Gajanan Khandake
# Created: April 2021
#

import argparse
import json
import os
import sys
import gzip
from statistics import mean, median, mode, StatisticsError


# =======================================================================================================
# Helper classes
# =======================================================================================================
class GenericStatisticsCalculator:
    """
    Provides basic statistical analysis over a number of samples that represent a KPI changing over time.
    """

    def __init__(self, unit: str) -> None:
        self.__stats = list()
        self.__unit = unit

    def insert_stat(self, value) -> None:
        self.__stats.append(value)

    def get_min(self):
        return min(self.__stats)

    def get_max(self):
        return max(self.__stats)

    def get_mean(self):
        return mean(self.__stats)

    def get_median(self):
        return median(self.__stats)

    def get_mode(self):
        try:
            return mode(self.__stats)
        except StatisticsError:
            return "no unique mode"

    def get_unit(self):
        return self.__unit

    def get_all_stats(self):
        return self.__stats

    def dump_json(self, verbose) -> dict:
        statistics = dict()

        if len(self.__stats) > 0:
            statistics["minimum"] = self.get_min()
            statistics["maximum"] = self.get_max()
            statistics["mean"] = self.get_mean()
            statistics["median"] = self.get_median()
            statistics["mode"] = self.get_mode()
            statistics["unit"] = self.get_unit()
            if verbose:
                statistics["stats"] = self.__stats
                statistics["samples"] = len(self.__stats)

        return statistics


class CgroupTasksStatistics:
    """
    Stores all important statistical information associated with a Linux Cgroup
    """

    def __init__(self) -> None:
        self.cpu = GenericStatisticsCalculator("%")
        self.memory = GenericStatisticsCalculator("bytes")
        self.io = GenericStatisticsCalculator("bytes")
        self.memory_failcnt = GenericStatisticsCalculator("")
        self.cpu_throttle = GenericStatisticsCalculator("%")

    def insert_cpu_stats(self, stats: dict, sample_index: int) -> None:
        if "cpu_tot" in stats:
            self.cpu.insert_stat(stats["cpu_tot"]["user"] + stats["cpu_tot"]["sys"])
        else:
            print(f"WARNING: The JSON file provided does not contain the 'cpu_tot' measurement for sample #{sample_index}. Skipping this sample.")

        if "throttling" in stats:
            cpu_throttle_percentage = 0
            if stats["throttling"]["nr_periods"] > 0:
                cpu_throttle_percentage = (stats["throttling"]["nr_throttled"] * 100) / stats["throttling"]["nr_periods"]
            self.cpu_throttle.insert_stat(cpu_throttle_percentage)
        else:
            print(f"WARNING: The JSON file provided does not contain the 'throttling' measurement for sample #{sample_index}. Skipping this sample.")

    def dump_cpu_stats(self, verbose) -> dict:
        return self.cpu.dump_json(verbose)

    def dump_cpu_throttle_stats(self, verbose) -> dict:
        return self.cpu_throttle.dump_json(verbose)

    def insert_memory_stats(self, stats: dict, sample_index: int, cgroup_version: int) -> None:
        stat_label = "stat.rss" if cgroup_version == 1 else "stat.file"
        if stat_label in stats:
            self.memory.insert_stat(stats[stat_label])
        else:
            print(
                f"WARNING: The JSON file provided does not contain the '{stat_label}' measurement for sample #{sample_index}. Skipping this sample."
            )

        stat_label = "events.failcnt" if cgroup_version == 1 else "events.oom_kill"
        if stat_label in stats:
            self.memory_failcnt.insert_stat(stats[stat_label])
        else:
            print(
                f"WARNING: The JSON file provided does not contain the '{stat_label}' measurement for sample #{sample_index}. Skipping this sample."
            )

    def dump_memory_stats(self, verbose) -> dict:
        return self.memory.dump_json(verbose)

    def dump_memory_failcnt_stats(self, verbose) -> dict:
        return self.memory_failcnt.dump_json(verbose)

    # cgroup_blkio not yet available
    # def insert_io_stats(self, stats: dict) -> None:
    #    self.io.insert_stat(stats["io_rchar"] + stats["io_wchar"])
    # def dump_io_stats(self) -> None:
    #    return self.io.dump_json()


# =======================================================================================================
# CmonitorStatisticsEngine
# =======================================================================================================
class CmonitorStatisticsEngine:
    """
    Interface between JSON structure collected by cmonitor_collector and statistical calculators
    """

    def __init__(self, be_verbose=False) -> None:
        self.cgroup_statistics = CgroupTasksStatistics()
        self.verbose = be_verbose
        self.num_samples_analyzed = 0
        pass

    def process(self, json_data) -> bool:
        """
        Loads the provided JSON data and runs all statistical analyses on it.
        """
        if "samples" not in json_data:
            print("Unexpected JSON format. Aborting.")
            return False
        if len(json_data["samples"]) <= 2:
            print("This tool requires at least 3 samples in the input JSON file. Aborting.")
            return False

        cgroup_version = 1
        jheader = json_data["header"]
        if "cgroup_config" in jheader and "version" in jheader["cgroup_config"]:
            cgroup_version = int(jheader["cgroup_config"]["version"])
        else:
            print(f"WARNING: cgroup version not found in header using default value {cgroup_version}")

        # skip sample 0 because it contains less statistics due to the differential logic that requires some
        # initialization sample for most of the stats
        first_sample = json_data["samples"][1]
        do_cpu_stats = True
        if "cgroup_cpuacct_stats" not in first_sample:
            do_cpu_stats = False
            print(
                "WARNING: The JSON file provided does not contain measurements for the 'cpuacct' cgroup. Please use '--collect=cgroup_cpu' when launching cmonitor_collector."
            )
        elif "cpu_tot" not in first_sample["cgroup_cpuacct_stats"]:
            do_cpu_stats = False
            print(
                "WARNING: The JSON file provided does not contain the 'cpu_tot' measurement. Probably it was produced by cmonitor version 1.7-0 or earlier. Skipping CPU statistics."
            )

        do_memory_stats = True
        if "cgroup_memory_stats" not in first_sample:
            do_memory_stats = False
            print(
                "WARNING: The JSON file provided does not contain measurements for the 'memory' cgroup. Please use '--collect=cgroup_memory' when launching cmonitor_collector."
            )

        samples_to_analyze = json_data["samples"][1:]
        for sample in samples_to_analyze:
            try:
                nsample = sample["timestamp"]["sample_index"]
            except KeyError:
                nsample = -1
            if do_cpu_stats:
                self.cgroup_statistics.insert_cpu_stats(sample["cgroup_cpuacct_stats"], nsample)
            if do_memory_stats:
                self.cgroup_statistics.insert_memory_stats(sample["cgroup_memory_stats"], nsample, cgroup_version)

        self.num_samples_analyzed = len(samples_to_analyze)
        # self.cgroup_statistics.insert_io_stats(stats)     # cgroup_blkio not yet available
        return True

    def __dump_json_to_file(
        self,
        statistics: dict,
        outfile: str,
    ) -> None:
        print(f"Opening output file {outfile}")
        with open(outfile, "w") as of:
            json.dump(statistics, of)

    def get_cgroup_statistics(self) -> CgroupTasksStatistics:
        return self.cgroup_statistics

    def get_statistics_dict(self) -> dict:
        return {
            "num_samples_analyzed": self.num_samples_analyzed,
            "statistics": {
                "cpu": self.cgroup_statistics.dump_cpu_stats(self.verbose),
                "cpu_throttle": self.cgroup_statistics.dump_cpu_throttle_stats(self.verbose),
                "memory": self.cgroup_statistics.dump_memory_stats(self.verbose),
                "memory_failcnt": self.cgroup_statistics.dump_memory_failcnt_stats(self.verbose),
                # "io": self.cgroup_statistics.dump_io_stats(),
            },
        }

    def dump_statistics_json(self, output_file=None) -> None:
        """
        Writes the result of the statistical analysis on a file or to stdout
        """
        statistics = self.get_statistics_dict()
        if output_file:
            self.__dump_json_to_file(statistics, output_file)
        else:
            print("Result of analysis:")
            print(json.dumps(statistics, indent=4, sort_keys=True))
