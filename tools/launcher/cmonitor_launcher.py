#!/usr/bin/python3

#
# cmonitor_launcher.py
#
# Author: Satyabrata Bharati
# Created: April 2022
#

import concurrent.futures
from concurrent.futures import ProcessPoolExecutor
import subprocess
from subprocess import Popen
import argparse
import queue
import os
import sys
import time
import logging
from datetime import datetime

from cmonitor_watcher import CgroupWatcher
from argparse import RawTextHelpFormatter

queue = queue.Queue()
logger = logging.getLogger(__name__)

# default sleep timeout
default_sleep_timeout = 20
# =======================================================================================================
# CmonitorLauncher
# =======================================================================================================
class CmonitorLauncher:
    """
    - Retrieves all the events from the Queue.
    - Lauch cMonitor with appropriate command.
    """

    def __init__(self, path, filter, ip, command, timeout):
        self.path = path
        self.filter = filter
        self.ip = ip
        self.command = command
        self.timeout = timeout
        self.process_host_dict = {}

        """
        Should add the list of IPs as key to the dictionary
        """
        tmp_ip = self.ip
        for key in self.filter:
            for value in tmp_ip:
                self.process_host_dict[key] = value
                tmp_ip.remove(value)
        # Printing resultant dictionary
        print("Input [filter-host]: " + str(self.process_host_dict))
        logging.info(f"Input [filter-host] {str(self.process_host_dict)}")

    def process_events(self, event):
        """Main thread function for processing input events from the queue.
        Args:
          event: events to read from this queue.
          The events from this queue will be processed by this threading function to
          launch cMonitor with appropriate command input.

        """
        try:
            entry = 1
            while True:
                if not event.empty():
                    fileList = event.get()
                    for key, value in fileList.items():
                        filename = key
                        process_name = value
                    logging.info(f"In processing event from the Queue - event: {entry},file: {filename},process_name: {process_name}")
                    logging.info(f"Launching cMonitor with: {filename} with IP :{self.process_host_dict.get(process_name)}")
                    self.__launch_cmonitor(filename, self.process_host_dict.get(process_name))
                    entry = entry + 1
                else:
                    # time.sleep(10)
                    time.sleep(self.timeout)
                    logging.info(f"In processing event Queue is empty - sleeping: {self.timeout} sec")
        except event.Empty():
            pass

    def __launch_cmonitor(self, filename, ip):
        """
        - Lauch cMonitor with appropriate command.
        """

        for c in self.command:
            cmd = c.strip()
            ip_port = ip.split(":")
            ip = ip_port[0]
            port = ip_port[1]
            base_path = os.path.dirname(filename)
            path = "/".join(base_path.split("/")[5:])
            cmd = f"{cmd} --cgroup-name={path} --remote-ip {ip} --remote-port {port}"
            print("Launch cMonitor with command:", cmd)
            logging.info(f"Launch cMonitor with command: {cmd}")
            # os.system(cmd)
            subprocess.run(cmd, shell=True)


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    parser = argparse.ArgumentParser(
        description="Utility to Lauch cMonitor with appropriate command.",
        epilog="""
Example:
         cmonitor_launcher.py --path /sys/fs/cgroup/memory/kubepods/burstable/ 
			      --filter process_1 process_2 
			      --ip-port 172.0.0.1:9090 172.0.0.2:9099 
			      --command "./cmonitor_collector --num-samples=until-cgroup-alive
                                        --deep-collect --collect=cgroup_threads,cgroup_cpu,cgroup_memory,cgroup_network
                                        --score-threshold=0  --sampling-interval=3 --output-directory=/home
                                        --allow-multiple-instances --remote prometheus"
			      --log /home
			      --timeout 20
""",
        formatter_class=RawTextHelpFormatter,
    )
    parser.add_argument("-p", "--path", help="path to watch", default=None)
    parser.add_argument(
        "-f",
        "--filter",
        nargs="*",
        help="cmonitor triggers for matching pattern",
        default=None,
    )
    parser.add_argument(
        "-c",
        "--command",
        nargs="*",
        help="cmonitor input command parameters",
        default=None,
    )
    parser.add_argument("-i", "--ip-port", nargs="*", help="cmonitor input <IP:PORT>", default=None)
    parser.add_argument("-l", "--log", help="path to logfile")
    parser.add_argument("-t", "--timeout", type=int, help="sleep time interval")
    args = parser.parse_args()

    if args.path is None:
        print("Please provide the input path to watch for iNotify events to be monitored")
        parser.print_help()
        sys.exit(os.EX_USAGE)

    if args.filter is None:
        print("Please provide the input filter for white-listing events")
        parser.print_help()
        sys.exit(os.EX_USAGE)

    if args.command is None:
        print("Please provide the input comamnd to launch cMonitor with")
        parser.print_help()
        sys.exit(os.EX_USAGE)

    if args.ip_port is None:
        print("Please provide the input ip:port to launch cMonitor")
        parser.print_help()
        sys.exit(os.EX_USAGE)

    return {
        "input_path": args.path,
        "input_filter": args.filter,
        "input_command": args.command,
        "input_ip": args.ip_port,
        "input_log": args.log,
        "input_timeout": args.timeout,
    }


def main():
    config = parse_command_line()
    # default sleep timeout
    timeout = default_sleep_timeout

    # command line inputs
    input_path = config["input_path"]
    print("Input [path]:", input_path)
    filter = config["input_filter"]
    command = config["input_command"]
    ip = config["input_ip"]
    log_dir = config["input_log"]
    timeout = config["input_timeout"]

    if log_dir:
        log_file_name = os.path.join(log_dir, datetime.now().strftime("cmonitor_launcher_%Y%m%d_%H%M%S.log"))
    else:
        log_file_name = datetime.now().strftime("cmonitor_launcher_%Y%m%d_%H%M%S.log")

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)-8s %(message)s",
        datefmt="%a, %d %b %Y %H:%M:%S",
        filename=log_file_name,
        filemode="w",
    )
    logging.info("Started")
    logging.info(f"timeout set for sleep: {timeout}")

    exit_flag = False

    cGroupWatcher = CgroupWatcher(input_path, filter, timeout)
    cMonitorLauncher = CmonitorLauncher(input_path, filter, ip, command, timeout)

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        executor.submit(cGroupWatcher.inotify_events, queue, exit_flag)
        executor.submit(cMonitorLauncher.process_events, queue)


if __name__ == "__main__":
    main()
