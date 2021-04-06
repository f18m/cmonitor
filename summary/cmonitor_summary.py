#!/usr/bin/python3

#
# cmonitor_summary.py
#
# Author: Gajanan Khandake
# Created: April 2021
#

import getopt
import json
import os
import sys


# =======================================================================================================
# CONSTANTS
# =======================================================================================================

CMONITOR_VERSION = "1.4-4"

# =======================================================================================================
# CLASS
# =======================================================================================================
class CmonitorSummary:
    class Summary:
        def __init__(self) -> None:
            self.__min = sys.maxsize
            self.__max = 0
            self.__avg = 0.0
            self.__total = 0
            self.__samples = 0

        @property
        def min(self) -> int:
            return self.__min

        @min.setter
        def min(self, value: int) -> None:
            self.__min = value

        @property
        def max(self) -> int:
            return self.__max

        @max.setter
        def max(self, value: int) -> None:
            self.__max = value

        @property
        def average(self) -> int:
            return self.__avg

        @average.setter
        def average(self, value: int) -> None:
            self.__total += value
            self.__samples += 1
            self.__avg = self.__total / self.__samples

        def dump_json(self) -> dict:
            return {
                "min": self.__min,
                "max": self.__max,
                "avg": self.__avg,
            }

    class CgroupTasksSummary:
        def __init__(self) -> None:
            self.cpu = CmonitorSummary.Summary()
            self.memory = CmonitorSummary.Summary()
            self.io = CmonitorSummary.Summary()

        def add_cpu_stats(self, stats: dict) -> None:
            if stats["cpu_tot"] < self.cpu.min:
                self.cpu.min = stats["cpu_tot"]

            if stats["cpu_tot"] > self.cpu.max:
                self.cpu.max = stats["cpu_tot"]

            self.cpu.average = stats["cpu_tot"]

        def dump_cpu_stats(self) -> None:
            return self.cpu.dump_json()

        def add_memory_stats(self, stats: dict) -> None:
            if stats["mem_rss_bytes"] < self.memory.min:
                self.memory.min = stats["mem_rss_bytes"]

            if stats["mem_rss_bytes"] > self.memory.max:
                self.memory.max = stats["mem_rss_bytes"]

            self.memory.average = stats["mem_rss_bytes"]

        def dump_memory_stats(self) -> None:
            return self.memory.dump_json()

        def add_io_stats(self, stats: dict) -> None:
            total_io = stats["io_rchar"] + stats["io_wchar"]
            if total_io < self.io.min:
                self.io.min = total_io

            if total_io > self.io.max:
                self.io.max = total_io

            self.io.average = total_io

        def dump_io_stats(self) -> None:
            return self.io.dump_json()

    def __init__(self) -> None:
        self.cgroup_summary = self.CgroupTasksSummary()
        pass

    def process(self, input_json: str, output_log: str) -> None:
        with open(input_json) as file:
            json_data = json.load(file)
            for sample in json_data["samples"]:
                for pid, stats in sample["cgroup_tasks"].items():
                    self.cgroup_summary.add_cpu_stats(stats)
                    self.cgroup_summary.add_memory_stats(stats)
                    self.cgroup_summary.add_io_stats(stats)

            self.dump_summary_json()

    def __dump_json_to_file(
        self,
        summary: dict,
        outfile: str,
    ) -> None:
        pass

    def __dump_json_to_stdout(self, summary: dict) -> None:
        print(summary)

    def dump_summary_json(self, output_log="") -> None:
        summary = {
            "summary": {
                "cpu": self.cgroup_summary.dump_cpu_stats(),
                "memory": self.cgroup_summary.dump_memory_stats(),
                "io": self.cgroup_summary.dump_io_stats(),
            }
        }

        if output_log:
            self.__dump_json_to_file(summary, output_log)
        else:
            self.__dump_json_to_stdout(summary)


# =======================================================================================================
# MAIN HELPERS
# =======================================================================================================


def usage():
    """Provides commandline usage"""
    print("cmonitor_summary version {}".format(CMONITOR_VERSION))
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
    output_log = ""
    for o, a in opts:
        if o in ("-i", "--input"):
            input_json = a
        elif o in ("-o", "--output"):
            output_log = a
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

    return {"input_json": input_json, "output_log": output_log}


# =======================================================================================================
# MAIN HELPERS
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    CmonitorSummary().process(config["input_json"], config["output_log"])
