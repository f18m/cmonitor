[![Build Status](https://travis-ci.com/f18m/cmonitor.svg?branch=master)](https://travis-ci.com/f18m/cmonitor "TravisCI status")
[![Copr build status](https://copr.fedorainfracloud.org/coprs/f18m/cmonitor/package/cmonitor/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/f18m/cmonitor/package/cmonitor/ "RPMs on Fedora COPR")
[![Docker Version](https://images.microbadger.com/badges/version/f18m/cmonitor.svg)](https://hub.docker.com/r/f18m/cmonitor "Docker Image on DockerHub")

# cmonitor - lightweight container monitor

A **Docker, LXC, database-free, lightweight container performance monitoring solution**, perfect for ephemeral containers
(e.g. containers used for DevOps automatic testing). Can also be used with InfluxDB and Grafana to monitor long-lived 
containers in real-time.

The project is composed by 2 tools: 
1) a lightweight agent (80KB, native binary; no JVM, Python or other interpreters needed) to collect actual CPU/memory/disk statistics (Linux-only)
   and store them in a JSON file;
2) a simple Python script to convert the generated JSON to a self-contained HTML page.

The agent is actually a cgroup-aware statistics collector; cgroups (i.e. Linux Control Groups) are the basic technology used 
to create containers (you can [read more on them here](https://en.wikipedia.org/wiki/Cgroups)); this project is thus aimed at monitoring your LXC/Docker container performances but can equally monitor physical servers.

This project supports only **Linux x86_64 architectures**.

Table of contents of this README:

- [Features](#section-id-22)
- [Yet-Another-Monitoring-Project?](#section-id-39)
- [How to install](#section-id-63)
  - [RPM](#section-id-65)
  - [Ubuntu](#section-id-76)
  - [Docker](#section-id-86)
- [How to collect stats](#section-id-98)
- [How to plot stats collected as JSON](#section-id-116)
- [Connecting with InfluxDB and Grafana](#section-id-150)
- [Project History](#section-id-176)



<div id='section-id-22'/>

## Features

This project collects performance data about:

- per-CPU-core usage;
- memory usage;
- network traffic (PPS and MB/s or Mbps);
- disk load;
- average Linux load;
- CPU usage as reported by the `cpuacct` (CPU accounting) cgroup;
- memory usage as reported by the `memory` cgroup;
- disk usage as reported by the `blkio` cgroup;

Moreover the project allows you to easily post-process collected data and produce a **self-contained** HTML page which allows
to visualize all the performance data easily using [Google Charts](https://developers.google.com/chart/).


<div id='section-id-39'/>

## Yet-Another-Monitoring-Project?

You may be thinking "yet another monitoring project" for containers. Indeed there are already quite a few open source solutions, e.g.:

- [kapacitor](https://www.influxdata.com/time-series-platform/kapacitor/)
- [Prometheus](https://prometheus.io/)
- [cAdvisor](https://github.com/google/cadvisor)
- [netdata](https://github.com/netdata/netdata)
- [collectd](https://collectd.org/)

All these are very complete solutions that allow you to monitor swarms of containers, in real time.
The downside is that all these projects require you to setup an infrastructure (usually a time-series database) that collects
in real-time all the statistics and then have some powerful web platform (e.g. Graphana) to render those time-series.
All this is fantastic for **persistent** containers.

This project instead is focused on providing a database-free, lightweight container performance monitoring solution, 
perfect for **ephemeral** containers (e.g. containers used for DevOps automatic testing). The idea is much simpler:
1) you collect data for your container (or, well, your physical server) using a small collector software (written in C++ to
  avoid Java virtual machines, Python interpreters or the like!) that saves data on disk in JSON format;
2) you save the JSON file, convert it to a **self-contained** HTML page;
3) you can archive that HTML file, send it by email, put in a tarball or whatever you like the most: no dependencies at all
  are required to visualize it later!


<div id='section-id-63'/>

## How to install

<div id='section-id-65'/>

### RPM

If you use an LXC/Docker container based on a Centos/RHEL/Fedora distribution you can log into the container (or change its Dockerfile)
and just install the RPM right away from the [COPR](https://copr.fedorainfracloud.org/coprs/f18m/cmonitor/) repository:

```
yum install -y yum-plugin-copr
yum copr enable -y f18m/cmonitor
yum install -y cmonitor
```

<div id='section-id-76'/>

### Ubuntu

If you use an LXC/Docker container based on a Ubuntu distribution you can similarly install from [my Ubuntu PPA](https://launchpad.net/~francesco-montorsi/+archive/ubuntu/cmonitor)
using the following commands:

```
add-apt-repository ppa:francesco-montorsi/ppa
apt-get install cmonitor
```

<div id='section-id-86'/>

### Docker

If you want to simply use a out-of-the-box Docker container to monitor your baremetal performances you can run:

```
docker run -d --name=cmonitor-baremetal-collector -v /root:/perf f18m/cmonitor
```

which downloads the Docker image for this project from [Docker Hub](https://hub.docker.com/r/f18m/cmonitor)
and runs the stats collector saving data in JSON format inside your /root folder.


<div id='section-id-98'/>

## How to collect stats

The RPM installs a single utility, `cmonitor_collector` inside your container; launch it like that:

```
cmonitor_collector -s 3 -m /home
```

to produce in the `/home` folder a JSON with CPU/memory/disk/network stats for the container
sampling all supported performance statistics every 3 seconds.
Whenever you want you can either:

- inject that JSON inside InfluxDB (mostly useful for **persistent** containers that you want to monitor in real-time);
  see section "Connecting with InfluxDB and Grafana" below;
- or use the `cmonitor_chart` utility to convert that JSON into a self-contained HTML file (mostly useful for **ephemeral** containers);
  see below for practical examples.


<div id='section-id-116'/>

## How to plot stats collected as JSON

To plot the JSON containing the collected statistics, simply launch the `cmonitor_chart` utility installed together
with the RPM/Debian package:

```
cmonitor_chart /path/to/json-stats.json
```

Example of resulting output files:

1) [baremetal1](https://f18m.github.io/cmonitor/examples/baremetal1.html): 
   example of graph generated with the performance stats collected from a physical server running Ubuntu 18.04; 
   the `cmonitor_collector` utility was running inside the default "user.slice" cgroup and collected both the stats of that cgroup 
   and all baremetal stats;
2) [docker_centos7_collecting_baremetal_stats](https://f18m.github.io/cmonitor/examples/docker-centos7-collecting-baremetal-stats.html): 
   example of graph generated with the performance stats collected from a physical server from inside a Docker container;
   in this case cgroup stat collection was explicitely disabled so that only baremetal performance graphs are present;
3) [docker_ubuntu1804_userapp_with_embedded_collector](https://f18m.github.io/cmonitor/examples/docker-ubuntu1804-userapp-with-embedded-collector.html): 
   example of graph generated with the performance stats collected from inside a Docker container using Ubuntu as base image; this is a practical example
   where the Docker container is actually deploying something that simulates your target application, together with an embedded
   `cmonitor_collector` instance that monitors the performance of the Docker container itself;
   in this case both cgroup stats and baremetal performance graphs are present.
4) [docker_centos7_userapp_with_embedded_collector](https://f18m.github.io/cmonitor/examples/docker-centos7-userapp-with-embedded-collector.html):
   same graph as example n. 3, but obtained from a Docker container running Centos 7 instead of Ubuntu as base image.

A longer example of collected statistics (results in a larger file, may take some time to download):

1) [baremetal2](https://f18m.github.io/cmonitor/examples/baremetal2.html): 
   example of graph generated with 9 hours of performance stats collected from a physical server running Centos7 and with 56 CPUs (!!); 
   the `cmonitor_collector` utility was running inside the default "user.slice" cgroup so both "CGroup" and "Baremetal"
   graphs are present;


<div id='section-id-150'/>

## Connecting with InfluxDB and Grafana

The `cmonitor_collector` can be connected to an [InfluxDB](https://www.influxdata.com/) deployment to store collected data (this can happen
in parallel to the JSON default storage). This can be done by simply providing the IP and port of the InfluxDB instance when launching
the collector:

```
cmonitor_collector --remote-ip=1.2.3.4 --remote-port=8086
```

The InfluxDB instance can then be used as data source for graphing tools like [Grafana](https://grafana.com/)
which allow you to create nice interactive dashboards like the following one:

![Basic Dashboard Example](examples/grafana-dashboards/BasicDashboardExample.png)

You can also play with the [live dashboard example](https://snapshot.raintank.io/dashboard/snapshot/JdX4hDukUCGuJHsXymM86KbFO5LC9GrY?orgId=2&from=1558478922136&to=1558479706448)

To setup easily and quickly the chain "cmonitor_collector-InfluxDB-Grafana" you can checkout the repo of this project and run:

```
make -C examples regen_grafana_screenshots
```

which uses Docker files to deploy a temporary setup and fill the InfluxDB with 10minutes of data collected from the baremetal.


<div id='section-id-176'/>

## Project History

This project started as a fork of [Nigel's performance Monitor for Linux](http://nmon.sourceforge.net), adding cgroup-awareness;
but it has quickly evolved to a point where it shares very little code pieces with the original `njmon` tool.

Some key differences now include:
 - cgroup-aware: several performance cgroup stats are collected by `cmonitor_collector` and plotted by `cmonitor_chart`
 - more command-line options for `cmonitor_collector`;
 - HTML page generated by `cmonitor_chart` differently organized;
 - `cmonitor_collector` is able to connect to InfluxDB directly and does not need intermediate Python scripts to transform
   from JSON streamed data to InfluxDB-compatible stream.

This fork supports only Linux x86_64 architectures; support for AIX/PowerPC (present in original `nmon`) has been dropped.

- Original project: [http://nmon.sourceforge.net](http://nmon.sourceforge.net)
- Other forks: [https://github.com/axibase/nmon](https://github.com/axibase/nmon)
