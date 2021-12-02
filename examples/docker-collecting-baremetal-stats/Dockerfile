# for this example we use the same base image used by our musl-builder image:
#   https://github.com/radupopescu/musl-builder/blob/master/Dockerfile
FROM alpine:3.14  

# make sure you did run the "cmonitor_musl" target before building this image:
RUN apk add libstdc++ libc6-compat fmt-dev 
COPY cmonitor_collector /usr/bin/

# finally run the cmonitor collector 
#  - in foreground since Docker does not like daemons
#  - do not collect "cgroups": in this way we just collect baremetal performance stats
#  - for this example collect 3minutes of data (60 samples) and then stop
#  - put resulting files in /perf folder which is actually a volume shared with the host (see docker run command)
CMD /usr/bin/cmonitor_collector \
      --foreground \
      --sampling-interval=3 \
      --num-samples=180 \
      --collect=cpu,memory,disk,network \
      --output-filename=docker-collecting-baremetal-stats \
      --output-directory /perf
