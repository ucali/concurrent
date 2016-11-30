cmake_minimum_required(VERSION 3.1.0)
project(concurrent C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(THREADS_PREFER_PTHREAD_FLAG ON)

include_directories(include test/Catch/include)
file(GLOB SOURCES test/*.cpp)

add_definitions(-DNOMINMAX)

find_package(Threads REQUIRED)

add_executable(test_concurrent ${SOURCES})

set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 

find_package(Boost 1.62.0 COMPONENTS fiber context) 

MESSAGE("Searching Boost in: " $ENV{BOOST_ROOT})
if (Boost_FOUND)
    MESSAGE("header: " ${Boost_INCLUDE_DIRS})
    MESSAGE("libs: " ${Boost_LIBRARY_DIRS})
    
    include_directories(${Boost_INCLUDE_DIRS}) 
    link_directories(${Boost_LIBRARY_DIRS})
    target_link_libraries(test_concurrent Threads::Threads ${Boost_LIBRARIES})
   
    add_definitions(-DU_WITH_FIBER)
else()
    target_link_libraries(test_concurrent Threads::Threads)
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/output" CACHE PATH "default install path" FORCE )
endif()

if (CMAKE_BUILD_TYPE_INITIALIZED_TO_DEFAULT)
   set(CMAKE_BUILD_TYPE Release)
endif()

