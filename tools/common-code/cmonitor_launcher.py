#
# cmonitor_launcher.py
#
# Author: Satyabrata Bharati
# Created: April 2022
#

import inotify.adapters
import multiprocessing
from threading import Thread, Lock
from queue import Queue
from subprocess import Popen
import os, fnmatch
import time
import logging


class CmonitorLauncher:
    def __init__(self):
        self.path = ""
        self.filter = ""
        self.command = ""

    def load(self, input_path, filter, command):
        self.path = input_path
        self.filter = filter
        self.command = command

    def get_cgroup_version(self):
        proc_self_mount = "/proc/self/mounts"
        ncgroup_v1 = 0
        ncgroup_v2 = 0
        with open(proc_self_mount) as file:
            for line in file:
                row = line.split()
                fs_spec = row[0]
                fs_file = row[1]
                fs_vfstype = row[2]
                if (
                    fs_spec == "cgroup" or fs_spec == "cgroup2"
                ) and fs_vfstype == "cgroup2":
                    ncgroup_v2 += 1
                else:
                    ncgroup_v1 += 1

        if ncgroup_v1 == 0 and ncgroup_v2 > 0:
            cgroup_versopn = "v2"
            return cgroup_version
        else:
            cgroup_version = "v1"
            return cgroup_version

    def get_process_name(self, pid):
        cgroup_version = self.get_cgroup_version()
        if cgroup_version == "v1":
            proc_filename = "/proc" + "/" + pid + "/stat"
        else:
            proc_filename = "/proc" + "/" + pid + "/cgroup.procs"
        with open(proc_filename) as file:
            for line in file:
                parts = line.split()
                process_name = parts[1].strip("()")
        return process_name

    def get_pid_list(self, filename):
        list = []
        with open(filename) as file:
            for line in file:
                list.append(line.strip())
        return list

    def get_list_of_files(self, dir):
        listOfFiles = os.listdir(dir)
        allFiles = list()
        for entry in listOfFiles:
            fullpath = os.path.join(dir, entry)
            if os.path.isdir(fullpath):
                allFiles = allFiles + self.get_list_of_files(fullpath)
            else:
                allFiles.append(fullpath)

        return allFiles

    def process_task_files(self, dir):
        time.sleep(5)
        allFiles = self.get_list_of_files(dir)
        for file in allFiles:
            if file.endswith("tasks"):
                list = self.get_pid_list(file)
                if list:
                    for pid in list:
                        process_name = self.get_process_name(pid)
                        match = self.check_filter(process_name)
                        if match is True:
                            print("found match:", process_name)
                            print("launchig cmonitor", process_name, self.filter)
                            self.launch_cmonitor(file)

    def launch_cmonitor(self, filename):
        for c in self.command:
            cmd = c.strip()
            base_path = os.path.dirname(filename)
            path = "/".join(base_path.split("/")[5:])
            print("path:", path)
            cmd = cmd + " " + "--cgroup-name=" + path
            print("Launch CMonitor : After update command:", cmd)
            # p = Popen(cmd)
            # print("process id:",p.pid)  #FIXME : need to delete the current cmonitor already running fot this process..!!!
            os.system(cmd)

    def check_filter(self, process_name):
        for e in self.filter:
            if process_name in e:
                return True

    def inotify_events(self, input_path, queue):
        lock = multiprocessing.Lock()
        i = inotify.adapters.Inotify()
        print("path:", input_path)
        i.add_watch(input_path)
        try:
            for event in i.event_gen():
                if event is not None:
                    if "IN_CREATE" in event[1]:
                        (header, type_names, path, filename) = event
                        print(header, type_names, path, filename)
                        lock.acquire()
                        dir = path + filename
                        queue.put(dir)
                        lock.release()
        finally:
            i.remove_watch(path)

    def process_events(self, event, filter, command):
        self.filter = filter
        self.command = command
        lock = multiprocessing.Lock()
        entry = 1
        while True:
            lock.acquire()
            filename = event.get()
            print(" in process events entry:", entry)
            print(" in process events", filename)
            time.sleep(50)
            self.process_task_files(filename)
            entry = entry + 1
            lock.release()


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Processsome integers.")
    parser.add_argument("-p", "--path", help="path to watch")
    parser.add_argument(
        "-filter",
        "--filter",
        nargs="*",
        help="cmonitor triggers for matching pattern",
    )
    parser.add_argument(
        "-command",
        "--command",
        nargs="*",
        help="cmonitor triggers for matching pattern",
    )
    args = parser.parse_args()
    input_path = args.path
    print("input path:", input_path)
    filter = args.filter
    command = args.command

    CmonitorLauncher().load(input_path, filter, command)

    queue = multiprocessing.Queue()
    process_1 = multiprocessing.Process(
        target=CmonitorLauncher().inotify_events, args=(input_path, queue)
    )
    process_2 = multiprocessing.Process(
        target=CmonitorLauncher().process_events, args=(queue, filter, command)
    )

    process_1.start()
    process_2.start()

    process_1.join()
    process_2.join()


if __name__ == "__main__":
    main()
