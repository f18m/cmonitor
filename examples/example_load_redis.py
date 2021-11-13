#!/usr/bin/python3

#
# Very simple script to generate some "random" load on a Redis instance
#
# Author: Francesco Montorsi
# Created: June 2021
#

import sys
import subprocess
import os
import time
import redis
import random

loadtime_before_random_sleep_sec = 3
cmd_batch_len = 30


def get_ext_test_port(internal_port, full_container_name):
    formatArg = (
        "--format='{{(index (index .NetworkSettings.Ports \"%d/tcp\") 0).HostPort}}'"
        % (internal_port)
    )
    command_output = subprocess.run(
        ["docker", "inspect", formatArg, full_container_name],
        stdout=subprocess.PIPE,
        universal_newlines=True,
    )

    if command_output.returncode != 0:
        print(f"The docker container {full_container_name} does not respond...")
        sys.exit(2)
    port_str = command_output.stdout.replace("'", "")
    try:
        ext_port = int(port_str)
        print(
            f"The docker container {full_container_name} has its internal port {internal_port} exposed to localhost port {ext_port}..."
        )
        return ext_port
    except:
        return 0


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":

    if len(sys.argv) != 2 and len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <docker-name> [constant-load|intermittent-load]")
        sys.exit(1)

    redis_port = get_ext_test_port(6379, sys.argv[1])
    r = redis.Redis(host="localhost", port=redis_port, db=0)

    if len(sys.argv) == 3:
        mode = sys.argv[2]
    else:
        mode = "intermittent-load"

    if mode == "intermittent-load":
        while True:
            start_time = time.time()
            while round(time.time() - start_time, 3) < loadtime_before_random_sleep_sec:
                for i in range(1, cmd_batch_len):
                    r.set(f"foo{i}", "bar")
                for i in range(1, cmd_batch_len):
                    r.get(f"bar{i}")
            time.sleep(random.randint(0, loadtime_before_random_sleep_sec * 2))
    elif mode == "constant-load":
        while True:
            for i in range(1, cmd_batch_len):
                r.set(f"foo{i}", "bar")
            for i in range(1, cmd_batch_len):
                r.get(f"bar{i}")
    else:
        print(f"Usage: f{sys.argv[0]} <docker-name> [constant-load|intermittent-load]")
        sys.exit(1)
