#!/usr/bin/python3

#
# cmonitor_filter.py
#
# Author: Francesco Montorsi, Marco Zizzi
# Created: January 2022
#

import argparse
import json
import os
import sys
import datetime

# this introduces as dependency the "python-dateutil" package >= 2.7.0
# this is better than using datetime.fromisoformat() which introduces as dependency Python >= 3.7
# which is not available on Centos7
import dateutil.parser as datetime_parser

from cmonitor_loader import CmonitorCollectorJsonLoader
from cmonitor_version import CmonitorToolVersion

# =======================================================================================================
# GLOBALs
# =======================================================================================================

verbose = False

# =======================================================================================================
# CLASS
# =======================================================================================================
class CmonitorFilter:
    def __init__(self, input_file, output_file) -> None:
        global verbose
        self.input_file = input_file
        self.output_file = output_file
        self.json_data = CmonitorCollectorJsonLoader().load(self.input_file, this_tool_version=CmonitorToolVersion().get(), be_verbose=verbose)

    def __write_output_file(self):
        global verbose

        n_samples = len(self.json_data["samples"])
        if self.output_file:  # use has provided an output file... dump on disk:
            dest_dir = os.path.dirname(self.output_file)
            if not os.path.exists(dest_dir):
                os.makedirs(dest_dir)
            with open(self.output_file, "w") as f:
                json.dump(self.json_data, f)
            if verbose:
                print(f"Wrote {n_samples} samples into {self.output_file}")
        else:
            print(json.dumps(self.json_data))
            if verbose:
                print(f"Wrote {n_samples} samples on standard output")

    def filter_by_time(self, start_timestamp=None, end_timestamp=None):
        """
        Filter samples outside the given interval.
        One of the two timestamps can be None.
        """

        assert start_timestamp is None or isinstance(start_timestamp, datetime.datetime)
        assert end_timestamp is None or isinstance(end_timestamp, datetime.datetime)

        n_removed_samples = 0

        def _filter_by_both_starttime_endtime(sample):
            nonlocal n_removed_samples
            # convert from string to datetime object:
            sample_datetime = datetime.datetime.strptime(sample["timestamp"]["UTC"], "%Y-%m-%dT%H:%M:%S.%f")
            # filter:
            if not (start_timestamp <= sample_datetime <= end_timestamp):
                self.json_data["samples"].remove(sample)
                n_removed_samples += 1

        def _filter_only_by_starttime(sample):
            nonlocal n_removed_samples
            # convert from string to datetime object:
            sample_datetime = datetime.datetime.strptime(sample["timestamp"]["UTC"], "%Y-%m-%dT%H:%M:%S.%f")
            print(sample_datetime)
            # filter:
            if not (start_timestamp <= sample_datetime):
                self.json_data["samples"].remove(sample)
                n_removed_samples += 1

        def _filter_only_by_endtime(sample):
            nonlocal n_removed_samples
            # convert from string to datetime object:
            sample_datetime = datetime.datetime.strptime(sample["timestamp"]["UTC"], "%Y-%m-%dT%H:%M:%S.%f")
            # filter:
            if not (sample_datetime <= end_timestamp):
                self.json_data["samples"].remove(sample)
                n_removed_samples += 1

        if start_timestamp and end_timestamp:
            for sample in self.json_data["samples"]:
                _filter_by_both_starttime_endtime(sample)
            if verbose:
                print(f"Filtering samples by start and end timestamp [{start_timestamp}-{end_timestamp}]. Removed {n_removed_samples} samples.")
        elif start_timestamp:
            for sample in self.json_data["samples"]:
                _filter_only_by_starttime(sample)
            if verbose:
                print(f"Filtering samples by start timestamp [{start_timestamp}]. Removed {n_removed_samples} samples.")
        elif end_timestamp:
            for sample in self.json_data["samples"]:
                _filter_only_by_endtime(sample)
            if verbose:
                print(f"Filtering samples by end timestamp [{end_timestamp}]. Removed {n_removed_samples} samples.")
        else:
            assert False

        self.__write_output_file()

    def filter_by_task_name(self, task_name: str):
        """
        Filter tasks by given name
        """

        # we cannot iterate over a list on which we're
        # original_data =

        samples_copy = self.json_data["samples"].copy()
        n_removed_tasks = 0

        for sample_idx, sample in enumerate(samples_copy):
            if "cgroup_tasks" in sample:
                for pid_sample in sample["cgroup_tasks"].copy():
                    if task_name not in sample["cgroup_tasks"][pid_sample]["cmd"]:
                        del self.json_data["samples"][sample_idx]["cgroup_tasks"][pid_sample]
                        n_removed_tasks += 1

        if verbose:
            print(f"Filtering samples by task name [{task_name}]. Removed {n_removed_tasks} tasks.")
        self.__write_output_file()


# =======================================================================================================
# MAIN HELPERS
# =======================================================================================================

from argparse import RawTextHelpFormatter


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    parser = argparse.ArgumentParser(
        description="Utility to post-process data recorded by 'cmonitor_collector' and filter out samples based on different criteria.",
        epilog="""
Examples:

* Filter a recording by task name and create an HTML chart out of it
    cmonitor_filter --input=cmonitor_collector_trace.json --task_name=myApp | cmonitor_chart --output=cmonitor_collector_trace.html -

* Filter a recording by start time and get some statistics from remaining samples
    cmonitor_filter --input=cmonitor_collector_trace.json --start_timestamp="2022-01-18T00:02:50" | cmonitor_statistics --output=cmonitor_collector_trace.json -

""",
        formatter_class=RawTextHelpFormatter,
    )

    # Optional arguments
    # NOTE: we cannot add required=True to --output option otherwise it's impossible to invoke this tool with just --version
    parser.add_argument(
        "-o", "--output", help="The name of the output filtered JSON file. If not provided the filtered JSON is printed on stdout.", default=None
    )
    parser.add_argument("--start_timestamp", help="Output only samples recorded AFTER the provided UTC start timestamp.", default=None)
    parser.add_argument("--end_timestamp", help="Output only samples recorded BEFORE the provided UTC end timestamp.", default=None)
    parser.add_argument(
        "--task_name", help="Output only samples related to processes/threads (tasks) whose name contains the TASK_NAME string.", default=None
    )
    parser.add_argument("-v", "--verbose", help="Be verbose.", action="store_true", default=False)
    parser.add_argument("-V", "--version", help="Print version and exit", action="store_true", default=False)
    # NOTE: we use nargs='?' to make it possible to invoke this tool with just --version
    parser.add_argument("input", nargs="?", help="The JSON file to analyze.", default=None)

    if "COLUMNS" not in os.environ:
        os.environ["COLUMNS"] = "120"  # avoid too many line wraps
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    if args.version:
        CmonitorToolVersion().print()
        sys.exit(0)

    if args.input is None:
        print("Please provide the input file to process as positional argument")
        parser.print_help()
        sys.exit(os.EX_USAGE)

    if args.input != "-" and not os.path.isabs(args.input):
        # take absolute path:
        args.input = os.path.join(os.getcwd(), args.input)

    if args.output is not None and not os.path.isabs(args.output):
        # take absolute path:
        args.output = os.path.join(os.getcwd(), args.output)
    # else: will send output to stdout

    if args.start_timestamp:
        try:
            args.start_timestamp = datetime_parser.parse(args.start_timestamp)
        except:
            print("Please provide an ISO-8601 datetime string to --start_timestamp option.")
            sys.exit(os.EX_USAGE)
    if args.end_timestamp:
        try:
            args.end_timestamp = datetime_parser.parse(args.end_timestamp)
        except:
            print("Please provide an ISO-8601 datetime string to --end_timestamp option.")
            sys.exit(os.EX_USAGE)

    return {
        "input_json": args.input,
        "output_file": args.output,
        "start_timestamp": args.start_timestamp,
        "end_timestamp": args.end_timestamp,
        "task_name": args.task_name,
    }


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    filter_engine = CmonitorFilter(config["input_json"], config["output_file"])

    if config["start_timestamp"] or config["end_timestamp"]:
        filter_engine.filter_by_time(config["start_timestamp"], config["end_timestamp"])
    elif config["task_name"]:
        filter_engine.filter_by_task_name(config["task_name"])
    else:
        print("Please provide at least one filter criteria using CLI options. Use --help for more info.")
        sys.exit(os.EX_USAGE)
