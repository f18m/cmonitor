//------------------------------------------------------------------------------
// GTest unit tests for UTILITY functions
//------------------------------------------------------------------------------

#include "../utils_misc.h"
#include <gtest/gtest.h>
#include <iostream>
#include <sstream> //std::stringstream

//------------------------------------------------------------------------------
// unit tests
//------------------------------------------------------------------------------

TEST(Utils, format_timestamp)
{
    struct {
        time_t time_instant;
        const char* expected_output;
    } testArray[] = {
        { 1, "1970-01-01T00:00:01.000" }, // force newline
        { 1234567890, "2009-02-13T23:31:30.000" }, // force newline
        { 1639444398, "2021-12-14T01:13:18.000" }, // force newline
        { 1639444399, "2021-12-14T01:13:19.000" }, // force newline
    };

    for (unsigned int i = 0; i < sizeof(testArray) / sizeof(testArray[0]); i++) {
        std::chrono::time_point<std::chrono::system_clock> ts1
            = std::chrono::system_clock::from_time_t(testArray[i].time_instant);
        std::string utcTime;
        format_timestamp(ts1, utcTime);

        ASSERT_EQ(testArray[i].expected_output, utcTime);
    }
}
