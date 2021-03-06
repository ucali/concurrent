cmake_minimum_required(VERSION 3.6.0)
project(concurrent C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(THREADS_PREFER_PTHREAD_FLAG ON)

include_directories(include test/Catch/include)
file(GLOB SOURCES test/*.cpp)

add_definitions(-DNOMINMAX)

find_package(Threads REQUIRED)

add_executable(test_concurrent ${SOURCES})

target_link_libraries(test_concurrent Threads::Threads)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

MESSAGE("Searching Boost in: " $ENV{BOOST_ROOT})
find_package(Boost 1.62.0 COMPONENTS fiber context)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(test_concurrent ${Boost_LIBRARIES})
   
    add_definitions(-DU_WITH_BOOST)
endif()

find_package(OpenCL)
if (OpenCL_FOUND)
    include_directories(${OpenCL_INCLUDE_DIRS})
    target_link_libraries(test_concurrent ${OpenCL_LIBRARY})
    
    add_definitions(-DBOOST_COMPUTE_THREAD_SAFE)
    add_definitions(-DU_WITH_OPENCL)
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/bin" CACHE PATH "default install path" FORCE )
endif()

if (CMAKE_BUILD_TYPE_INITIALIZED_TO_DEFAULT)
   set(CMAKE_BUILD_TYPE Release)
endif()

