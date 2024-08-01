# This is just a small docker used to BUILD the cmonitor_collector utility

FROM alpine:3.20.2
RUN apk update && apk add --no-cache binutils make libgcc musl-dev gcc g++ fmt-dev gtest-dev git
RUN git config --global --add safe.directory /opt/src
