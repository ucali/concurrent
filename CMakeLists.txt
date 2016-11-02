cmake_minimum_required(VERSION 3.1.0)
project(concurrent C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(THREADS_PREFER_PTHREAD_FLAG ON)

include_directories(include test/Catch/include)
file(GLOB SOURCES test/*.cpp)

find_package(Threads REQUIRED)

set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
find_package(Boost 1.62.0 COMPONENTS fiber) 
include_directories(${Boost_INCLUDE_DIRS}) 

add_executable(test_concurrent ${SOURCES})
target_link_libraries(test_concurrent Threads::Threads ${Boost_LIBRARIES})

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/output" CACHE PATH "default install path" FORCE )
endif()

if (CMAKE_BUILD_TYPE_INITIALIZED_TO_DEFAULT)
   set(CMAKE_BUILD_TYPE Release)
endif()

