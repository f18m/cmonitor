//------------------------------------------------------------------------------
// GTest for FastFileReader
//------------------------------------------------------------------------------

#include "../fast_file_reader.h"
#include <gtest/gtest.h>

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
TEST(FastFileReader, basic_read)
{
    FastFileReader r("/proc/stat");
    ASSERT_TRUE(r.open_or_rewind());

    size_t nlines = 0;
    const char* p = r.get_next_line();
    while (p) {
        p = r.get_next_line();
        nlines++;
    }

    // proc/stat length depends on the number of CPUs available, so its number of lines varies
    std::cout << "FastFileReader: read " << nlines << "lines from /proc/stat" << std::endl;
    ASSERT_TRUE(nlines > 0 && nlines < 1024);
}

TEST(FastFileReader, read_multiple_times)
{
    FastFileReader r("/proc/stat");

    uint64_t prev_hash = 0, curr_hash = 0;
    std::hash<std::string> hasher;
    for (unsigned int i = 0; i < 10; i++) {

        ASSERT_TRUE(r.open_or_rewind());
        curr_hash = 0;

        // now hash each line
        const char* p = r.get_next_line();
        while (p) {
            curr_hash += hasher(std::string(p));
            p = r.get_next_line();
        }

        // we expect that each time we read the file, it's different:
        std::cout << "FastFileReader: current hash for /proc/stat is " << curr_hash << std::endl;
        ASSERT_TRUE(curr_hash != prev_hash);
        prev_hash = curr_hash;

        // sleep 50msec: enough time for the CPU utilization (and other parameters) inside /proc/stat
        // to change
        usleep(50000);
    }
}
