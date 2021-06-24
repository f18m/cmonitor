#!/usr/bin/python3

#
# cmonitor_statistics.py
#
# Author: Gajanan Khandake
# Created: April 2021
#

import getopt
import json
import os
import sys
from statistics import mean, median, mode, StatisticsError


# =======================================================================================================
# CONSTANTS
# =======================================================================================================

CMONITOR_VERSION = "1.6-0"

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

        def insert_cpu_stats(self, stats: dict) -> None:
            self.cpu.insert_stat(stats["cpu_tot"])

        def dump_cpu_stats(self) -> None:
            return self.cpu.dump_json()

        def insert_memory_stats(self, stats: dict) -> None:
            self.memory.insert_stat(stats["mem_rss_bytes"])

        def dump_memory_stats(self) -> None:
            return self.memory.dump_json()

        def insert_io_stats(self, stats: dict) -> None:
            self.io.insert_stat(stats["io_rchar"] + stats["io_wchar"])

        def dump_io_stats(self) -> None:
            return self.io.dump_json()

    def __init__(self) -> None:
        self.cgroup_statistics = self.CgroupTasksStatistics()
        pass

    def process(self, input_json: str, output_file: str) -> None:
        with open(input_json) as file:
            json_data = json.load(file)
            for sample in json_data["samples"]:
                for pid, stats in sample["cgroup_tasks"].items():
                    self.cgroup_statistics.insert_cpu_stats(stats)
                    self.cgroup_statistics.insert_memory_stats(stats)
                    self.cgroup_statistics.insert_io_stats(stats)

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
                "memory": self.cgroup_statistics.dump_memory_stats(),
                "io": self.cgroup_statistics.dump_io_stats(),
            }
        }

        if output_file:
            self.__dump_json_to_file(statistics, output_file)
        else:
            print(json.dumps(statistics))


# =======================================================================================================
# MAIN HELPERS
# =======================================================================================================


def usage():
    """Provides commandline usage"""
    print("cmonitor_statistics version {}".format(CMONITOR_VERSION))
    print("Typical usage:")
    print(
        "  %s --input=output_from_cmonitor_collector.json [--output=myreport.log]"
        % sys.argv[0]
    )
    print("Required parameters:")
    print("  -i, --input=<file.json>    The JSON file to analyze.")
    print("Main options:")
    print("  -h, --help                 (this help)")
    print("  -v, --verbose              Be verbose.")
    print("      --version              Print version and exit.")
    print("  -o, --output=<file.log>   The name of the output log file.")
    sys.exit(0)


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    try:
        opts, remaining_args = getopt.getopt(
            sys.argv[1:], "hvv", ["help", "verbose", "version", "output=", "input="]
        )
    except getopt.GetoptError as err:
        # print help information and exit:
        print(str(err))  # will print something like "option -a not recognized"
        usage()  # will exit program

    global verbose
    input_json = ""
    output_file = ""
    for o, a in opts:
        if o in ("-i", "--input"):
            input_json = a
        elif o in ("-o", "--output"):
            output_file = a
        elif o in ("-h", "--help"):
            usage()
        elif o in ("-v", "--verbose"):
            verbose = True
        elif o in ("--version"):
            print("{}".format(CMONITOR_VERSION))
            sys.exit(0)
        else:
            assert False, "unhandled option " + o + a

    if input_json == "":
        print("Please provide --input option (it is a required option)")
        sys.exit(os.EX_USAGE)

    abs_input_json = input_json
    if not os.path.isabs(input_json):
        abs_input_json = os.path.join(os.getcwd(), input_json)

    abs_ouput_file = output_file
    if abs_ouput_file and not os.path.isabs(output_file):
        abs_ouput_file = os.path.join(os.getcwd(), output_file)

    return {"input_json": abs_input_json, "output_file": abs_ouput_file}


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    CmonitorStatistics().process(config["input_json"], config["output_file"])
