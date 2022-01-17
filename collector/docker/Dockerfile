# NOTE: the version of Alpine used here must be aligned with the one in Dcokerfile.builder
FROM alpine:3.15

# make sure you did run the "cmonitor_musl" target before building this image:
RUN apk add libstdc++ libc6-compat fmt-dev
COPY cmonitor_collector /usr/bin/

# finally run the cmonitor collector 
#  - in foreground since Docker does not like daemons
#  - put resulting files in /perf folder which is actually a volume shared with the host (see README.md for the docker run command)
ENTRYPOINT ["/usr/bin/cmonitor_collector", "--foreground", "--output-directory", "/perf"]

LABEL GIT_REPO_URL="https://github.com/f18m/cmonitor"
