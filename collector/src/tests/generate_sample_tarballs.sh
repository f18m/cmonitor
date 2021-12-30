#!/bin/bash

# main inputs of this script, can be provided by CLI
# Usage:
#  this_script.sh <output-folder> [name-of-docker-to-launch] [nsamples] [sampling_interval_sec]
# If [name-of-docker-to-launch] is "none" then the system cgroup are sampled
#

# CLI option parsing
output_folder=${1:-}
docker_name=${2:-userapp}
nsamples=${3:-4}
ninterval_sec=${4:-5}

# Globals
redis_load_generator=../../../examples/example_load_redis.py
cgroups_ver=1 # will be auto-adjusted later on to "2" depending on where we run
process_name=redis-server
redis_docker_cpus="0,1"
redis_docker_cpu_quota="0.9"
redis_docker_memory_limit="10m"
pids_to_save=""


function init_docker_cgroups()
{
    echo "Launching the docker ${docker_name} from which we will sample unit test data"
    docker run \
        --detach \
        --interactive \
        --tty \
        --rm \
        --cpuset-cpus=${redis_docker_cpus} \
        --cpus=${redis_docker_cpu_quota} \
        --memory=${redis_docker_memory_limit} \
        -P \
        --name ${docker_name} \
        redis:latest
    sleep 1
    
    # get some information that won't change across samples
    cgroup="$(docker ps -aq --no-trunc -f "name=$docker_name")"
    pids_to_save="$(ps -T -o tid --no-headers -C ${process_name} | tr '\n' ' ')"
    if [ "${cgroup}" = "" ]; then
        echo "Failed to launch the docker or retrieve its full name"
        exit 2
    fi

    echo "Monitoring PIDs: ${pids_to_save}"

    # now detect cgroup ver
    cgroups_ver=$(docker info 2>/dev/null | grep 'Cgroup Version' | sed 's@\s*Cgroup Version:\s*\([1,2]\)@\1@g')

    # older docker versions did not includ ethe "Cgroup Version" at all... assume v1 in such case:
    if [ -z "${cgroups_ver}" ]; then
        cgroups_ver="1"
    fi

    if [ "${cgroups_ver}" = "1" ]; then
        # cgroups v1 folders:
        echo "Detected docker using cgroups v1 with cgroupfs driver"
        cgroup_MEMORY="memory/docker/${cgroup}"
        cgroup_CPUACCT="cpu,cpuacct/docker/${cgroup}"
        cgroup_CPUSET="cpuset/docker/${cgroup}"
        cgroup_BLKIO="blkio/docker/${cgroup}"
        cgroup_HUGETLB="hugetlb/docker/${cgroup}"
    elif [ "${cgroups_ver}" = "2" ]; then
        # cgroups v2 folders:
        echo "Detected docker using cgroups v2 with systemd driver"
        cgroup_UNIFIED="system.slice/docker-${cgroup}.scope/"
    else
        echo "!!! Cannot understand cgroup version in use from 'docker info' !!! Aborting"
        exit 2
    fi
        
    # make sure load generator is stopped
    pkill example_load_re
}

function init_baremetal_cgroups()
{
    echo "Will snapshot cgroups created on the baremetal by systemd"

    num_controllers="$(cat /proc/self/cgroup | wc -l)"

    if [ "${num_controllers}" = "1" ]; then
        echo "Detected systemd using (only) cgroups v2"
        cgroups_ver="2"
    else
        echo "Detected systemd using cgroups v1"
        cgroups_ver="1"
    fi


    # see https://www.freedesktop.org/wiki/Software/systemd/ControlGroupInterface/
    
    if [ "${cgroups_ver}" = "1" ]; then
        # cgroups v1 folders:
        cgroup_MEMORY="memory/$( cat /proc/self/cgroup | grep 'memory' | sed 's@[0-9]*:memory:\(.*\)@\1@g' )"
        cgroup_CPUACCT="cpu,cpuacct/$( cat /proc/self/cgroup | grep 'cpuacct' | sed 's@[0-9]*:cpuacct,cpu:\(.*\)@\1@g' )"
        cgroup_CPUSET="cpuset/$( cat /proc/self/cgroup | grep 'cpuset' | sed 's@[0-9]*:cpuset:\(.*\)@\1@g' )"
        cgroup_BLKIO="blkio/$( cat /proc/self/cgroup | grep 'blkio' | sed 's@[0-9]*:blkio:\(.*\)@\1@g' )"
        cgroup_HUGETLB="hugetlb/$( cat /proc/self/cgroup | grep 'hugetlb' | sed 's@[0-9]*:hugetlb:\(.*\)@\1@g' )"
        cgroup_SYSTEMD="systemd/$( cat /proc/self/cgroup | grep 'name=systemd' | sed 's@[0-9]*:name=systemd:\(.*\)@\1@g' )"

        # FIXME: we should start the 'process_name' inside current SCOPE here

        # PIDs that belong to the current SLICE/SCOPE can be found in the systemd folder:
        # NOTE: this is NOT the expected behavior: when the user starts cmonitor_collector outside Docker he's interested
        #       in monitoring all non-containerized processes so we should collect all tasks under e.g. MEMORY controller
        #pids_to_save="$( cat /sys/fs/cgroup/${cgroup_SYSTEMD}/tasks | tr '\n' ' ' )"

        # NOTE: do not take all processes on the system... just first 50 for speed reason
        pids_to_save="$( cat /sys/fs/cgroup/${cgroup_MEMORY}/tasks | head -n50 | tr '\n' ' ' )"
        

    elif [ "${cgroups_ver}" = "2" ]; then
        # cgroups v2 folders:
        cgroup_UNIFIED="$( cat /proc/self/cgroup | sed 's@0::@@g' )"

        pids_to_save="$( cat /sys/fs/cgroup/${cgroup_UNIFIED}/cgroup.threads | tr '\n' ' ' )"
    else
        echo "!!! Cannot understand cgroup version in use from 'docker info' !!! Aborting"
        exit 2
    fi
    
    echo "Monitoring PIDs: ${pids_to_save}"
}

function copy_all_cgroup_folders_v1()
{
    for dir_to_copy in \
        /sys/fs/cgroup/${cgroup_MEMORY}/ \
        /sys/fs/cgroup/${cgroup_CPUACCT}/ \
        /sys/fs/cgroup/${cgroup_CPUSET}/ \
        /sys/fs/cgroup/${cgroup_BLKIO}/ \
        /sys/fs/cgroup/${cgroup_HUGETLB}/ ; do

        echo "Copying ${dir_to_copy}"
        mkdir -p /tmp/cmonitor-temp/${dir_to_copy}

        # rsync is unable to copy special files coming from /sys
        #rsync --inplace -rlptgo ${dir_to_copy} /tmp/cmonitor-temp/${dir_to_copy}

        # cp instead is capable:
        # IMPORTANT: do not perform a RECURSIVE copy: we may capture nested cgroups which are not interesting to us...
        #            cmonitor_collector is not going to use nested cgroup information in any manner anyhow
        cp --no-dereference --preserve=all ${dir_to_copy}/* /tmp/cmonitor-temp/${dir_to_copy} 2>/dev/null
    done
}

function copy_all_cgroup_folder_v2()
{
    dir_to_copy="/sys/fs/cgroup/${cgroup_UNIFIED}"

    echo "Copying ${dir_to_copy}"
    mkdir -p /tmp/cmonitor-temp/${dir_to_copy}
    cp -ar ${dir_to_copy}/* /tmp/cmonitor-temp/${dir_to_copy} 2>/dev/null
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
        mkdir -p $dst_dir $dst_dir/net
        cp -a /proc/${pid}/cgroup /proc/${pid}/mounts /proc/${pid}/stat /proc/${pid}/statm /proc/${pid}/status /proc/${pid}/io         $dst_dir
        cp -a /proc/${pid}/net/dev $dst_dir/net/

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
    if [ "$cgroups_ver" = "1" ]; then
        copy_all_cgroup_folders_v1
    else
        copy_all_cgroup_folder_v2
    fi

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

# we need abs path
output_folder="$(readlink -f $output_folder)"

# launch the docker from which we sample unit test data
if [ "${docker_name}" != "none" ]; then
    init_docker_cgroups
else
    init_baremetal_cgroups
fi

# generate each sample
for nsample in $(seq 1 $nsamples); do
    echo "** Generating tarball for the ${nsample}-th sample of process=${process_name} inside docker named=${docker_name}"
    generate_sample_tarball $output_folder/sample${nsample}/sample${nsample}.tar.gz

    if [ "${docker_name}" != "none" ]; then
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
    fi

    echo "Sleeping for ${ninterval_sec}sec"
    sleep ${ninterval_sec}
done
