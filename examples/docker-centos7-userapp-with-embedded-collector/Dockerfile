FROM centos:7

# first install Python3 & stress utility:
RUN yum install -y epel-release
RUN yum install -y python36 stress

# then install cmonitor from COPR repo:
RUN yum install -y yum-plugin-copr
RUN yum copr enable -y f18m/cmonitor
RUN yum install -y cmonitor-22-7

COPY example-load.sh .

# finally run the cmonitor collector 
#  - in BACKGROUND and leave in the foreground a dummy Bash script that simulates the actual
#    user's software doing some CPU and I/O
#  - collect "cgroups": in this way we just collect baremetal+container performance stats
#  - for this example collect 3minutes of data (60 samples) and then stop
#  - put resulting files in /perf folder which is actually a volume shared with the host (see docker run command)
CMD /usr/bin/cmonitor_collector \
      --sampling-interval=3 \
      --output-filename=docker-centos7-userapp-with-embedded-collector \
      --output-directory /perf ; \
    bash example-load.sh
