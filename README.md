[![Build Status](https://travis-ci.com/f18m/nmon-cgroup-aware.svg?branch=master)](https://travis-ci.com/f18m/nmon-cgroup-aware)
[![COPR RPM Build](https://copr.fedorainfracloud.org/coprs/f18m/nmon-cgroup-aware/package/nmon-cgroup-aware/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/f18m/nmon-cgroup-aware/)


# nmon-cgroup-aware

Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness.
Cgroups (i.e. Linux Control Groups) are the basic technology used to create containers.
This fork thus is aimed to monitoring your LXC/Docker container performances.

NOTE: this is actually a fork of "njmon", not "nmon". 
njmon provides no interactive mode (often useless inside containers!) and is a more modern tool that produces a 
JSON output that can be used together with tools like [InfluxDB](https://www.influxdata.com/) and [Grafana](https://grafana.com/).

This fork also aims at supporting mostly x86_64 architectures; support for AIX/PowerPC has been dropped.


## How to install

If you use an LXC container
based on a Centos/RHEL/Fedora distribution you can log into the container and run:

```
yum install -y yum-plugin-copr
yum copr enable f18m/nmon-cgroup-aware
yum install -y nmon-cgroup-aware
```

## How to collect stats

The RPM installs a single utility, `njmon` inside your container; launch it like that:

```
njmon_collector -C -s3 -f -m /home
```

to produce in the `/home` folder a JSON with CPU/memory/disk/network stats for the container.
Whenever you want you can either:

- inject that JSON inside InfluxDB (mostly useful for **persistent** containers that you want to monitor in real-time);
   this is not covered by this README;
- use the `njmonchart` utility to convert that JSON into a self-contained HTML file (mostly useful for **ephemeral** containers);
   see below for practical examples.


## How to plot stats

To plot the JSON containing the collected statistics, simply launch:

```
njmon_chart /path/to/json-stats.json /path/to/json-stats.html
```

Example of resulting output files:

 - [baremetal1](https://f18m.github.io/nmon-cgroup-aware/examples/baremetal1_20190413_1605.html): example of graph generated with the performance stats collected from a physical server
 - [container1](https://f18m.github.io/nmon-cgroup-aware/examples/container1_12cpus_20190416_1801.html): example of graph generated with the performance stats collected from a container having 12CPUs


## Links

- Original project: [http://nmon.sourceforge.net](http://nmon.sourceforge.net)
- Other forks: [https://github.com/axibase/nmon](https://github.com/axibase/nmon)
