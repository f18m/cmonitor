#!/bin/bash

# TODO: add some simulator for network activity!

set -e
set -u

# Constants
num_cycles=50

# Check utility "stress" is available
if [ -z "$(command -v stress)" ]; then
    echo "The 'stress' utility is not present. Cannot continue."
    exit 2
fi

cycle=1
while (( cycle<num_cycles )); do
    NJPID="$(pidof cmonitor_collector)"
    echo "cmonitor_collector pid=$NJPID; running CPU/memory/disk load simulator cycle #$cycle..."
    stress --cpu 1 --vm 1 --timeout $(( $RANDOM % 10 + 1 )) &
    sleep 2
    stress --cpu 1 --timeout $(( $RANDOM % 3 + 1 )) &
    sleep 6
    stress --hdd 2 --timeout $(( $RANDOM % 3 + 1 )) &
    sleep 3

    wait  # each cycle will last about 10secs
    (( cycle++ ))
done

echo "Stopping cmonitor_collector"
pkill cmonitor_collector
sleep 1

echo "Exiting the example-load.sh script..."
