//------------------------------------------------------------------------------
// GTest unit tests for CGROUPs
//------------------------------------------------------------------------------

#include "../cgroups.h"
#include "../logger.h"
#include "../output_frontend.h"
#include "../utils.h"
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
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
    printf("GTEST SETUP: executing now: %s\n", buff);
    int ret = system(buff);
    if (WIFEXITED(ret) == false || WEXITSTATUS(ret) != 0)
        exit(124);

    // update symlink current-sample -> sample{sampleIdx}
    printf("GTEST SETUP: adjusting symlink %s\n", current_sample_abs_dir.c_str());
    unlink(current_sample_abs_dir.c_str());
    if (symlink(orig_sample_abs_dir.c_str(), current_sample_abs_dir.c_str()) != 0)
        exit(125);

    // read the timestamp for this sample
    sample_timestamp_nsec = std::stoul(get_file_string(current_sample_abs_dir + "/sample-timestamp"));
}

void run_cmonitor_on_tarball_samples( // fn
    /* config params */
    const std::string& test_name, const std::string& kernel_under_test, const std::string& cgroup_name,
    bool include_threads, unsigned int nsamples, uint64_t simulated_cmonitor_collector_pid,
    /* expected */
    CGroupDetected expected_cgroup_ver = CG_VERSION1, uint64_t num_logged_errors = 0)
{
    // reset number of logged errors to keep each gtest isolated
    CMonitorLogger::instance()->reset_num_errors();

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

    // the PID provided must exist inside the sample directory
    ASSERT_TRUE(file_or_dir_exists(fmt::format("{}/proc/{}", current_sample_abs_dir, simulated_cmonitor_collector_pid).c_str()));

    printf(" --- now starting actual CMonitorGroups code under test ---\n");

    // allocate the class under test:
    // NOTE: the 'simulated_cmonitor_collector_pid' PID is important to remove ANY dependency of CMonitorGroups code
    //       from the PID of this gtest executable and make sure it never uses stuff like /proc/self/mounts but rather
    //       reads some unit-test-data file instead
    CMonitorCgroups t(&cfg, &actual_output);
    t.init(include_threads, current_sample_abs_dir, current_sample_abs_dir, simulated_cmonitor_collector_pid);
    ASSERT_EQ(t.get_detected_cgroup_version(), expected_cgroup_ver);

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
        t.sample_cpuacct(elapsed_sec);
        t.sample_memory(allowedStats, allowedStats);
        t.sample_processes(elapsed_sec, cfg.m_nOutputFields);
        t.sample_network_interfaces(elapsed_sec, cfg.m_nOutputFields);

        actual_output.push_current_sample();
        prev_ts = curr_ts;
    }
    actual_output.psample_array_end();
    actual_output.close();

    printf(" --- completed running code to test, now starting result verification ---\n");

    // make sure no errors have been found in the processing of files so far
    ASSERT_EQ(CMonitorLogger::instance()->get_num_errors(), num_logged_errors);

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
TEST(CGroups, centos7_Linux_3_10_0_docker_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "centos7-Linux-3.10.0-x86_64-docker", // force newline
        "docker/d20c1d74e74b4ee40954136e18d33ea85d7333dda4dca0161806395c2d26913c", false /* no threads */,
        4 /* nsamples */,
        1232906  /* simulated_cmonitor_collector_pid: in reality it's the PID of a REDIS but fits just fine our testing purposes */);
}
TEST(CGroups, centos7_Linux_3_10_0_docker_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "centos7-Linux-3.10.0-x86_64-docker", // force newline
        "docker/d20c1d74e74b4ee40954136e18d33ea85d7333dda4dca0161806395c2d26913c", true /* with threads */,
        4 /* nsamples */,
        1232906  /* simulated_cmonitor_collector_pid: in reality it's the PID of a REDIS but fits just fine our testing purposes */);
}

TEST(CGroups, centos7_Linux_3_10_0_systemd_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "centos7-Linux-3.10.0-x86_64-systemd", // force newline
        "self" /* cgroup name: ask to autodetect cgroup under monitor */, false /* no threads */, 4 /* nsamples */, // fn
        775367 /* simulated_cmonitor_collector_pid: in reality it's the PID of a Bash but fits just fine our testing purposes */);
}

TEST(CGroups, centos7_Linux_3_10_0_systemd_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline 
        "centos7-Linux-3.10.0-x86_64-systemd", // force newline
        "self" /* cgroup name: ask to autodetect cgroup under monitor */, true /* with threads */, 4 /* nsamples */, // fn
        775367 /* simulated_cmonitor_collector_pid: in reality it's the PID of a Bash but fits just fine our testing purposes */);
}

#if 0 // FIXME reenable
TEST(CGroups, ubuntu2004_Linux_5_4_0_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "ubuntu20.04-Linux-5.4.0-x86_64", // force newline
        "docker//fffe499793dc451b96e4d8628adfcd762d1a8177d8627d8e879c56ca093bc7ef", false /* with threads */,
        4 /* nsamples */,
        2525  /* simulated_cmonitor_collector_pid: in reality it's the PID of a REDIS but fits just fine our testing purposes */);
}
TEST(CGroups, ubuntu2004_Linux_5_4_0_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "ubuntu20.04-Linux-5.4.0-x86_64", // force newline
        "docker//fffe499793dc451b96e4d8628adfcd762d1a8177d8627d8e879c56ca093bc7ef", true /* with threads */,
        4 /* nsamples */,
        2525  /* simulated_cmonitor_collector_pid: in reality it's the PID of a REDIS but fits just fine our testing purposes */);
}
#endif

//------------------------------------------------------------------------------
// unit tests on cgroups v2
//------------------------------------------------------------------------------

TEST(CGroups, fedora35_Linux_5_14_17_docker_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "fedora35-Linux-5.14.17-x86_64-docker", // force newline
        "system.slice/docker-3cfe7ca058f43dbb15a6cc68c472978a14c93fd7e263384dd0a1fa1517f6d7f0.scope/",
        false /* with threads */, 4 /* nsamples */,
        3834 /* pid of a process inside the docker to correctly autodetect the cgroups v2 */, CG_VERSION2);
}
TEST(CGroups, fedora35_Linux_5_14_17_docker_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "fedora35-Linux-5.14.17-x86_64-docker", // force newline
        "system.slice/docker-3cfe7ca058f43dbb15a6cc68c472978a14c93fd7e263384dd0a1fa1517f6d7f0.scope/",
        true /* with threads */, 4 /* nsamples */,
        3834 /* pid of a process inside the docker to correctly autodetect the cgroups v2 */, CG_VERSION2);
}

TEST(CGroups, fedora35_Linux_5_14_17_systemd_nothreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "nothreads", // force newline
        "fedora35-Linux-5.14.17-x86_64-systemd", // force newline
        "self" /* cgroup name: ask to autodetect cgroup under monitor */, false /* with threads */, 4 /* nsamples */,
        1003, /* simulated_cmonitor_collector_pid: in reality it's the PID of a SSHD but fits just fine our testing
                purposes */
        CG_VERSION2, 2 /* num_logged_errors: absence of cpu.max and cpuset.cpus */);
}
TEST(CGroups, fedora35_Linux_5_14_17_systemd_withthreads)
{
    run_cmonitor_on_tarball_samples( // force newline
        "withthreads", // force newline
        "fedora35-Linux-5.14.17-x86_64-systemd", // force newline
        "self" /* cgroup name: ask to autodetect cgroup under monitor */, true /* with threads */, 4 /* nsamples */,
        1003, /* simulated_cmonitor_collector_pid: in reality it's the PID of a SSHD but fits just fine our testing
                purposes */
        CG_VERSION2, 2 /* num_logged_errors: absence of cpu.max and cpuset.cpus */);
}
