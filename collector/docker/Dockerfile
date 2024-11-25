# This Dockerfile uses a multi-stage build to create the cmonitor_collector utility

# Stage 1: Build stage
FROM alpine:3.20.2 AS builder

# Install necessary dependencies for building
RUN apk update && apk add --no-cache binutils make libgcc musl-dev gcc g++ fmt-dev gtest-dev git

# Set a working directory inside the container
WORKDIR /opt/src/cmonitor

# Copy all source files from the host to the working directory in the builder stage
COPY . .

# Build the cmonitor_collector utility, clean up, and strip debug symbols
RUN git config --global --add safe.directory /opt/src && \
    cd /opt/src/cmonitor/collector && \
    make -j MUSL_BUILD=1 DISABLE_BENCHMARKS_BUILD=1 && \
    make MUSL_BUILD=1 strip 


# Stage 2: Final stage
FROM alpine:3.20.2 AS final

RUN apk add libstdc++ libc6-compat fmt-dev

# Copy the built binary from the builder stage to the final stage
COPY --from=builder /opt/src/cmonitor/collector/bin/musl/cmonitor_collector /usr/bin/

# finally run the cmonitor collector 
#  - in foreground since Docker does not like daemons
#  - put resulting files in /perf folder which is actually a volume shared with the host (see README.md for the docker run command)
ENTRYPOINT ["/usr/bin/cmonitor_collector", "--foreground", "--output-directory", "/perf"]

LABEL GIT_REPO_URL="https://github.com/f18m/cmonitor"
