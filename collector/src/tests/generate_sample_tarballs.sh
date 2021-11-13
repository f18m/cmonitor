#!/bin/bash

# main inputs of this script, can be provided by CLI
# Usage:
#  this_script.sh <output-folder> [docker-name] [name-main-process-inside-docker] [nsamples] [sampling_interval_sec]
#

# CLI option parsing
output_folder=${1:-}
docker_name=${2:-userapp}
process_name=${3:-redis-server}
nsamples=${4:-4}
ninterval_sec=${5:-5}
redis_load_generator=../../../../examples/example_load_redis.py


function copy_all_cgroup_folders()
{
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
}

function copy_all_proc_files_SLOW()
{
    # FIXME: this function is pretty slow and introduces some inaccuracies in the sampling time:
    #        in practice we sample the /proc filesystem hierarchy so slowly that the approximation of
    #        considering data sampled "instantaneously" becomes very coarse
    for pid in ${pids_to_save}; do
        echo "Processing PID/TID=${pid}"
        proc_files="$(find /proc/${pid}/ -not -name pagemap -not -path '*/attr/*' -not -path '*/fd/*' -not -path '*/fdinfo/*' -not -path '*/map_files/*' -not -path '*/net/*' -not -path '*/ns/*')"
        nfiles=0
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
                    (( nfiles++ ))
                fi
            fi
        done
        echo "... copied ${nfiles} files"
    done
}

function copy_selected_proc_files()
{
    for pid in ${pids_to_save}; do
        echo "Processing PID/TID=${pid}"

        dst_dir="/tmp/cmonitor-temp/proc/${pid}"
        mkdir -p $dst_dir
        cp -a /proc/${pid}/stat /proc/${pid}/statm /proc/${pid}/status /proc/${pid}/io        $dst_dir

        # copy also all equivalent files from the "tasks" folder
        for task in /proc/${pid}/task/*; do
            # task will be something like /proc/1930618/task/1930618/
            dst_dir="/tmp/cmonitor-temp/${task}"
            mkdir -p $dst_dir
            cp -a ${task}/stat ${task}/statm ${task}/status ${task}/io        $dst_dir
        done
    done
}

function generate_sample_tarball()
{
    local output_sample="$1"

    rm -rf /tmp/cmonitor-temp
    mkdir -p /tmp/cmonitor-temp

    # save current sampling time in nanosecs
    date +"%s%N" >/tmp/cmonitor-temp/sample-timestamp

    # generate, as fast as possible, a perfect copy of /sys/fs/cgroup and /proc hierarchies
    copy_all_cgroup_folders
    copy_selected_proc_files

    echo "Generating ${output_sample}"
    mkdir -p "$(dirname ${output_sample})"
    pushd /tmp/cmonitor-temp
    tar -czf ${output_sample} *
    popd
}




## MAIN ##

if [[ "$output_folder" = "" ]]; then
    echo "At least the output folder name is required"
    exit 2
fi

# get some information that won't change across samples
cgroup="$(docker ps -aq --no-trunc -f "name=$docker_name")"
pids_to_save="$(ps -T -o tid --no-headers -C ${process_name})"

# make sure load generator is stopped
pkill example_load_re

# generate each sample
for nsample in $(seq 1 $nsamples); do
    echo "** Generating tarball for the ${nsample}-th sample of process=${process_name} inside docker named=${docker_name}"
    generate_sample_tarball $output_folder/sample${nsample}/sample${nsample}.tar.gz

    if [[ "$nsample" == "2" ]]; then
        # immediately after sample#2 is produced, put artificial load on Redis
        for i in $(seq 1 20); do
            echo "Spawning instance $i of the simple Redis load application" 
            ${redis_load_generator} ${docker_name} constant-load &
        done
    elif [[ "$nsample" == "3" ]]; then
        # immediately after sample#3 is produced, remove artificial load on Redis
	    pkill example_load_re
    fi

    echo "Sleeping for ${ninterval_sec}sec"
    sleep ${ninterval_sec}
done
