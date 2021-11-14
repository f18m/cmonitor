//------------------------------------------------------------------------------
// GTest unit tests MAIN file
//------------------------------------------------------------------------------

#include "../cgroups.h"
#include "../logger.h"
#include "../output_frontend.h"
#include "../utils.h"
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

void prepare_sample_dir(std::string kernel_test, unsigned int sampleIdx, uint64_t& sample_timestamp_nsec)
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

    // update symlink current-sample -> sample{sampleIdx}
    printf("Adjusting symlink %s\n", current_sample_abs_dir.c_str());
    unlink(current_sample_abs_dir.c_str());
    if (symlink(orig_sample_abs_dir.c_str(), current_sample_abs_dir.c_str()) != 0)
        exit(125);

    // read the timestamp for this sample
    sample_timestamp_nsec = std::stoul(get_file_string(current_sample_abs_dir + "/sample-timestamp"));
}

void run_cmonitor_on_tarball_samples(const std::string& test_name, const std::string& kernel_under_test,
    const std::string& cgroup_name, bool include_threads, unsigned int nsamples)
{
    // prepare AUX objects
    std::string result_json_file = get_unit_test_abs_dir() + kernel_under_test + "/result-" + test_name + ".json";
    std::string expected_json_file = get_unit_test_abs_dir() + kernel_under_test + "/expected-" + test_name + ".json";
    CMonitorOutputFrontend actual_output(result_json_file);
    actual_output.enable_json_pretty_print();

    CMonitorCollectorAppConfig cfg;
    cfg.m_strCGroupName = cgroup_name;
    cfg.m_nProcessScoreThreshold = 0;

    CMonitorLogger::instance()->enable_debug();
    CMonitorLogger::instance()->init_error_output_file("stdout");

    std::set<std::string> allowedStats;

    std::string current_sample_abs_dir = get_unit_test_abs_dir() + kernel_under_test + "/current-sample";
    uint64_t prev_ts;
    prepare_sample_dir(kernel_under_test, 1, prev_ts); // prepare before invoking cgroup_init()

    // allocate the class under test:
    CMonitorCgroups t(&cfg, &actual_output);
    t.cgroup_init( // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/memory", // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/cpu,cpuacct", // force newline
        current_sample_abs_dir + "/sys/fs/cgroup/cpuset", // force newline
        current_sample_abs_dir);

    // start feeding fixed, test data
    actual_output.pheader_start();
    t.output_config();
    actual_output.push_header();
    actual_output.psample_array_start();
    for (unsigned int i = 0; i < nsamples; i++) {
        uint64_t curr_ts;

        printf("\n** Processing sample %d\n", i + 1);
        prepare_sample_dir(kernel_under_test, i + 1, curr_ts);
        double elapsed_sec = (curr_ts - prev_ts) * (1e-9);
        printf("Elapsed time: %.6fsec\n", elapsed_sec);

        // finally run the code to test
        actual_output.psample_start();
        t.cgroup_proc_cpuacct(elapsed_sec);
        t.cgroup_proc_memory(allowedStats);
        t.cgroup_proc_tasks(
            elapsed_sec, cfg.m_nOutputFields /* emit JSON */, include_threads /* do not include threads */);

        actual_output.push_current_sample();
        prev_ts = curr_ts;
    }
    actual_output.psample_array_end();
    actual_output.close();

    // make sure no errors have been found in the processing of files so far
    ASSERT_EQ(CMonitorLogger::instance()->get_num_errors(), 0);

    // now read back the resulting JSON... but hide/mask-out the precise location of the unit testing data;
    // that's because on the developer machine this will be an absolute path like
    //     /home/youruser/myprojects/git/cmonitor/collector/src/tests/centos7-Linux-3.10.0-x86_64/
    // while in the CI/CD pipeline it will be something like:
    //     /home/runner/work/cmonitor/cmonitor/collector/src/tests/centos7-Linux-3.10.0-x86_64/
    std::string result_json_str = get_file_string(result_json_file);
    replace_string(
        result_json_str, current_sample_abs_dir, "/removed-unit-test-data-location", true /* allOccurrences */);

    // now compare produced JSON with expected JSON
    std::string expected_json_str = get_file_string(expected_json_file);
    ASSERT_EQ(result_json_str, expected_json_str);
}

TEST(CGroups, centos7_Linux_3_10_0_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "centos7-Linux-3.10.0-x86_64", // force newline
        "docker/5ccb1395eef093a837e302c52f8cb633cc276ea7d697151ecc34187db571a3b2", false /* no threads */,
        4 /* nsamples */);
}
TEST(CGroups, centos7_Linux_3_10_0_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "centos7-Linux-3.10.0-x86_64", // force newline
        "docker/5ccb1395eef093a837e302c52f8cb633cc276ea7d697151ecc34187db571a3b2", true /* with threads */,
        4 /* nsamples */);
}
TEST(CGroups, ubuntu2004_Linux_5_4_0_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "ubuntu20.04-Linux-5.4.0-x86_64", // force newline
        "docker//938cbdc624d3af04e6e75ed6ace47c5155276353cb36aa7ee9cc1e52cc10fa6a", false /* with threads */,
        4 /* nsamples */);
}
TEST(CGroups, ubuntu2004_Linux_5_4_0_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "ubuntu20.04-Linux-5.4.0-x86_64", // force newline
        "docker//938cbdc624d3af04e6e75ed6ace47c5155276353cb36aa7ee9cc1e52cc10fa6a", true /* with threads */,
        4 /* nsamples */);
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
