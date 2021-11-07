#!/bin/bash

cgroup="docker/5ccb1395eef093a837e302c52f8cb633cc276ea7d697151ecc34187db571a3b2"
pid_to_save=$(pidof redis-server)
nsamples=3

function generate_sample_tarball()
{
    local output_sample="$1"

    rm -rf /tmp/cmonitor-temp
    mkdir -p /tmp/cmonitor-temp
    for dir_to_copy in \
        /sys/fs/cgroup/memory/${cgroup}/ \
        /sys/fs/cgroup/cpu,cpuacct/${cgroup}/ \
        /sys/fs/cgroup/cpuset/${cgroup}/ ; do

        echo "Copying ${dir_to_copy}"
        mkdir -p /tmp/cmonitor-temp/${dir_to_copy}

        # rsync is unable to copy special files coming from /sys
        #rsync --inplace -rlptgo ${dir_to_copy} /tmp/cmonitor-temp/${dir_to_copy}

        # cp instead is capable:
        cp -ar ${dir_to_copy}/* /tmp/cmonitor-temp/${dir_to_copy} 2>/dev/null
    done

    proc_files="$(find /proc/${pid_to_save}/ ! -name pagemap)"
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

    echo "Generating ${output_sample}"
    mkdir -p "$(dirname ${output_sample})"
    pushd /tmp/cmonitor-temp
    tar -czf ${output_sample} *
    popd
}

for i in $(seq 1 $nsamples); do
    generate_sample_tarball $(pwd)/sample${i}/sample${i}.tar.gz
    sleep 1
done
