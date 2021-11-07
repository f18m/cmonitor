//------------------------------------------------------------------------------
// GTest unit tests MAIN file
//------------------------------------------------------------------------------

#include "../cgroups.h"
#include "../logger.h"
#include "../output_frontend.h"
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <linux/limits.h>
#include <malloc.h>
#include <sstream> //std::stringstream

std::string get_unit_test_abs_dir()
{
    static std::string ret;

    if (ret.empty()) {
        char buff[PATH_MAX + 1];
        if (getcwd(buff, sizeof(buff)) == NULL)
            exit(123);
        char actualpath[PATH_MAX + 1];
        char* ptr = realpath(buff, actualpath);

        ret = std::string(ptr) + "/";
    }
    return ret;
}

std::string get_file_string(const std::string& file)
{
    std::ifstream ifs(file);
    return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}

void prepare_sample_dir(std::string kernel_test, unsigned int sampleIdx)
{
    std::string orig_sample_abs_dir = get_unit_test_abs_dir() + kernel_test + "/sample" + std::to_string(sampleIdx);
    std::string orig_sample_tarball = orig_sample_abs_dir + "/sample" + std::to_string(sampleIdx) + ".tar.gz";
    std::string current_sample_abs_dir = get_unit_test_abs_dir() + kernel_test + "/current-sample";

    // decompress tarball
    char buff[1024];
    snprintf(buff, 1024, "/usr/bin/tar -C %s -xf %s", orig_sample_abs_dir.c_str(), orig_sample_tarball.c_str());
    printf("Executing now: %s\n", buff);
    int ret = system(buff);
    if (WIFEXITED(ret) == false || WEXITSTATUS(ret) != 0)
        exit(124);

    printf("Adjusting symlink %s\n", current_sample_abs_dir.c_str());
    unlink(current_sample_abs_dir.c_str());
    if (symlink(orig_sample_abs_dir.c_str(), current_sample_abs_dir.c_str()) != 0)
        exit(125);
}

void run_cmonitor_on_tarball_samples(const std::string& kernel_under_test, const std::string& cgroup_name)
{
    // prepare AUX objects
    std::string result_json_file = get_unit_test_abs_dir() + kernel_under_test + "/result.json";
    std::string expected_json_file = get_unit_test_abs_dir() + kernel_under_test + "/expected.json";
    CMonitorOutputFrontend actual_output(result_json_file);
    actual_output.enable_json_pretty_print();

    CMonitorCollectorAppConfig cfg;
    cfg.m_strCGroupName = cgroup_name;

    CMonitorLogger::instance()->enable_debug();
    CMonitorLogger::instance()->init_error_output_file("stdout");

    double elapsed_sec = 0.1;
    std::set<std::string> allowedStats;

    std::string current_sample_abs_dir = get_unit_test_abs_dir() + kernel_under_test + "/current-sample";
    prepare_sample_dir(kernel_under_test, 1); // prepare before invoking cgroup_init()

    // allocate the class under test:
    CMonitorCgroups t(&cfg, &actual_output);
    t.cgroup_init( // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/memory", // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/cpu,cpuacct", // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/cpuset", // force newline
        current_sample_abs_dir);

    // start feeding fixed, test data
    actual_output.pheader_start();
    actual_output.push_header();
    actual_output.psample_array_start();
    for (unsigned int i = 0; i < 3; i++) {
        printf("\n** Processing sample %d\n", i + 1);
        prepare_sample_dir(kernel_under_test, i + 1);

        actual_output.psample_start();
        t.cgroup_proc_cpuacct(elapsed_sec, true /* emit JSON */);
        t.cgroup_proc_memory(allowedStats);
        t.cgroup_proc_tasks(elapsed_sec, cfg.m_nOutputFields /* emit JSON */, false /* do not include threads */);
        // t.cgroup_proc_tasks(elapsed_sec, cfg.m_nOutputFields /* emit JSON */, true /* do include threads */);

        actual_output.push_current_sample();
    }
    actual_output.psample_array_end();
    actual_output.close();

    // make sure no errors have been found in the processing of files so far
    ASSERT_EQ(CMonitorLogger::instance()->get_num_errors(), 0);

    // now compare produced JSON with expected JSON
    std::string result_json_str = get_file_string(result_json_file);
    std::string expected_json_str = get_file_string(expected_json_file);
    ASSERT_EQ(result_json_str, expected_json_str);
}

TEST(CGroups, centos7_Linux_3_10_0)
{
    run_cmonitor_on_tarball_samples( // force newline
        "centos7-Linux-3.10.0-x86_64", // force newline
        "docker/5ccb1395eef093a837e302c52f8cb633cc276ea7d697151ecc34187db571a3b2");
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
