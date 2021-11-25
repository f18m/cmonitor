//------------------------------------------------------------------------------
// GTest unit tests MAIN file
//------------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <malloc.h>

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
