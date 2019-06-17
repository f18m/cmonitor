FROM centos:7
ARG rpm_version=1.3
ARG sampling_interval=3
ARG num_samples=60

# first install Python3:
RUN yum install -y epel-release
RUN yum install -y python36

# then install cmonitor from COPR repo:
RUN yum install -y yum-plugin-copr
RUN yum copr enable -y f18m/cmonitor
RUN yum install -y cmonitor-$rpm_version

# finally run the cmonitor collector 
#  - in foreground since Docker does not like daemons
#  - do not collect "cgroups": in this way we just collect baremetal performance stats
#  - for this example collect 3minutes of data (60 samples) and then stop
#  - put resulting files in /perf folder which is actually a volume shared with the host (see docker run command)
CMD /usr/bin/cmonitor_collector \
    --foreground \
    --sampling-interval=$sampling_interval \
    --num-samples=$num_samples \
    --collect=cpu,memory,disk,network \
    --output-directory /perf
