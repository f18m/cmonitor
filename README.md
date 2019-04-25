[![Build Status](https://travis-ci.com/f18m/nmon-cgroup-aware.svg?branch=master)](https://travis-ci.com/f18m/nmon-cgroup-aware)
[![COPR RPM Build](https://copr.fedorainfracloud.org/coprs/f18m/nmon-cgroup-aware/package/nmon-cgroup-aware/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/f18m/nmon-cgroup-aware/)


# nmon-cgroup-aware

Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness.
Cgroups (i.e. Linux Control Groups) are the basic technology used to create containers.
This fork is thus aimed at monitoring your LXC/Docker container performances.

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


## How to install

If you use an LXC container
based on a Centos/RHEL/Fedora distribution you can log into the container and run:

```
yum install -y yum-plugin-copr
yum copr enable -y f18m/nmon-cgroup-aware
yum install -y nmon-cgroup-aware
```

## How to collect stats

The RPM installs a single utility, `njmon` inside your container; launch it like that:

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

To plot the JSON containing the collected statistics, simply launch:

```
njmon_chart /path/to/json-stats.json /path/to/output-file.html
```

Example of resulting output files:

 - [baremetal1](https://f18m.github.io/nmon-cgroup-aware/examples/baremetal1_20190413_1605.html): example of graph generated with the performance stats collected from a physical server
 - [container1](https://f18m.github.io/nmon-cgroup-aware/examples/container1_12cpus_20190416_1801.html): example of graph generated with the performance stats collected from a container having 12CPUs


## Links

- Original project: [http://nmon.sourceforge.net](http://nmon.sourceforge.net)
- Other forks: [https://github.com/axibase/nmon](https://github.com/axibase/nmon)


## TODO

- Add disk stats plotting
- Add 'blkio' cgroup data collection & plotting
- Add LXC and Docker examples
- Test integration with InfluxDB (JSON streaming over socket)
- Test deployment on supported Linux distributions
- Add info about RAM, disk model, NIC model
