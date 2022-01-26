#!/usr/bin/python3

#
# cmonitor_statistics.py
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
from cmonitor_loader import CmonitorCollectorJsonLoader
from cmonitor_version import VERSION_STRING

# =======================================================================================================
# GLOBALs
# =======================================================================================================

verbose = False

# =======================================================================================================
# CLASS
# =======================================================================================================
class CmonitorStatistics:
    class Statistics:
        def __init__(self, unit: str) -> None:
            self.__stats = list()
            self.__unit = unit

        def insert_stat(self, value) -> None:
            self.__stats.append(value)

        def __min(self):
            return min(self.__stats)

        def __max(self):
            return max(self.__stats)

        def __mean(self):
            return mean(self.__stats)

        def __median(self):
            return median(self.__stats)

        def __mode(self):
            try:
                return mode(self.__stats)
            except StatisticsError:
                return "no unique mode"

        def dump_json(self) -> dict:
            global verbose
            statistics = dict()

            if len(self.__stats) > 0:
                statistics["minimum"] = self.__min()
                statistics["maximum"] = self.__max()
                statistics["mean"] = self.__mean()
                statistics["median"] = self.__median()
                statistics["mode"] = self.__mode()
                statistics["unit"] = self.__unit
                if verbose:
                    statistics["stats"] = self.__stats
                    statistics["samples"] = len(self.__stats)

            return statistics

    class CgroupTasksStatistics:
        def __init__(self) -> None:
            self.cpu = CmonitorStatistics.Statistics("%")
            self.memory = CmonitorStatistics.Statistics("bytes")
            self.io = CmonitorStatistics.Statistics("bytes")
            self.memory_failcnt = CmonitorStatistics.Statistics("")
            self.cpu_throttle = CmonitorStatistics.Statistics("%")

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
                print(
                    f"WARNING: The JSON file provided does not contain the 'throttling' measurement for sample #{sample_index}. Skipping this sample."
                )

        def dump_cpu_stats(self) -> None:
            return self.cpu.dump_json()

        def dump_cpu_throttle_stats(self) -> None:
            return self.cpu_throttle.dump_json()

        def insert_memory_stats(self, stats: dict, sample_index: int) -> None:
            if "stat.rss" in stats:
                self.memory.insert_stat(stats["stat.rss"])
            else:
                print(
                    f"WARNING: The JSON file provided does not contain the 'stat.rss' measurement for sample #{sample_index}. Skipping this sample."
                )
            if "events.failcnt" in stats:
                self.memory_failcnt.insert_stat(stats["events.failcnt"])
            else:
                print(
                    f"WARNING: The JSON file provided does not contain the 'events.failcnt' measurement for sample #{sample_index}. Skipping this sample."
                )

        def dump_memory_stats(self) -> None:
            return self.memory.dump_json()

        def dump_memory_failcnt_stats(self) -> None:
            return self.memory_failcnt.dump_json()

        # cgroup_blkio not yet available
        # def insert_io_stats(self, stats: dict) -> None:
        #    self.io.insert_stat(stats["io_rchar"] + stats["io_wchar"])
        # def dump_io_stats(self) -> None:
        #    return self.io.dump_json()

    def __init__(self) -> None:
        self.cgroup_statistics = self.CgroupTasksStatistics()
        pass

    def process(self, input_json: str, output_file: str) -> None:
        global verbose
        json_data = CmonitorCollectorJsonLoader().load(input_json, this_tool_version=VERSION_STRING, be_verbose=verbose)
        if "samples" not in json_data:
            print("Unexpected JSON format. Aborting.")
            sys.exit(1)
        if len(json_data["samples"]) <= 2:
            print("This tool requires at least 3 samples in the input JSON file. Aborting.")
            sys.exit(1)

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

        for sample in json_data["samples"][1:]:
            try:
                nsample = sample["timestamp"]["sample_index"]
            except KeyError:
                nsample = -1
            if do_cpu_stats:
                self.cgroup_statistics.insert_cpu_stats(sample["cgroup_cpuacct_stats"], nsample)
            if do_memory_stats:
                self.cgroup_statistics.insert_memory_stats(sample["cgroup_memory_stats"], nsample)

            # self.cgroup_statistics.insert_io_stats(stats)     # cgroup_blkio not yet available

        self.dump_statistics_json(output_file)

    def __dump_json_to_file(
        self,
        statistics: dict,
        outfile: str,
    ) -> None:
        print(f"Opening output file {outfile}")
        with open(outfile, "w") as of:
            json.dump(statistics, of)

    def dump_statistics_json(self, output_file="") -> None:
        statistics = {
            "statistics": {
                "cpu": self.cgroup_statistics.dump_cpu_stats(),
                "cpu_throttle": self.cgroup_statistics.dump_cpu_throttle_stats(),
                "memory": self.cgroup_statistics.dump_memory_stats(),
                "memory_failcnt": self.cgroup_statistics.dump_memory_failcnt_stats(),
                # "io": self.cgroup_statistics.dump_io_stats(),
            }
        }

        if output_file:
            self.__dump_json_to_file(statistics, output_file)
        else:
            print("Result of analysis:")
            print(json.dumps(statistics, indent=4, sort_keys=True))


# =======================================================================================================
# MAIN HELPERS
# =======================================================================================================


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    parser = argparse.ArgumentParser(
        description="Utility to post-process data recorded by 'cmonitor_collector' and extract min/max/mean/median/mode for CPU/MEMORY/IO measurements."
    )

    # Optional arguments
    # NOTE: we cannot add required=True to --input option otherwise it's impossible to invoke this tool with just --version
    parser.add_argument("-i", "--input", help="The JSON file to analyze.", default="")
    parser.add_argument("-o", "--output", help="The name of the output JSON file with statistics.", default="")
    parser.add_argument("-v", "--verbose", help="Be verbose.", action="store_true", default=False)
    parser.add_argument("-V", "--version", help="Print version and exit", action="store_true", default=False)
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    if args.version:
        print("{}".format(VERSION_STRING))
        sys.exit(0)

    if args.input == "":
        print("Please provide --input option to analyze some cmonitor_collector trace.")
        sys.exit(os.EX_USAGE)

    abs_input_json = args.input
    if not os.path.isabs(args.input):
        abs_input_json = os.path.join(os.getcwd(), args.input)

    abs_ouput_file = args.output
    if abs_ouput_file and not os.path.isabs(args.output):
        abs_ouput_file = os.path.join(os.getcwd(), args.output)

    return {"input_json": args.input, "output_file": args.output}


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    CmonitorStatistics().process(config["input_json"], config["output_file"])
