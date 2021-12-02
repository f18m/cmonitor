# NOTE: for this example we build our Docker on Fedora which provides a few utils like
# - lshw
# - stress
# out of the box!
# NOTE: fedora:32 ships /lib64/libfmt.so.6 which is the shared object with MAJOR version required
#       by the GLIBC cmonitor_collector that gets built on Centos7... that's why we stick with 32
FROM fedora:32

RUN dnf install -y gnupg2 stress lshw fmt

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
  --output-filename=docker-userapp-with-embedded-collector \
  --output-directory=/perf ; \
  bash example-load.sh
