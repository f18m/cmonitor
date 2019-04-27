#!/bin/bash

# TODO: add some simulator for network activity!

set -e
set -u

# Constants
num_cycles=10

# Check utility "stress" is available
if [ -z "$(command -v stress)" ]; then
    echo "The 'stress' utility is not present. Cannot continue."
    exit 2
fi

cycle=1
while (( cycle<num_cycles )); do
    echo "Running cycle #$cycle..."
    stress --cpu 2 --vm 1 --timeout 10 &
    sleep 2
    stress --cpu 1 --timeout 3 &
    sleep 6
    stress --hdd 2 --timeout 3 &
    sleep 3

    wait
    (( cycle++ ))
done
