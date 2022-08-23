#!/usr/bin/env python3

"""
    Author: Satyabrata Bharati
    Verify the behavior of CmonitorCgroupWatcher InotifyEvent : dummy process to monitor
"""
import time

def call_sleep():
    while True:
        time.sleep(30)

if __name__ == "__main__":
    call_sleep()
