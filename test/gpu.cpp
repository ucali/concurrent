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

auto rand_sample = [] (auto& context, auto& queue) {
    //From compute samples:
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

auto kmeans = [] (auto& context, auto& queue) {
    
    using namespace boost;
    using compute::dim;
    using compute::int_;
    using compute::float_;
    using compute::float2_;

    size_t k = 6;

    // number of points
    size_t n_points = 1024*1024;

    // height and width of image
    size_t height = 1024;
    size_t width = 1024;

    compute::default_random_engine random_engine(queue);
    compute::uniform_real_distribution<float_> uniform_distribution(0, 800);

    compute::vector<float2_> points(n_points, context);
    uniform_distribution.generate(
        compute::make_buffer_iterator<float_>(points.get_buffer(), 0),
        compute::make_buffer_iterator<float_>(points.get_buffer(), n_points * 2),
        random_engine,
        queue
    );

    // initialize all points to cluster 0
    compute::vector<int_> clusters(n_points, context);
    compute::fill(clusters.begin(), clusters.end(), 0, queue);

    // create initial means with the first k points
    compute::vector<float2_> means(k, context);
    compute::copy_n(points.begin(), k, means.begin(), queue);

    // k-means clustering program source
    const char k_means_source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void assign_clusters(__global const float2 *points,
                                      __global const float2 *means,
                                      const int k,
                                      __global int *clusters)
        {
            const uint gid = get_global_id(0);

            const float2 point = points[gid];

            // find the closest cluster
            float current_distance = 0;
            int closest_cluster = -1;

            // find closest cluster mean to the point
            for(int i = 0; i < k; i++){
                const float2 mean = means[i];

                int distance_to_mean = distance(point, mean);
                if(closest_cluster == -1 || distance_to_mean < current_distance){
                    current_distance = distance_to_mean;
                    closest_cluster = i;
                }
            }

            // write new cluster
            clusters[gid] = closest_cluster;
        }

        __kernel void update_means(__global const float2 *points,
                                   const uint n_points,
                                   __global float2 *means,
                                   __global const int *clusters)
        {
            const uint k = get_global_id(0);

            float2 sum = { 0, 0 };
            float count = 0;
            for(uint i = 0; i < n_points; i++){
                if(clusters[i] == k){
                    sum += points[i];
                    count += 1;
                }
            }

            means[k] = sum / count;
        }
    );

    // build the k-means program
    compute::program k_means_program =
        compute::program::build_with_source(k_means_source, context);

    // setup the k-means kernels
    compute::kernel assign_clusters_kernel(k_means_program, "assign_clusters");
    assign_clusters_kernel.set_arg(0, points);
    assign_clusters_kernel.set_arg(1, means);
    assign_clusters_kernel.set_arg(2, int_(k));
    assign_clusters_kernel.set_arg(3, clusters);

    compute::kernel update_means_kernel(k_means_program, "update_means");
    update_means_kernel.set_arg(0, points);
    update_means_kernel.set_arg(1, int_(n_points));
    update_means_kernel.set_arg(2, means);
    update_means_kernel.set_arg(3, clusters);

    // run the k-means algorithm
    for(int iteration = 0; iteration < 25; iteration++){
        queue.enqueue_1d_range_kernel(assign_clusters_kernel, 0, n_points, 0);
        queue.enqueue_1d_range_kernel(update_means_kernel, 0, k, 0);
    }

    // create output image
    compute::image2d image(
        context, width, height, compute::image_format(CL_RGBA, CL_UNSIGNED_INT8)
    );

    // program with two kernels, one to fill the image with white, and then
    // one the draw to points calculated in coordinates on the image
    const char draw_walk_source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void draw_points(__global const float2 *points,
                                  __global const int *clusters,
                                  __write_only image2d_t image)
        {
            const uint i = get_global_id(0);
            const float2 coord = points[i];

            // map cluster number to color
            uint4 color = { 0, 0, 0, 0 };
            switch(clusters[i]){
              case 0:
                  color = (uint4)(255, 0, 0, 255);
                  break;
              case 1:
                  color = (uint4)(0, 255, 0, 255);
                  break;
              case 2:
                  color = (uint4)(0, 0, 255, 255);
                  break;
              case 3:
                  color = (uint4)(255, 255, 0, 255);
                  break;
              case 4:
                  color = (uint4)(255, 0, 255, 255);
                  break;
              case 5:
                  color = (uint4)(0, 255, 255, 255);
                  break;
            }

            // draw a 3x3 pixel point
            for(int x = -1; x <= 1; x++){
                for(int y = -1; y <= 1; y++){
                    if(coord.x + x > 0 && coord.x + x < get_image_width(image) &&
                       coord.y + y > 0 && coord.y + y < get_image_height(image)){
                        write_imageui(image, (int2)(coord.x, coord.y) + (int2)(x, y), color);
                    }
                }
            }
        }

        __kernel void fill_gray(__write_only image2d_t image)
        {
            const int2 coord = { get_global_id(0), get_global_id(1) };

            if(coord.x < get_image_width(image) && coord.y < get_image_height(image)){
                uint4 gray = { 15, 15, 15, 15 };
                write_imageui(image, coord, gray);
            }
        }
    );

    // build the program
    compute::program draw_program =
        compute::program::build_with_source(draw_walk_source, context);

    // fill image with dark gray
    compute::kernel fill_kernel(draw_program, "fill_gray");
    fill_kernel.set_arg(0, image);

    queue.enqueue_nd_range_kernel(
        fill_kernel, dim(0, 0), dim(width, height), dim(1, 1)
    );

    // draw points colored according to cluster
    compute::kernel draw_kernel(draw_program, "draw_points");
    draw_kernel.set_arg(0, points);
    draw_kernel.set_arg(1, clusters);
    draw_kernel.set_arg(2, image);
    queue.enqueue_1d_range_kernel(draw_kernel, 0, n_points, 0);
    queue.flush();

};

TEST_CASE("TestClComputeAMD") {
    std::cout << "TestClComputeAMD ->" << std::endl;

    auto platform = cl::Host::WithPlatform(cl::Host::AMD);
    auto context = platform->NewFullContext();

    auto cpu = context->CPU(0);

    auto gpu = context->GPU(0);
    gpu->Compute([] (auto& context, auto& queue) {
        rand_sample(context, queue);
        std::cout << "GPU DONE" << std::endl;
    });
    cpu->Compute([] (auto& context, auto& queue) {
        rand_sample(context, queue);
        std::cout << "CPU DONE" << std::endl;
    });
}

TEST_CASE("TestSharedBridge") {
    std::cout << "TestSharedBridge ->" << std::endl;
    cl::SharedComputeBridge bridge(cl::Host::AMD);
    bridge.ComputeOnGPU([] (auto& context, auto& queue) {
        kmeans(context, queue);
        queue.finish();
        std::cout << "GPU DONE" << std::endl;
    });
    bridge.ComputeOnCPU([] (auto& context, auto& queue) {
        kmeans(context, queue);
        queue.finish();
        std::cout << "CPU DONE" << std::endl;
    });
    bridge.Close();

}

TEST_CASE("TestIndipendentBridge") {
    std::cout << "TestIndipendentBridge ->" << std::endl;
    cl::IndipendentComputeBridge bridge(cl::Host::AMD);
    bridge.ComputeOnGPU([] (auto& context, auto& queue) {
        kmeans(context, queue);
        queue.finish();
        std::cout << "GPU DONE" << std::endl;
    });
    bridge.ComputeOnCPU([] (auto& context, auto& queue) {
        kmeans(context, queue);
        queue.finish();
        std::cout << "CPU DONE" << std::endl;
    });
    bridge.Close();

}

#endif