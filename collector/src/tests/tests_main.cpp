//------------------------------------------------------------------------------
// GTest unit tests MAIN file
//------------------------------------------------------------------------------

#include "../cgroups.h"
#include "../logger.h"
#include "../output_frontend.h"
#include <gtest/gtest.h>
#include <linux/limits.h>
#include <malloc.h>

std::string get_unit_test_abs_dir()
{
    char buff[PATH_MAX + 1];
    getcwd(buff, sizeof(buff));
    char actualpath[PATH_MAX + 1];
    char* ptr = realpath(buff, actualpath);

    return std::string(ptr) + "/";
}

TEST(CGroups, Read3Samples)
{
    CMonitorOutputFrontend actual_output("test1.json");
    CMonitorCollectorAppConfig cfg;
    cfg.m_strCGroupName = "docker/5ccb1395eef093a837e302c52f8cb633cc276ea7d697151ecc34187db571a3b2";

    CMonitorLogger::instance()->enable_debug();
    CMonitorLogger::instance()->init_error_output_file("stdout");

    double elapsed_sec = 0.1;
    std::set<std::string> allowedStats;

    // printf("fmon %s\n", get_unit_test_abs_dir().c_str());
    std::string kernel_test = "centos7-Linux-3.10.0-x86_64";
    std::string current_sample_abs_dir = get_unit_test_abs_dir() + kernel_test + "/current-sample";

    // allocate the class under test:
    CMonitorCgroups t(&cfg, &actual_output);
    t.cgroup_init( // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/memory", // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/cpu,cpuacct", // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/cpuset");

    // start feeding fixed, test data
    actual_output.pheader_start();
    actual_output.push_header();
    actual_output.psample_array_start();
    for (unsigned int i = 0; i < 2; i++) {
        // t.SetUnitTestMode("...$(root)/testing/centos7-Linux-3.10.0-x86_64/sample" + i);
        actual_output.psample_start();
        t.cgroup_proc_cpuacct(elapsed_sec, true /* emit JSON */);
        t.cgroup_proc_memory(allowedStats);
        t.cgroup_proc_tasks(elapsed_sec, cfg.m_nOutputFields /* emit JSON */, false /* do not include threads */);
        t.cgroup_proc_tasks(elapsed_sec, cfg.m_nOutputFields /* emit JSON */, true /* do include threads */);
        actual_output.push_current_sample();
    }
    actual_output.psample_array_end();

    // TODO: compare produced JSON with expected JSON

    ASSERT_EQ(CMonitorLogger::instance()->get_num_errors(), 0);
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
