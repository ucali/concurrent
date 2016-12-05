#include "catch.hpp"

#if defined(U_WITH_BOOST) && defined (U_WITH_OPENCL) 

#include "gpu.hpp"

using namespace concurrent;

TEST_CASE("TestClInit") {
    std::cout << "TestClInit" << std::endl;

    auto amd = cl::Host::WithPlatform(cl::Host::AMD);
    REQUIRE(amd != nullptr);
    std::cout << amd->platform().name() << std::endl;

    auto intel = cl::Host::WithPlatform(cl::Host::INTEL);
    REQUIRE(intel != nullptr);
    std::cout << intel->platform().name() << std::endl;
}

#endif