#!/usr/bin/env python3
"""
    Author: Satyabrata Bharati
    Verify the behavior of CmonitorWatcher InotifyEvent
"""

import pytest
import queue
import os
import time
import sys
import subprocess
from subprocess import Popen
import concurrent.futures
from concurrent.futures import ProcessPoolExecutor

# import cmonitor_watcher
from cmonitor_watcher import CgroupWatcher

# import dateutil.parser as datetime_parser

queue = queue.Queue()
myDict = {}

test_list = [
    # run0
    {
        "expected_task_file": "/tmp/unit_test_cmonitor/cgroup_memory_kubepods/task/tasks",
        "expected_process_name": "python3",
    },
]


def create_task_file(path, pid):
    cmd1 = f"rm -rf {path}/task"
    os.system(cmd1)

    cmd2 = f"mkdir {path}/task"
    os.system(cmd2)

    # create the task file with the process id of the dummy process
    # /tmp/unit_test_cmonitor/task/tasks
    cmd3 = f"cd {path}/task;rm -rf tasks;echo {pid} >> tasks"
    os.system(cmd3)

    filename = os.path.join(path, "task/tasks")
    return filename


def process_task_file(path, queue):
    # create the dummy process
    cmd = "python3 -c 'time.sleep(5)'"
    p = Popen(cmd.split())
    # process id of the dummy process
    pid = p.pid

    filename = create_task_file(path, pid)
    print(f"Created: task file {filename} with process Id: {pid}")

    d = queue.get()
    for k, v in d.items():
        # print(k, v)
        process_name = v
        print(f"Read from Queue filename :{k}, process_name {process_name}")

    # store the task file and the process name in the dictionary
    global myDict
    myDict = d.copy()
    if queue.empty():
        print("Queue is Empty")
        # terminate the dummy process
        p.terminate()
        return


@pytest.mark.parametrize("testrun_idx", range(len(test_list)))
def test_outputCmonitorWatcherInotifyEvent(testrun_idx):
    global test_list
    testrun = test_list[testrun_idx]

    path = "/tmp/unit_test_cmonitor/cgroup_memory_kubepods/"
    filter = ["python3"]
    if not os.path.exists(path):
        #os.mkdir(path)
        os.makedirs(path)
        print("Directory '% s' created" % path)

    watcher = CgroupWatcher(path, filter, 10)
    flag = True
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        future1 = executor.submit(watcher.inotify_events, queue, flag)
        future2 = executor.submit(process_task_file, path, queue)

    # both threads completely executed
    print("Done!")

    for k, v in myDict.items():
        # print(k, v)
        assert k == testrun["expected_task_file"]
        assert v == testrun["expected_process_name"]
