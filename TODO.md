## TODO collector-side

- Add memory.pressure and cpu.pressure collection for cgroups v2
- Start using "fmt-devel" to get very fast formatting support, see https://github.com/fmtlib/fmt
  since the collector is mostly dealing with string/integer operations
- Add 'io' cgroup v2 data collection
- Add support for UDP data tx to InfluxDB

## TODO chart-side

- Add javascript buttons to toggle graphs for e.g. disks/networks/cpus
- Add 'blkio' cgroup data plotting
- Plot CPU "counters" JSON section

## TODO testing/documentation

- Add tests on cmonitor_statistics tool taking the expected JSONs of unit tests
- Add tests on:
   CMonitorSystem
   -> challenge is it uses "lsblk" and "ifconfig" utilities, does not just read the filesystem!
   CMonitorHeaderInfo

- add more sampled data for CMonitorCGroup, for several kernels
- test more configurations for CMonitorCGroup, like dockers having no memory limit or no cpu limits
- Test deployment on supported Linux distributions
- Add LXC examples

## TODO influxdb/grafana

- Provide a nice dashboard to get started
