FROM centos:7

# first install Python3:
RUN yum install -y epel-release
RUN yum install -y python36

# then install nmon-cgroup-aware from COPR repo:
RUN yum install -y yum-plugin-copr
RUN yum copr enable -y f18m/nmon-cgroup-aware
RUN yum install -y nmon-cgroup-aware-22-4

# finally run the njmon collector 
#  - in foreground since Docker does not like daemons
#  - do not collect "cgroups": in this way we just collect baremetal performance stats
#  - for this example collect 3minutes of data (60 samples) and then stop
#  - put resulting files in /perf folder which is actually a volume shared with the host (see docker run command)
CMD /usr/bin/njmon_collector --foreground --sampling-interval=3 --num-samples=60 --collect=cpu,memory,disk,network --output-directory /perf
