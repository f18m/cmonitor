# NOTE: for this example we build our Docker on Ubuntu which provides a few utils like
# - lshw
# - stress
# out of the box!
FROM ubuntu:bionic

RUN apt-get update &&  apt-get -y install gnupg2 stress lshw

# for now simply install from the local build (assuming that you built the binary on Ubuntu!)
COPY cmonitor_collector /usr/bin/
COPY example-load.sh .

# finally run the cmonitor collector 
#  - in BACKGROUND and leave in the foreground a dummy Bash script that simulates the actual
#    user's software doing some CPU and I/O
#  - collect "cgroups": in this way we just collect baremetal+container performance stats
#  - for this example collect 3minutes of data (60 samples) and then stop
#  - put resulting files in /perf folder which is actually a volume shared with the host (see docker run command)
CMD /usr/bin/cmonitor_collector \
      --sampling-interval=3 \
      --output-filename=docker-ubuntu1804-userapp-with-embedded-collector \
      --output-directory /perf ; \
    bash example-load.sh
