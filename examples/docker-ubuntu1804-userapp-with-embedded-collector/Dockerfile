FROM ubuntu:bionic

RUN apt-get update &&  apt-get -y install gnupg2 stress lshw

# this is the way to install a released version from Ubuntu PPA:
#RUN apt-get install software-properties-common
#RUN add-apt-repository ppa:francesco-montorsi/ppa
#RUN echo "deb http://ppa.launchpad.net/francesco-montorsi/ppa/ubuntu bionic main" >>/etc/apt/sources.list
#RUN echo "deb-src http://ppa.launchpad.net/francesco-montorsi/ppa/ubuntu bionic main" >>/etc/apt/sources.list
#RUN apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 54B0E4B849B7DBCB
#RUN apt-get update && apt-get install nmon-cgroup-aware

# for now simply install from the local build (assuming that you built the binary on Ubuntu!)
COPY njmon_collector /usr/bin/
COPY example-load.sh .

# finally run the njmon collector 
#  - in BACKGROUND and leave in the foreground a dummy Bash script that simulates the actual
#    user's software doing some CPU and I/O
#  - collect "cgroups": in this way we just collect baremetal+container performance stats
#  - for this example collect 3minutes of data (60 samples) and then stop
#  - put resulting files in /perf folder which is actually a volume shared with the host (see docker run command)
CMD /usr/bin/njmon_collector \
      --sampling-interval=3 \
      --output-filename=docker-ubuntu1804-userapp-with-embedded-collector \
      --output-directory /perf ; \
    bash example-load.sh
