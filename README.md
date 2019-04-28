[![Build Status](https://travis-ci.com/f18m/nmon-cgroup-aware.svg?branch=master)](https://travis-ci.com/f18m/nmon-cgroup-aware)
[![COPR RPM Build](https://copr.fedorainfracloud.org/coprs/f18m/nmon-cgroup-aware/package/nmon-cgroup-aware/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/f18m/nmon-cgroup-aware/)
[![Docker Version](https://images.microbadger.com/badges/version/f18m/nmon-cgroup-aware.svg)](https://hub.docker.com/r/f18m/nmon-cgroup-aware "Docker Image on DockerHub")


# nmon-cgroup-aware

A Docker/LXC, database-free, lightweight container performance monitoring solution, perfect for ephemeral containers
(e.g. containers used for DevOps automatic testing).

This project is a fork of [Nigel's performance Monitor for Linux](http://nmon.sourceforge.net), adding cgroup-awareness;
cgroups (i.e. Linux Control Groups) are the basic technology used to create containers; this fork is thus aimed at 
monitoring your LXC/Docker container performances (opposed to the original project coinceived only for physical servers).

NOTE: this is actually a fork of `njmon`, not `nmon`. 
`njmon` provides no interactive mode (often useless inside containers!) and is a modern tool that produces a 
JSON output that can be used together with tools like [InfluxDB](https://www.influxdata.com/) and [Grafana](https://grafana.com/).

This fork supports only **Linux x86_64 architectures**; support for AIX/PowerPC (present in original `nmon`) has been dropped.

## Features

This project collects performance data about:

- per-CPU-core usage;
- memory usage;
- network traffic (PPS and MB/s or Mbps);
- disk load;
- average Linux load;
- CPU usage as reported by the 'cpuacct' (CPU accounting) cgroup;
- memory usage as reported by the 'memory' cgroup;
- disk usage as reported by the 'blkio' cgroup;

Moreover the project allows you to easily post-process collected data and produce a **self-contained** HTML page which allows
to visualize all the performance data easily using [Google Charts](https://developers.google.com/chart/).


## Yet-Another-Monitoring-Project?

You may be thinking "yet another monitoring project" for containers. Indeed there are already quite a few open source solutions, e.g.:

- [kapacitor](https://www.influxdata.com/time-series-platform/kapacitor/)
- [Prometheus](https://prometheus.io/)
- [cAdvisor](https://github.com/google/cadvisor)
- [netdata](https://github.com/netdata/netdata)

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


## How to install

### RPM

If you use an LXC/Docker container based on a Centos/RHEL/Fedora distribution you can log into the container (or change its Dockerfile)
and just install the RPM right away from the [COPR](https://copr.fedorainfracloud.org/coprs/f18m/nmon-cgroup-aware/) repository:

```
yum install -y yum-plugin-copr
yum copr enable -y f18m/nmon-cgroup-aware
yum install -y nmon-cgroup-aware
```

### Ubuntu

If you use an LXC/Docker container based on a Ubuntu distribution you can similarly install from [my Ubuntu PPA](https://launchpad.net/~francesco-montorsi/+archive/ubuntu/ppa)
using the following commands:

```
add-apt-repository ppa:francesco-montorsi/ppa
apt-get install nmon-cgroup-aware
```

### Docker

If you want to simply use a out-of-the-box Docker container to monitor your baremetal performances you can run:

```
docker run -d --name=nmon-baremetal-collector -v /root:/perf f18m/nmon-cgroup-aware
```

which downloads the Docker image for this project from [Docker Hub](https://hub.docker.com/r/f18m/nmon-cgroup-aware)
and runs the stats collector saving data in JSON format inside your /root folder.


## How to collect stats

The RPM installs a single utility, `njmon_collector` inside your container; launch it like that:

```
njmon_collector -s 3 -m /home
```

to produce in the `/home` folder a JSON with CPU/memory/disk/network stats for the container
sampling all supported performance statistics every 3 seconds.
Whenever you want you can either:

- inject that JSON inside InfluxDB (mostly useful for **persistent** containers that you want to monitor in real-time);
  this is not covered by this README;
- use the `njmonchart` utility to convert that JSON into a self-contained HTML file (mostly useful for **ephemeral** containers);
  see below for practical examples.


## How to plot stats

To plot the JSON containing the collected statistics, simply launch the `njmon_chart` utility installed together
with the RPM/Debian package:

```
njmon_chart /path/to/json-stats.json /path/to/output-file.html
```

Example of resulting output files:

 - [baremetal1](https://f18m.github.io/nmon-cgroup-aware/examples/baremetal1.html): 
   example of graph generated with the performance stats collected from a physical server running Ubuntu 18.04; 
   in this case the `njmon_collector` was running inside the default "user.slice" cgroup so both "CGroup" and "Baremetal"
   graphs are present;
 - [docker_centos7_collecting_baremetal_stats](https://f18m.github.io/nmon-cgroup-aware/examples/docker-centos7-collecting-baremetal-stats.html): 
   example of graph generated with the performance stats collected from a physical server from inside a Docker container;
   in this case cgroup stat collection was explicitely disabled so that only baremetal performance graphs are present;
 - [docker_ubuntu1804_userapp_with_embedded_collector](https://f18m.github.io/nmon-cgroup-aware/examples/docker-ubuntu1804-userapp-with-embedded-collector.html): 
   example of graph generated with the performance stats collected from inside a Docker container; this is a practical example
   where the Docker container is actually deploying something that simulates your target application, together with an embedded
   `njmon_collector` instance that monitors the performance of the Docker container itself;
   in this case both cgroup stats and baremetal performance graphs are present.

## Links

- Original project: [http://nmon.sourceforge.net](http://nmon.sourceforge.net)
- Other forks: [https://github.com/axibase/nmon](https://github.com/axibase/nmon)


## TODO

- Add disk stats plotting
- Add 'blkio' cgroup data collection & plotting
- Add LXC examples
- Test integration with InfluxDB (JSON streaming over socket)
- Test deployment on supported Linux distributions
- Add info about RAM, disk model, NIC model
- Put the "memory.failcnt" curve on a secondary Y axis
