#include "catch.hpp"

#if defined(U_WITH_BOOST) && defined (U_WITH_OPENCL) 

#include "gpu.hpp"

using namespace concurrent;

TEST_CASE("TestClInit") {
    std::cout << "TestClInit ->" << std::endl;

    auto platform = cl::Host::WithPlatform(cl::Host::AMD);
    REQUIRE(platform != nullptr);
    std::cout << platform->platform().name() << std::endl;

    platform = cl::Host::WithPlatform(cl::Host::INTEL);
    REQUIRE(platform != nullptr);
    std::cout << platform->platform().name() << std::endl;
}

TEST_CASE("TestClContext") {
    std::cout << "TestClContext ->" << std::endl;

    auto platform = cl::Host::WithPlatform(cl::Host::AMD);
    auto context = platform->NewFullContext();

    auto cpu = context->CPU(0);
    REQUIRE(cpu != nullptr);
    std::cout << cpu->device().name() << std::endl;

    auto gpu = context->GPU(0);
    REQUIRE(gpu != nullptr);
    std::cout << gpu->device().name() << std::endl;
}

#endif