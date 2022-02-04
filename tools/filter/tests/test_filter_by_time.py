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
        "start_timestamp": "2022-01-18T00:02:50",
        "end_timestamp": "2022-01-18T00:05:47.300",

        # the input JSON contains 180 samples:
        #   first one: "2022-01-18T00:02:47.897"
        #   last one: "2022-01-18T00:05:47.313"
        # so with filtering criteria we remove 2 samples from the start and 1 from the end --> 177
        "expected_survived_samples": 177,
        "expected_removed_samples": 3,
    },

]
# fmt: on


@pytest.mark.parametrize("testrun_idx", range(len(test_list)))
def test_outputCmonitorFilterEngineByTime(testrun_idx):
    global test_list
    testrun = test_list[testrun_idx]

    # load test input JSON
    json_data = CmonitorCollectorJsonLoader().load(testrun["input_file"], this_tool_version=CmonitorToolVersion().get(), be_verbose=True)

    # create a filter engine on it
    engine = CmonitorFilterEngine(json_data, be_verbose=True)

    # run TIME-based filter
    testrun["start_timestamp"] = datetime_parser.parse(testrun["start_timestamp"])
    testrun["end_timestamp"] = datetime_parser.parse(testrun["end_timestamp"])
    num_removed = engine.filter_by_time(testrun["start_timestamp"], testrun["end_timestamp"])
    assert num_removed == testrun["expected_removed_samples"]

    # check if the number of samples is the expected one:
    actual_output = engine.get_filtered_data()
    # print(actual_output) -- just for debug
    assert len(actual_output["samples"]) == testrun["expected_survived_samples"]
