#!/usr/bin/env python3
"""
    Author: Francesco Montorsi
    Verify the behavior of CmonitorStatisticsEngine
"""

import pytest, sys
from cmonitor_statistics_engine import CmonitorStatisticsEngine
from cmonitor_loader import CmonitorCollectorJsonLoader
from cmonitor_version import CmonitorToolVersion

# fmt: off
test_list = [

    # run0
    {
        "input_file": "docker-collecting-docker-stats.json", 
        "expected_process_success": True,
        "expected_num_samples": 179, # number of samples in the JSON - 1 (first one is skipped)
        "expected_cpu": {
            'minimum': 0.118,
            'maximum': 0.196,
            'mean': 0.16802234636871508,
            'median': 0.169,
            'mode': 0.168,
            'unit': '%'
        },
        "expected_cpu_throttle": {
            'minimum': 0.0,
            'maximum': 0.0,
            'mean': 0.0,
            'median': 0.0,
            'mode': 0.0,
            'unit': '%'
        },
        "expected_memory": {
            'minimum': 2334720, 
            'maximum': 2334720, 
            'mean': 2334720, 
            'median': 2334720, 
            'mode': 2334720, 
            'unit': 'bytes'
        },
        "expected_memory_failcnt": {
            'minimum': 0, 
            'maximum': 0, 
            'mean': 0, 
            'median': 0, 
            'mode': 0, 
            'unit': ''
        }
    },

    # run1
    {
        "input_file": "toosmall.json", 
        "expected_process_success": False,
        "expected_num_samples": 0, # number of samples in the JSON - 1 (first one is skipped)
    },
]
# fmt: on


@pytest.mark.parametrize("testrun_idx", range(len(test_list)))
def test_outputCmonitorStatisticsEngine(testrun_idx):
    global test_list
    testrun = test_list[testrun_idx]

    # IMPORTANT: Python difference:
    if sys.version_info[0] < 3:
        raise Exception("Must be using Python 3")
    if sys.version_info[1] < 8:
        if testrun_idx == 0:
            # as documented https://docs.python.org/3/library/statistics.html#statistics.mode there's a different
            # implementation for mode() starting from Python 3.8 -- older Python versions produce "no unique mode"
            testrun["expected_cpu"]["mode"] = "no unique mode"

    # load test input JSON
    json_data = CmonitorCollectorJsonLoader().load(testrun["input_file"], this_tool_version=CmonitorToolVersion().get(), be_verbose=True)

    # run the statistics engine on it
    engine = CmonitorStatisticsEngine()

    # verify result
    actual_success = engine.process(json_data)
    assert actual_success == testrun["expected_process_success"]

    if actual_success:
        # validate the dictionary produced
        actual_output = engine.get_statistics_dict()
        # print(actual_output) -- just for debug
        assert actual_output["num_samples_analyzed"] == testrun["expected_num_samples"]
        assert actual_output["statistics"]["cpu"] == testrun["expected_cpu"]
        assert actual_output["statistics"]["cpu_throttle"] == testrun["expected_cpu_throttle"]
        assert actual_output["statistics"]["memory"] == testrun["expected_memory"]
        assert actual_output["statistics"]["memory_failcnt"] == testrun["expected_memory_failcnt"]
