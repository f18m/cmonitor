#
# cmonitor_filter_engine.py
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

# =======================================================================================================
# CmonitorFilterEngine
# =======================================================================================================
class CmonitorFilterEngine:
    def __init__(self, json_data, be_verbose=False) -> None:
        self.json_data = json_data
        self.verbose = be_verbose

    def get_filtered_data(self):
        return self.json_data

    def write_output_file(self, output_file):
        """
        Write the filtered JSON to a file or stdout
        """
        n_samples = len(self.json_data["samples"])
        if output_file:  # user has provided an output file... dump on disk:
            dest_dir = os.path.dirname(output_file)
            if not os.path.exists(dest_dir):
                os.makedirs(dest_dir)
            with open(output_file, "w") as f:
                json.dump(self.json_data, f)
            if self.verbose:
                print(f"Wrote {n_samples} samples into {output_file}")
        else:  # write on stdout
            print(json.dumps(self.json_data))
            if self.verbose:
                print(f"Wrote {n_samples} samples on standard output")

    def filter_by_time(self, start_timestamp=None, end_timestamp=None) -> int:
        """
        Filter samples outside the given interval.
        One of the two timestamps (start or end) can be None to indicate no filtering should
        be done on the start or end time.
        Returns the number of samples FILTERED OUT.
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
            if self.verbose:
                print(f"Filtering samples by start and end timestamp [{start_timestamp}]-[{end_timestamp}]. Removed {n_removed_samples} samples.")
        elif start_timestamp:
            for sample in self.json_data["samples"]:
                _filter_only_by_starttime(sample)
            if self.verbose:
                print(f"Filtering samples by start timestamp [{start_timestamp}]. Removed {n_removed_samples} samples.")
        elif end_timestamp:
            for sample in self.json_data["samples"]:
                _filter_only_by_endtime(sample)
            if self.verbose:
                print(f"Filtering samples by end timestamp [{end_timestamp}]. Removed {n_removed_samples} samples.")
        else:
            assert False

        return n_removed_samples

    def filter_by_task_name(self, task_name: str) -> int:
        """
        Filter tasks by given name.
        Returns the number of tasks FILTERED OUT.
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

        if self.verbose:
            print(f"Filtering samples by task name [{task_name}]. Removed {n_removed_tasks} tasks.")

        return n_removed_tasks
