## TODO collector-side

- Add 'blkio' cgroup data collection
- Add benchmarks to see if fscanf() is actually slower compared to string2int() to optimize CPU usage
- Add support for UDP data tx to InfluxDB

## TODO chart-side

- Add javascript buttons to toggle graphs for e.g. disks/networks/cpus
- Add 'blkio' cgroup data plotting
- Plot CPU "counters" JSON section

## TODO testing/documentation

- Add tests... a lot of them
   On cmonitor_collector
     -> writing integration tests would be ideal... but how do we get consistent results across runs for things like CPU usage, memory, ifconfig output etc ?
     -> unit tests:
          cgroup monitoring inside CMonitorCollectorApp can be split into a separate CMonitorCgroups having 
           as INPUT:     m_strUnitTestModePrefix  ==  ""  for production,   while for testing $(root)/testing/centos7-Linux-3.10.0-x86_64/  <all files that are read by CMonitorCgroups>
           as OUTPUT:    

       GTEST would look like:
        
         
TEST(CGroups, Read3Samples)
{
    CMonitorCgroups t;

    g_logger.init();
    g_output.init_json_output_file("test1");

    for (i in 1,2,3)
    {
        t.SetUnitTestMode("...$(root)/testing/centos7-Linux-3.10.0-x86_64/sample" + i);

        t.cgroup_proc_cpuacct(elapsed, true /* emit JSON */);
        t.cgroup_proc_memory(charted_stats_from_cgroup_memory);
        t.cgroup_proc_tasks(elapsed, g_cfg.m_nOutputFields /* emit JSON */, false /* do not include threads */);
        t.cgroup_proc_tasks(elapsed, g_cfg.m_nOutputFields /* emit JSON */, true /* do include threads */);
    }

    // compare produced JSON with expected JSON
    

}

- Test deployment on supported Linux distributions
- Add LXC examples

## TODO influxdb/grafana

- Provide a nice dashboard to get started
