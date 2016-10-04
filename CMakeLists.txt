cmake_minimum_required(VERSION 3.1.0)
project(concurrent CXX)
set (CMAKE_CXX_STANDARD 14)

include_directories(include test/Catch/include)
file(GLOB SOURCES test/*.cpp)

add_executable(test_concurrent ${SOURCES})

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/output" CACHE PATH "default install path" FORCE )
endif()

if (CMAKE_BUILD_TYPE_INITIALIZED_TO_DEFAULT)
   set(CMAKE_BUILD_TYPE Release)
endif

