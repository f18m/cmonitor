#!/usr/bin/python3

#
# cmonitor_filter.py
#
# Author: Francesco Montorsi
# Created: January 2022
#

import argparse
import json
import os
import sys
import datetime
from cmonitor_loader import CmonitorCollectorJsonLoader
from cmonitor_version import VERSION_STRING

# =======================================================================================================
# GLOBALs
# =======================================================================================================

verbose = False

# =======================================================================================================
# CLASS
# =======================================================================================================
class CmonitorFilter:
    def __init__(self, input_file, output_file) -> None:
        self.input_file = input_file
        self.output_file = output_file

    def filter_by_time(self, start_timestamp=None, end_timestamp=None):
        json_data = CmonitorCollectorJsonLoader().load(self.input_file, this_tool_version=VERSION_STRING)

        for sample in json_data["samples"]:
            sample_datetime = datetime.strptime(sample["timestamp"]["UTC"], "%Y-%m-%dT%H:%M:%S")
            if not (start_timestamp <= sample_datetime <= end_timestamp):
                json_data["samples"].remove(sample)

        dest_dir = os.path.dirname(self.output_file)
        if not os.path.exists(dest_dir):
            os.makedirs(dest_dir)
        with open(self.output_file, "w") as f:
            json.dump(json_data, f)


# =======================================================================================================
# MAIN HELPERS
# =======================================================================================================


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    parser = argparse.ArgumentParser(
        description="Utility to post-process data recorded by 'cmonitor_collector' and filter out samples based on different criteria."
    )

    # Optional arguments
    # NOTE: we cannot add required=True to --input option otherwise it's impossible to invoke this tool with just --version
    parser.add_argument("-i", "--input", help="The JSON file to analyze.", default="")
    parser.add_argument("-o", "--output", help="The name of the output filtered JSON file.", default="")
    parser.add_argument("--start_timestamp", help="Filter out any sample recorded before the timestamp provided with this option.", default="")
    parser.add_argument("--end_timestamp", help="Filter out any sample recorded after the timestamp provided with this option.", default="")
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

    # uniform time format to Trafgen datetime
    sample_start_datetime = pytz.utc.localize(sample_start_datetime)

    return {"input_json": args.input, "output_file": args.output, "start_timestamp": args.start_timestamp, "end_timestamp": args.end_timestamp}


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    CmonitorFilter(config["input_json"], config["output_file"]).filter_by_time(config["start_timestamp"], config["end_timestamp"])
