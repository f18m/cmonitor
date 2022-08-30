#!/usr/bin/python3

#
# cmonitor_watcher.py
#
# Author: Satyabrata Bharati
# Created: April 2022
#
import inotify.adapters
import queue
import os
import time
import logging
from datetime import datetime

exit_flag = False
# =======================================================================================================
# CgroupWatcher : Basic inotify class
# =======================================================================================================
class CgroupWatcher:
    """
    - Watch all files below a directory and notify an event for changes.
    - Retrieves all the process and extract the process name "/proc/<pid>/stat.
    - check the process name against the white-list given in the filter list.
    - store the events in Queue.
    """

    def __init__(self, path, filter, timeout):
        """Initialize CgroupWatcher
        Args:
          path: path to watch for events.
          filter: white-list against which the process-event is filtered.

        """
        self.path = path
        self.filter = filter
        self.timeout = timeout
        self.myFileList = {}

    def __get_cgroup_version(self):
        """
        Detect the cgroup version.
        """
        proc_self_mount = "/proc/self/mounts"
        ncgroup_v1 = 0
        ncgroup_v2 = 0
        with open(proc_self_mount) as file:
            for line in file:
                row = line.split()
                fs_spec = row[0]
                fs_file = row[1]
                fs_vfstype = row[2]
                if (fs_spec == "cgroup" or fs_spec == "cgroup2") and fs_vfstype == "cgroup2":
                    ncgroup_v2 += 1
                else:
                    ncgroup_v1 += 1

        if ncgroup_v1 == 0 and ncgroup_v2 > 0:
            cgroup_versopn = "v2"
            return cgroup_version
        else:
            cgroup_version = "v1"
            return cgroup_version

    def __get_process_name(self, pid):
        """Returns the process name for the process id.
        Args:
          pid: process id.

        Returns:
          The process name.

        """
        cgroup_version = self.__get_cgroup_version()
        if cgroup_version == "v1":
            proc_filename = "/proc" + "/" + pid + "/stat"
        else:
            proc_filename = "/proc" + "/" + pid + "/cgroup.procs"
        with open(proc_filename) as file:
            for line in file:
                parts = line.split()
                process_name = parts[1].strip("()")
        return process_name

    def __get_pid_list(self, filename):
        """Get the list of the process ids belong to a tasks file.
        Args:
          filename: the tasks file.

        Returns:
          The list of PIDs within the tasks file.

        """
        list = []
        with open(filename) as file:
            for line in file:
                list.append(line.strip())
        return list

    def __get_list_of_files(self, dir):
        """Returns the list of the files created for the event within the watched dir.
        Args:
          filename: dir to be watched.

        Returns:
          The list of files created within the watched dir.

        """
        listOfFiles = os.listdir(dir)
        allFiles = list()
        for entry in listOfFiles:
            fullpath = os.path.join(dir, entry)
            if os.path.isdir(fullpath):
                allFiles = allFiles + self.__get_list_of_files(fullpath)
            else:
                allFiles.append(fullpath)

        return allFiles

    def __process_task_files(self, dir):
        """Process all the files for triggered-event within the watched dir.
            Finds the process Ids and filter out the process name against the
            provided white-list. If the process Id matches the whilte-listing
            process from command-line , it store and return the file anlog with the process-name.
            The process name later will be used to get the ip and port from the
            command-line for the specific process.
        Args:
          dir: dir to be watched.

        Returns:
          The file along with the process name which will be used to launch cmonitor.

        """
        # time.sleep(20)
        time.sleep(self.timeout)
        logging.info(f"watcher process file sleep: {self.timeout}")
        allFiles = self.__get_list_of_files(dir)
        for file in allFiles:
            if file.endswith("tasks"):
                list = self.__get_pid_list(file)
                if list:
                    for pid in list:
                        process_name = self.__get_process_name(pid)
                        logging.info(f"processing task file: {file} with pid: {pid}, process name: {process_name}")
                        match = self.__check_filter(process_name)
                        if match is True:
                            logging.info(f"Found match: {process_name}")
                            self.myFileList = {file: process_name}
                            return self.myFileList

    def __check_filter(self, process_name):
        """Check process name against the whilte-list.
        Args:
          process_name: process name to be matched against the whilte-list from command-line.

        Returns:
          True if process_name matches with the white-list.

        """
        for e in self.filter:
            if process_name in e:
                return True

    def inotify_events(self, queue):
        """Main thread function for notifying events.
            Monitored events that match with the white-list provided will be stored in this queue.
            The events from this queue will be processed by cMonitorLauncher threading function to
            launch cMonitor with appropriate command input
        Args:
          queue: monitored events will be stored in this queue.

        Returns:

        """
        logging.info(f"CgroupWatcher calling inotify_event")
        i = inotify.adapters.Inotify()
        i.add_watch(self.path)
        try:
            for event in i.event_gen():
                if event is not None:
                    if "IN_CREATE" in event[1]:
                        (header, type_names, path, filename) = event
                        logging.info(f"CgroupWatcher event triggered:{path},{filename}")
                        dir = path + filename
                        logging.info(f"CgroupWatcher event created:{filename}")
                        fileList = self.__process_task_files(dir)
                        if fileList:
                            logging.info(f"CgroupWatcher event in Queue:{fileList}")
                            queue.put(fileList)
                # global exit_flag
                if exit_flag is True:
                   logging.info(f"CgroupWatcher exit_flag {exit_flag}")
                   exit(1)


        finally:
            i.remove_watch(path)
