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
from cmonitor_filter_engine import CmonitorFilterEngine

# =======================================================================================================
# GLOBALs
# =======================================================================================================

verbose = False


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

    json_data = CmonitorCollectorJsonLoader().load(config["input_json"], this_tool_version=CmonitorToolVersion().get(), be_verbose=verbose)
    filter_engine = CmonitorFilterEngine(json_data, config["output_file"], be_verbose=verbose)

    if config["start_timestamp"] or config["end_timestamp"]:
        filter_engine.filter_by_time(config["start_timestamp"], config["end_timestamp"])
        filter_engine.write_output_file()
    elif config["task_name"]:
        filter_engine.filter_by_task_name(config["task_name"])
        filter_engine.write_output_file()
    else:
        print("Please provide at least one filter criteria using CLI options. Use --help for more info.")
        sys.exit(os.EX_USAGE)
