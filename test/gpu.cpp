#include "catch.hpp"

#if defined(U_WITH_BOOST) && defined (U_WITH_OPENCL) 

#include "gpu.hpp"

TEST_CASE("TestClInit") {
    concurrent::CL cl;
}

#endif