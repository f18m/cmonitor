## TODO collector-side

- Add memory.pressure and cpu.pressure collection for cgroups v2
- Add 'io' cgroup v2 data collection
- Add support for UDP data tx to InfluxDB
- Remove sscanf() calls in favour of a more optimized logic; from some simple
  benchmark test, sscanf() dominates the sampling time
- Add tests on:
   CMonitorSystem
   -> challenge is it uses "lsblk" utility, does not just read the filesystem!
   CMonitorHeaderInfo

- add more sampled data for CMonitorCGroup, for several kernels
- test more configurations for CMonitorCGroup, like dockers having no memory limit or no cpu limits
- add tests to Prometheus integration

## TODO tools-side

- Add 'blkio' cgroup data plotting once available
- Plot CPU "counters" JSON section
- rewrite in pytest the cmonitor_filter unit tests
- publish Pypi package

## TODO influxdb/prometheus/grafana

- Provide a nice dashboard to get started
