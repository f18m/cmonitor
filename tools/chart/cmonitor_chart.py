#!/usr/bin/python3

#
# cmonitor_chart.py
# Originally based on the "njmonchart_aix_v7.py" from Nigel project: http://nmon.sourceforge.net/
#
# Author: Francesco Montorsi
# Created: April 2019
#

import sys
import json
import gzip
import datetime
import zlib
import binascii
import textwrap
import argparse
import getopt
import os
import time
from cmonitor_loader import CmonitorCollectorJsonLoader
from cmonitor_version import CmonitorToolVersion
from cmonitor_chart_engine import CMonitorGraphGenerator

# =======================================================================================================
# GLOBALs
# =======================================================================================================

verbose = False

# =======================================================================================================
# CLI options
# =======================================================================================================


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    parser = argparse.ArgumentParser(
        description="Utility to post-process data recorded by 'cmonitor_collector' and create a self-contained HTML file for visualizing that data."
    )

    # Optional arguments
    # NOTE: we cannot add required=True to --output option otherwise it's impossible to invoke this tool with just --version
    parser.add_argument(
        "-o", "--output", help="The name of the output HTML file. Defaults to the name of the input JSON with .html extension.", default=None
    )
    parser.add_argument("-t", "--top_scorer", help="Plot the N most-CPU-hungry processes/threads. Default is 20. Zero means plot all.", default=20)
    parser.add_argument("-u", "--utc", help="Plot data using UTC timestamps instead of local timezone", action="store_true", default=False)
    parser.add_argument("-v", "--verbose", help="Be verbose.", action="store_true", default=False)
    parser.add_argument("-V", "--version", help="Print version and exit", action="store_true", default=False)
    # NOTE: we use nargs='?' to make it possible to invoke this tool with just --version
    parser.add_argument("input", nargs="?", help="The JSON file to analyze. If '-' the JSON is read from stdin.", default=None)

    if "COLUMNS" not in os.environ:
        os.environ["COLUMNS"] = "120"  # avoid too many line wraps
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    # instead of default 'datetime' which means local timezone
    # FIXME: currently the presence/absence of --utc flag is ignored... we need to add code that reads from the JSON header the timezone offset
    #        and that applies the offset on top of the UTC timestamps
    datetime = "UTC"

    if args.version:
        CmonitorToolVersion().print()
        sys.exit(0)

    if args.input is None:
        print("Please provide the input file to process as positional argument")
        parser.print_help()
        sys.exit(os.EX_USAGE)

    if args.output is None:
        # create a good default value for output file
        if args.input[-8:] == ".json.gz":
            args.output = args.input[:-8] + ".html"
        elif args.input[-5:] == ".json":
            args.output = args.input[:-5] + ".html"
        elif args.input == "-":
            print("Please provide the output HTML filename with --output option when reading from stdin")
            parser.print_help()
            sys.exit(os.EX_USAGE)
        else:
            args.output = args.input + ".html"

    if args.input != "-" and not os.path.isabs(args.input):
        # take absolute path:
        args.input = os.path.join(os.getcwd(), args.input)

    if args.top_scorer:
        try:
            args.top_scorer = int(args.top_scorer)
        except:
            print(f"Please provide an integer number to --top_scorer option instead of {args.top_scorer}")
            sys.exit(os.EX_USAGE)

    return {
        "input_json": args.input,
        "output_html": args.output,
        "top_scorer": args.top_scorer,
    }


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    start_time = time.time()
    my_ver = CmonitorToolVersion().get()

    # load the JSON
    entry = CmonitorCollectorJsonLoader().load(config["input_json"], this_tool_version=my_ver, min_num_samples=2, be_verbose=verbose)
    jheader = entry["header"]
    jdata = entry["samples"]
    if verbose:
        print("Found %d data samples" % len(jdata))

    print("Opening output file [%s]" % config["output_html"])
    graph_generator = CMonitorGraphGenerator(config["output_html"], jheader, jdata, be_verbose=verbose)
    graph_generator.generate_html(config["top_scorer"], version=my_ver)

    end_time = time.time()
    print("Completed processing of input JSON file of %d samples in %.3fsec. HTML output file is ready." % (len(jdata), end_time - start_time))
