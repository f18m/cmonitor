# =======================================================================================================
# cmonitor_launcher.py
#
# Author: Satyabrata Bharati
# Created: April 2022
#
# =======================================================================================================

import inotify.adapters
import concurrent.futures
from concurrent.futures import ProcessPoolExecutor
from subprocess import Popen
import queue
import os
import time

queue = queue.Queue()
# =======================================================================================================
# CmonitorLauncher
#
#  - Watch all files below a directory and notify an event for changes.
#  - Retrieves all the process and extract the process name "/proc/<pid>/stat.
#  - check the process name against the white-list given in the filter list.
#  - Execute command to launch CMonitor if the process name matches with the filter.
#
# =======================================================================================================
class CmonitorLauncher:
    def __init__(self,path, filter, ip , command):
        self.path = path
        self.filter = filter
        self.ip = ip
        self.command = command
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
        print("Input [filter-host]: " +  str(self.process_host_dict))

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
                            print("Found match:", process_name)
                            self.ip = self.process_host_dict.get(process_name)
                            self.launch_cmonitor(file, self.ip)


    def launch_cmonitor(self, filename, ip):
        for c in self.command:
            cmd = c.strip()
            ip_port = ip.split(":");
            ip = ip_port[0]
            port  = ip_port[1]
            base_path = os.path.dirname(filename)
            path = "/".join(base_path.split("/")[5:])
            cmd = f"{cmd} --cgroup-name={path} -A {ip} -S {port}"
            print("Launch cMonitor with command:", cmd)
            os.system(cmd)

    def check_filter(self, process_name):
        for e in self.filter:
            if process_name in e:
                return True

    def inotify_events(self, queue):
        i = inotify.adapters.Inotify()
        i.add_watch(self.path)
        try:
            for event in i.event_gen():
                if event is not None:
                    if "IN_CREATE" in event[1]:
                        (header, type_names, path, filename) = event
                        # print(header, type_names, path, filename)
                        dir = path + filename
                        queue.put(dir)
        finally:
            i.remove_watch(path)

    def process_events(self, event):
        entry = 1
        while True:
            filename = event.get()
            print("In process events entry:", entry, filename)
            # print("In process events", filename)
            time.sleep(50)
            self.process_task_files(filename)
            entry = entry + 1


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Processsome integers.")
    parser.add_argument("-p", "--path", help="path to watch")
    parser.add_argument(
        "-f",
        "--filter",
        nargs="*",
        help="cmonitor triggers for matching pattern",
    )
    parser.add_argument(
        "-c",
        "--command",
        nargs="*",
        help="cmonitor input command parameters",
    )
    parser.add_argument(
        "-i",
        "--ip-port",
        nargs="*",
        help="cmonitor input <IP:PORT>",
    )
    args = parser.parse_args()
    input_path = args.path
    print("Input [path]:", input_path)
    filter = args.filter
    command = args.command
    ip = args.ip_port

    cMonitorLauncher = CmonitorLauncher(input_path, filter, ip , command)

    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
       executor.submit(cMonitorLauncher.inotify_events, queue)
       executor.submit(cMonitorLauncher.process_events, queue)

if __name__ == "__main__":
    main()
