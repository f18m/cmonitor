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
import signal
#import subprocess

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
    def __init__(self,path, filter, ip , port, command):
        self.path = path
        self.filter = filter
        self.ip = ip
        self.port = port
        self.command = command
        self.process_host_dict = {}
        self.prev_process_dict= {}
        self.prev_file = ""

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
        print("Input [port]: " +  str(self.port))


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
                            # remove already running cmonitor for the same process
                            if self.prev_file:
                               self.remove_process(process_name, self.prev_file)
                            # launch cmonitor for new  process
                            self.ip = self.process_host_dict.get(process_name)
                            self.launch_cmonitor(file, self.ip)
                            # store the new process task file
                            cur_pod = file.split("/")[7]
                            self.prev_process_dict[process_name] = cur_pod
                            # store the old process task file
                            self.prev_file = file

    def remove_process(self, process_name, prev_file):
        prev_pod = prev_file.split("/")[7]
        print("Previous running process:", self.prev_process_dict)
        if process_name in self.prev_process_dict:
          for line in os.popen("ps -aef | grep " + prev_pod + " | grep -v grep"):
             fields = line.split()
             pid = fields[1]
             # terminating process
             print("Terminating cMonitor running process with ID:", process_name,pid)
             os.kill(int(pid), signal.SIGKILL)
             print("Process Successfully terminated")


    def launch_cmonitor(self, filename, ip):
        for c in self.command:
            cmd = c.strip()
            port = self.port
            base_path = os.path.dirname(filename)
            path = "/".join(base_path.split("/")[5:])
            cmd = cmd + " " + "--cgroup-name=" + path + " " + "-A" + " " + ip + " " + "-S" + " " + port
            #cmd = cmd + " " + "--cgroup-name=" + path + " " + "--remote-ip" + " " + ip + " " + "--remote-port" + " " + port
            print("Launch cMonitor with command:", cmd)
            os.system(cmd)
            # FIXME : need to delete the current cmonitor already running for this process..!!!
            # pid = subprocess.Popen(cmd.split(), shell=True)
            # print("cmd PID:", pid.pid)
            # As it does not store the pid of the actual command <cmd> , need to find another way to
            # kill the already running process : using pod ID

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
        "--ip",
        nargs="*",
        help="cmonitor input IP",
    )
    parser.add_argument(
        "-r",
        "--port",
        help="cmonitor input Port",
    )
    args = parser.parse_args()
    input_path = args.path
    print("Input [path]:", input_path)
    filter = args.filter
    command = args.command
    ip = args.ip
    port = args.port

    cMonitorLauncher = CmonitorLauncher(input_path, filter, ip , port, command)

    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
       executor.submit(cMonitorLauncher.inotify_events, queue)
       executor.submit(cMonitorLauncher.process_events, queue)

if __name__ == "__main__":
    main()
