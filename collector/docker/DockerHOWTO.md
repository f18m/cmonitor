# cmonitor - lightweight container monitor

A Docker/LXC, database-free, lightweight container performance monitoring solution, perfect for ephemeral containers
(e.g. containers used for DevOps automatic testing). Can also be used with InfluxDB and Grafana to monitor long-lived 
containers in real-time.

The project is composed by 2 tools: 
1) a lightweight agent (80KB, native binary; no JVM, Python or other interpreters needed) to collect actual CPU/memory/disk statistics (Linux-only)
   and store them in a JSON file;
2) a simple Python script to convert the generated JSON to a self-contained HTML page.

The agent is actually a cgroup-aware statistics collector; cgroups (i.e. Linux Control Groups) are the basic technology used 
to create containers; this project is thus aimed at monitoring your LXC/Docker container performances but can equally monitor
physical servers.

This project supports only **Linux x86_64 architectures**.

# How to use cmonitor docker

The cmonitor Docker image exposes a shared volume under /perf, so you can mount a host directory to that point to access the generated JSON file (if using JSON output mode). A typical invocation of the container might be:

```
    @docker run 
        --name=cmonitor-baremetal-collector \
        -v /root:/perf \
        f18m/cmonitor
```
