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
#include <malloc.h>
#include <sstream> //std::stringstream

//------------------------------------------------------------------------------
// GTest helpers
//------------------------------------------------------------------------------

#define PATH_MAX (4096)

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

void write_file_string(const std::string& filename, const std::string& str)
{
    std::ofstream out(filename);
    out << str;
    out.close();
}

unsigned int replace_string_in_file(
    const std::string& filename, const std::string& from, const std::string& to, bool allOccurrences)
{
    std::string contents_str = get_file_string(filename);
    unsigned int noccurrences = replace_string(contents_str, from, to, allOccurrences);
    write_file_string(filename, contents_str);

    return noccurrences;
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
        current_sample_abs_dir, current_sample_abs_dir);

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
        t.cgroup_proc_memory(allowedStats, allowedStats);
        t.cgroup_proc_tasks(elapsed_sec, cfg.m_nOutputFields, include_threads);
        t.cgroup_proc_network_interfaces(elapsed_sec, cfg.m_nOutputFields);

        actual_output.push_current_sample();
        prev_ts = curr_ts;
    }
    actual_output.psample_array_end();
    actual_output.close();

    // make sure no errors have been found in the processing of files so far
    ASSERT_EQ(CMonitorLogger::instance()->get_num_errors(), 0);

    // now before reading back the resulting JSON hide/mask-out the precise location of the unit testing data;
    // that's because on the developer machine this will be an absolute path like
    //     /home/youruser/myprojects/git/cmonitor/collector/src/tests/centos7-Linux-3.10.0-x86_64/
    // while in the CI/CD pipeline it will be something like:
    //     /home/runner/work/cmonitor/cmonitor/collector/src/tests/centos7-Linux-3.10.0-x86_64/
    replace_string_in_file(
        result_json_file, current_sample_abs_dir, "/removed-unit-test-data-location", true /* allOccurrences */);

    // now compare produced JSON with expected JSON
    std::string result_json_str = get_file_string(result_json_file);
    std::string expected_json_str = get_file_string(expected_json_file);
    ASSERT_EQ(result_json_str, expected_json_str);
}

//------------------------------------------------------------------------------
// unit tests on cgroups v1
//------------------------------------------------------------------------------
TEST(CGroups, centos7_Linux_3_10_0_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "centos7-Linux-3.10.0-x86_64", // force newline
        "docker/27d5147ebb2620bfd9c20f728e0785f55e523efd0bb25a1a8e225c7fa9e0e335", false /* no threads */,
        4 /* nsamples */);
}
TEST(CGroups, centos7_Linux_3_10_0_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "centos7-Linux-3.10.0-x86_64", // force newline
        "docker/27d5147ebb2620bfd9c20f728e0785f55e523efd0bb25a1a8e225c7fa9e0e335", true /* with threads */,
        4 /* nsamples */);
}
#if 0
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
// unit tests on cgroups v2
//------------------------------------------------------------------------------

TEST(CGroups, fedora35_Linux_5_14_17_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "fedora35-Linux-5.14.17-x86_64", // force newline
        "sys/fs/cgroup/system.slice/docker-573203c86cacbab444fed316a0e25aa9f017144cd3def79a91684d1a63c51419.scope/",
        false /* with threads */, 4 /* nsamples */);
}
TEST(CGroups, fedora35_Linux_5_14_17_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "fedora35-Linux-5.14.17-x86_64", // force newline
        "sys/fs/cgroup/system.slice/docker-573203c86cacbab444fed316a0e25aa9f017144cd3def79a91684d1a63c51419.scope/",
        true /* with threads */, 4 /* nsamples */);
}
#endif

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
#ifdef __GLIBC__
    // Make uses of freed and uninitialized memory known.
    mallopt(M_PERTURB, 42);
#endif
    // RUN ALL GTESTS
    testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS() != 0)
        return 1;

    std::cout << "All GoogleTest testsuites passed." << std::endl;

    return 0;
}
