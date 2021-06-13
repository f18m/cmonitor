FROM alpine:3.7

# make sure you did run the "cmonitor_musl" target before building this image:
RUN apk add libstdc++ libc6-compat
COPY cmonitor_collector /usr/bin/

ARG SAMPLING_INTERVAL_SEC=60
ENV SAMPLING_INTERVAL_SEC=${SAMPLING_INTERVAL_SEC}

# finally run the cmonitor collector 
#  - in foreground since Docker does not like daemons
#  - collect "all_baremetal": in this way we just collect ONLY baremetal performance stats, not the cgroup where cmonitor_collector itself is running!
#  - by default collect every 1min
#  - put resulting files in /perf folder which is actually a volume shared with the host (see docker run command)
CMD /usr/bin/cmonitor_collector \
    --foreground \
    --sampling-interval=$SAMPLING_INTERVAL_SEC \
    --num-samples=0 \
    --collect=all_baremetal \
    --output-directory /perf

LABEL GIT_REPO_URL="https://github.com/f18m/cmonitor"
