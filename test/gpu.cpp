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


TEST_CASE("TestClComputeAMD") {
    std::cout << "TestClComputeAMD ->" << std::endl;

    auto platform = cl::Host::WithPlatform(cl::Host::AMD);
    auto context = platform->NewFullContext();

    auto func = [] (boost::compute::context& context, boost::compute::command_queue& queue) {
        using namespace boost;
        using compute::uint_;
        using compute::uint2_;

        size_t n = 10000000;
        compute::default_random_engine rng(queue);
        compute::vector<uint_> vector(n * 2, context);
        rng.generate(vector.begin(), vector.end(), queue);

        BOOST_COMPUTE_FUNCTION(bool, is_in_unit_circle, (const uint2_ point), {
            const float x = point.x / (float) UINT_MAX - 1;
            const float y = point.y / (float) UINT_MAX - 1;

            return (x*x + y*y) < 1.0f;
        });

        compute::buffer_iterator<uint2_> start =
            compute::make_buffer_iterator<uint2_>(vector.get_buffer(), 0);
        compute::buffer_iterator<uint2_> end =
            compute::make_buffer_iterator<uint2_>(vector.get_buffer(), vector.size() / 2);

        size_t count = compute::count_if(start, end, is_in_unit_circle, queue);

        float count_f = static_cast<float>(count);
        std::cout << "count: " << count << " / " << n << std::endl;
        std::cout << "ratio: " << count_f / float(n) << std::endl;
        std::cout << "pi = " << (count_f / float(n)) * 4.0f << std::endl;
    };

    auto cpu = context->CPU(0);
    cpu->Compute(func);

    auto gpu = context->GPU(0);
    gpu->Compute(func);
}

#endif