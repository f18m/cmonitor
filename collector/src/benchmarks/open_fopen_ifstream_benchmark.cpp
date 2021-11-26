//------------------------------------------------------------------------------
// Benchmark tests MAIN file
/*
	Last run done on 22 Nov 2021 showed that "BM_open_syscall_with_rewind"
	is the fastest solution on small & larger files... of course the gap
	with other solutions becomes smaller as file gets larger (larger number after '/')

	Running ../../bin/glibc/benchmark_tests
	Run on (40 X 3112.06 MHz CPU s)
	CPU Caches:
	L1 Data 32 KiB (x20)
	L1 Instruction 32 KiB (x20)
	L2 Unified 256 KiB (x20)
	L3 Unified 25600 KiB (x2)
	Load Average: 1.38, 1.40, 1.38
	------------------------------------------------------------------------
	Benchmark                              Time             CPU   Iterations
	------------------------------------------------------------------------
	BM_open_syscall/0                   6193 ns         6136 ns       109720
	BM_open_syscall/1                   3997 ns         3939 ns       177747
	BM_open_syscall/2                   3787 ns         3724 ns       187590
	BM_open_syscall/3                   3632 ns         3571 ns       194074
	BM_open_syscall/4                  26348 ns        26272 ns        26630
	BM_open_syscall/5                  55929 ns        55557 ns        12530
	BM_open_syscall/6                 387032 ns       385802 ns         1795
	BM_open_syscall_with_rewind/0       3325 ns         3325 ns       210355 <-- winner
	BM_open_syscall_with_rewind/1       1485 ns         1485 ns       470774 <-- winner
	BM_open_syscall_with_rewind/2       1289 ns         1289 ns       543043 <-- winner
	BM_open_syscall_with_rewind/3       1306 ns         1306 ns       535131 <-- winner
	BM_open_syscall_with_rewind/4      23146 ns        23146 ns        30199 <-- winner
	BM_open_syscall_with_rewind/5      50405 ns        50405 ns        13859 <-- winner
	BM_open_syscall_with_rewind/6     379598 ns       379597 ns         1859 <-- winner
	BM_fopen/0                         15618 ns        15559 ns        44681
	BM_fopen/1                         12553 ns        12491 ns        55987
	BM_fopen/2                         11862 ns        11804 ns        57951
	BM_fopen/3                         11509 ns        11453 ns        61336
	BM_fopen/4                         35458 ns        35393 ns        19723
	BM_fopen/5                         71680 ns        71333 ns         9779
	BM_fopen/6                        457606 ns       456195 ns         1547
	BM_fopen_with_rewind/0              4047 ns         4047 ns       173361
	BM_fopen_with_rewind/1              2110 ns         2110 ns       331927
	BM_fopen_with_rewind/2              2066 ns         2066 ns       339410
	BM_fopen_with_rewind/3              2085 ns         2085 ns       335047
	BM_fopen_with_rewind/4             24877 ns        24876 ns        28088
	BM_fopen_with_rewind/5             56394 ns        56394 ns        12489
	BM_fopen_with_rewind/6            426895 ns       426894 ns         1655
	BM_ifstream/0                       9760 ns         9703 ns        71989
	BM_ifstream/1                       6945 ns         6886 ns       101208
	BM_ifstream/2                       6625 ns         6567 ns       105938
	BM_ifstream/3                       6347 ns         6289 ns       111658
	BM_ifstream/4                      30881 ns        30820 ns        22642
	BM_ifstream/5                      61748 ns        61367 ns        11338
	BM_ifstream/6                     453834 ns       451524 ns         1548
*/
//------------------------------------------------------------------------------

#include <benchmark/benchmark.h> // "google-benchmark-devel" RPM (or similar package) is required
#include <fcntl.h> // open()
#include <fstream> // std::ifstream
#include <stdio.h> // fopen()
#include <string.h> // strchr()
#include <unistd.h> // read()

/*
        cmonitor_collector will typically access
        a) very small mono-line files like e.g.
                 /proc/self/stat
                 /proc/self/statm
                 /sys/fs/cgroup/.../cpuset.cpus
                 /sys/fs/cgroup/.../cpuacct.usage_percpu_sys
                 /sys/fs/cgroup/.../memory.faicnt
                 etc etc
        b) longer multiline files like e.g.
                 /proc/self/status
                 /proc/stat
                 /proc/net/dev
*/
#define NUM_FILES 6
const char* g_files_to_test[] = {
    "/proc/self/stat", // 0
    "/proc/self/statm", // 1
    "/sys/fs/cgroup/cpuacct/cpu.cfs_quota_us", // 2
    "/sys/fs/cgroup/memory/memory.failcnt", // 3
    "/proc/self/status", // 4
    "/proc/stat", // 5
    "/proc/net/dev", // 6
};

// file /proc/stat is pretty large, 8k is not enough, so we use 16k:
#define MAX_FILE_SIZE 16384

//------------------------------------------------------------------------------
// dummy_char_processor
//------------------------------------------------------------------------------

int dummy_acc;
static int* dummy __attribute__((__used__))
= &dummy_acc; // this trick is to avoid compiler optimizing dummy_acc and all dummy_char_processor() away

void dummy_char_processor(const char* pline)
{
    size_t n = strlen(pline);

    // just access
    for (size_t i = 0; i < n; i++)
        dummy_acc &= pline[i];
}

//------------------------------------------------------------------------------
// BM_open_syscall
//------------------------------------------------------------------------------

static void read_whole_file(int fp, char* buf)
{
    ssize_t nread = read(fp, buf, MAX_FILE_SIZE);
    if (nread <= 0 || nread >= (ssize_t)MAX_FILE_SIZE)
        assert(0); // we expect a non-zero value less than the "buf" size
    buf[nread] = '\0'; // add NUL termination
}

static void process_each_line_of_buffer(char* buf)
{
    // process line by line
    char* pline_start = buf;
    int i = 0;
    while (pline_start < buf + MAX_FILE_SIZE) {
        char* pline_end = strchr(pline_start, '\n');
        if (pline_end == NULL) // no more newlines
            break;
        *pline_end = '\0'; // replace the newline with NUL terminator

        //...cmonitor_collector processing happens here between pline_start/end
        dummy_char_processor(pline_start);

        pline_start = pline_end + 1;
        i++;
    }
}

static void BM_open_syscall(benchmark::State& state)
{
    int fp;
    char buf[MAX_FILE_SIZE];

    const char* pfile = g_files_to_test[state.range(0)];
    for (auto _ : state) {
        if ((fp = open(pfile, O_RDONLY)) == -1)
            assert(0);
        read_whole_file(fp, buf);
        process_each_line_of_buffer(buf);
        close(fp);
    }
}
BENCHMARK(BM_open_syscall)->DenseRange(0, NUM_FILES, 1);

//------------------------------------------------------------------------------
// BM_open_syscall_with_rewind
// Out of this benchmark test FastFileReader class has been written
//------------------------------------------------------------------------------

static void BM_open_syscall_with_rewind(benchmark::State& state)
{
    int fp = -1;
    char buf[MAX_FILE_SIZE];

    const char* pfile = g_files_to_test[state.range(0)];
    for (auto _ : state) {
        if (fp == -1) {
            // only on first run do open the file with open()
            if ((fp = open(pfile, O_RDONLY)) == -1)
                assert(0);
        } else {
            lseek(fp, 0, SEEK_SET);
        }

        read_whole_file(fp, buf);
        process_each_line_of_buffer(buf);
    }

    // close only when exiting
    close(fp);
}
BENCHMARK(BM_open_syscall_with_rewind)->DenseRange(0, NUM_FILES, 1);

//------------------------------------------------------------------------------
// BM_fopen
//------------------------------------------------------------------------------

static void BM_fopen(benchmark::State& state)
{
    FILE* fp;
    char buf[MAX_FILE_SIZE];

    const char* pfile = g_files_to_test[state.range(0)];
    for (auto _ : state) {
        if ((fp = fopen(pfile, "r")) == NULL)
            assert(0);

        for (int i = 0;; i++) {
            if (fgets(buf, 1024, fp) == NULL) {
                break;
            }

            //...cmonitor_collector processing happens here...
            dummy_char_processor(buf);
        }
        fclose(fp);
    }
}
BENCHMARK(BM_fopen)->DenseRange(0, NUM_FILES, 1);

//------------------------------------------------------------------------------
// BM_fopen_with_rewind
//------------------------------------------------------------------------------

static void BM_fopen_with_rewind(benchmark::State& state)
{
    FILE* fp = NULL;
    char buf[MAX_FILE_SIZE];

    const char* pfile = g_files_to_test[state.range(0)];
    for (auto _ : state) {
        // only on first run do open the file with fopen()
        if (fp == NULL) {
            if ((fp = fopen(pfile, "r")) == NULL)
                assert(0);
        } else {
            rewind(fp);
        }

        for (int i = 0;; i++) {
            if (fgets(buf, 1024, fp) == NULL) {
                break;
            }

            //...cmonitor_collector processing happens here...
            dummy_char_processor(buf);
        }
    }

    // close only when exiting
    fclose(fp);
}
BENCHMARK(BM_fopen_with_rewind)->DenseRange(0, NUM_FILES, 1);

//------------------------------------------------------------------------------
// BM_ifstream
//------------------------------------------------------------------------------

static void BM_ifstream(benchmark::State& state)
{
    for (auto _ : state) {

        std::ifstream inputf(g_files_to_test[state.range(0)]);
        if (!inputf.is_open())
            assert(0);

        std::string line;
        while (std::getline(inputf, line))
            dummy_char_processor(line.c_str());
    }
}
BENCHMARK(BM_ifstream)->DenseRange(0, NUM_FILES, 1);

BENCHMARK_MAIN();
