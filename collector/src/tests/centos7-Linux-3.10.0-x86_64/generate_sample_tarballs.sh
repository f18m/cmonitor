#!/bin/bash

# main inputs of this script, can be provided by CLI
# Usage:
#  this_script.sh  <docker-name> <name-main-process-inside-docker> <nsamples>
#
docker_name=${1:-userapp}
process_name=${2:-redis-server}
nsamples=${3:-3}

function generate_sample_tarball()
{
    local output_sample="$1"

    rm -rf /tmp/cmonitor-temp
    mkdir -p /tmp/cmonitor-temp
    for dir_to_copy in \
        /sys/fs/cgroup/memory/docker/${cgroup}/ \
        /sys/fs/cgroup/cpu,cpuacct/docker/${cgroup}/ \
        /sys/fs/cgroup/cpuset/docker/${cgroup}/ ; do

        echo "Copying ${dir_to_copy}"
        mkdir -p /tmp/cmonitor-temp/${dir_to_copy}

        # rsync is unable to copy special files coming from /sys
        #rsync --inplace -rlptgo ${dir_to_copy} /tmp/cmonitor-temp/${dir_to_copy}

        # cp instead is capable:
        cp -ar ${dir_to_copy}/* /tmp/cmonitor-temp/${dir_to_copy} 2>/dev/null
    done

    for pid in ${pids_to_save}; do
        echo "Processing PID/TID=${pid}"
        proc_files="$(find /proc/${pid}/ ! -name pagemap)"
        for ff in ${proc_files}; do
            if [ ! -d "$ff" ]; then
                if [[ "$ff" = *"pagemap" ]]; then
                    # pagemap file is huge, skip it
                    echo "Skipping $ff"
                else
                    #echo "copying $ff"
                    dst_dir="/tmp/cmonitor-temp/$(dirname $ff)"
                    mkdir -p $dst_dir
                    cp -a $ff $dst_dir/ 2>/dev/null
                fi
            fi
        done
    done

    echo "Generating ${output_sample}"
    mkdir -p "$(dirname ${output_sample})"
    pushd /tmp/cmonitor-temp
    tar -czf ${output_sample} *
    popd
}

# get some information that won't change across samples
cgroup="$(docker ps -aq --no-trunc -f "name=$docker_name")"
pids_to_save="$(ps -T -o tid --no-headers -C ${process_name})"

# generate each sample
for i in $(seq 1 $nsamples); do
    echo "** Generating tarball for the ${i}-th sample of process=${process_name} inside docker named=${docker_name}"
    generate_sample_tarball $(pwd)/sample${i}/sample${i}.tar.gz
    sleep 1
done
