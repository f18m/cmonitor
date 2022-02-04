#!/usr/bin/env python3
"""
    Author: Francesco Montorsi
    Verify the behavior of CmonitorFilterEngine
"""

import pytest
from cmonitor_filter_engine import CmonitorFilterEngine
from cmonitor_loader import CmonitorCollectorJsonLoader
from cmonitor_version import CmonitorToolVersion
import dateutil.parser as datetime_parser

# fmt: off
test_list = [

    # run0
    {
        "input_file": "docker-collecting-docker-stats.json", 
        "task_name": "non_existent",

        "expected_survived_tasks": 0,
        "expected_removed_tasks": 895,

        # number of samples in the output should not change:
        "expected_num_samples": 180,
    },

    # run1
    {
        "input_file": "docker-collecting-docker-stats.json", 
        "task_name": "jemalloc_bg_thd",

        # the input JSON contains 179 samples having "jemalloc_bg_thd" recordings inside; since we are filtering out every other
        # process/thread, we expect to count now only 179 tasks over 180 samples:
        "expected_survived_tasks": 179,
        "expected_removed_tasks": 716,

        # number of samples in the output should not change:
        "expected_num_samples": 180,
    },

    # run2
    {
        "input_file": "docker-collecting-docker-stats.json", 
        "task_name": "jemall", # a subset of the full thread name is fine

        # the input JSON contains 179 samples having "jemalloc_bg_thd" recordings inside; since we are filtering out every other
        # process/thread, we expect to count now only 179 tasks over 180 samples:
        "expected_survived_tasks": 179,
        "expected_removed_tasks": 716,

        # number of samples in the output should not change:
        "expected_num_samples": 180,
    },

    # run3
    {
        "input_file": "docker-collecting-docker-stats.json", 
        "task_name": "bio_", # this string will match 3 threads of Redis: bio_close_file bio_aof_fsync bio_lazy_free

        # the input JSON contains 179 samples, each with 3 threads matching task name filter, so we expect 179*3
        "expected_survived_tasks": 179*3,
        "expected_removed_tasks": 358,

        # number of samples in the output should not change:
        "expected_num_samples": 180,
    },



    
]
# fmt: on


@pytest.mark.parametrize("testrun_idx", range(len(test_list)))
def test_outputCmonitorFilterEngineByTaskName(testrun_idx):
    global test_list
    testrun = test_list[testrun_idx]

    # load test input JSON
    json_data = CmonitorCollectorJsonLoader().load(testrun["input_file"], this_tool_version=CmonitorToolVersion().get(), be_verbose=True)

    # create a filter engine on it
    engine = CmonitorFilterEngine(json_data, be_verbose=True)

    # run TASKNAME-based filter
    num_removed = engine.filter_by_task_name(testrun["task_name"])
    assert num_removed == testrun["expected_removed_tasks"]

    # check if the number of samples is the expected one:
    actual_output = engine.get_filtered_data()
    # print(actual_output) -- just for debug
    assert len(actual_output["samples"]) == testrun["expected_num_samples"]
    assert sum([len(x["cgroup_tasks"]) for x in actual_output["samples"]]) == testrun["expected_survived_tasks"]
