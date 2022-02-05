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
from cmonitor_version import CmonitorToolVersion
from cmonitor_statistics_engine import CmonitorStatisticsEngine

# =======================================================================================================
# GLOBALs
# =======================================================================================================

verbose = False

# =======================================================================================================
# MAIN HELPERS
# =======================================================================================================


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    parser = argparse.ArgumentParser(
        description="Utility to post-process data recorded by 'cmonitor_collector' and extract min/max/mean/median/mode for CPU/MEMORY/IO measurements."
    )

    # Optional arguments
    # NOTE: we cannot add required=True to --output option otherwise it's impossible to invoke this tool with just --version
    parser.add_argument("-o", "--output", help="The name of the output JSON file with statistics.", default=None)
    parser.add_argument("-v", "--verbose", help="Be verbose.", action="store_true", default=False)
    parser.add_argument("-V", "--version", help="Print version and exit", action="store_true", default=False)
    # NOTE: we use nargs='?' to make it possible to invoke this tool with just --version
    parser.add_argument("input", nargs="?", help="The JSON file to analyze. If '-' the JSON is read from stdin.", default=None)

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
        # take absolute path
        args.input = os.path.join(os.getcwd(), args.input)

    if args.output is not None and not os.path.isabs(args.output):
        # take absolute path
        args.output = os.path.join(os.getcwd(), args.output)

    return {"input_json": args.input, "output_file": args.output}


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    json_data = CmonitorCollectorJsonLoader().load(config["input_json"], this_tool_version=CmonitorToolVersion().get(), be_verbose=verbose)
    engine = CmonitorStatisticsEngine()
    if not engine.process(json_data):
        sys.exit(1)
    engine.dump_statistics_json(config["output_file"])
