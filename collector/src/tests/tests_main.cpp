//------------------------------------------------------------------------------
// GTest unit tests MAIN file
//------------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <malloc.h>
#include "../cgroups.h"
#include "../output_frontend.h"


TEST(CGroups, Read3Samples)
{
    CMonitorOutputFrontend actual_output;
    actual_output.init_json_output_file("test1.json");

    CMonitorCollectorAppConfig cfg;

    double elapsed_sec = 0.1;
    std::set<std::string> allowedStats;

    CMonitorCgroups t(&cfg, &actual_output);
    for (unsigned int i=0; i < 2; i++)
    {
        //t.SetUnitTestMode("...$(root)/testing/centos7-Linux-3.10.0-x86_64/sample" + i);
        actual_output.psample_start();

        t.cgroup_proc_cpuacct(elapsed_sec, true /* emit JSON */);
        t.cgroup_proc_memory(allowedStats);
        t.cgroup_proc_tasks(elapsed_sec, cfg.m_nOutputFields /* emit JSON */, false /* do not include threads */);
        t.cgroup_proc_tasks(elapsed_sec, cfg.m_nOutputFields /* emit JSON */, true /* do include threads */);
        actual_output.push_current_sample();
    }

    // TODO: compare produced JSON with expected JSON
}



//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Make uses of freed and uninitialized memory known.
    mallopt(M_PERTURB, 42);

    // RUN ALL GTESTS
    testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS() != 0)
        return 1;

    std::cout << "All GoogleTest testsuites passed." << std::endl;

    return 0;
}
