# This is just a small docker used to BUILD the cmonitor_collector utility

FROM alpine:3.15
RUN apk update && apk add --no-cache binutils make libgcc musl-dev gcc g++ fmt-dev gtest-dev
