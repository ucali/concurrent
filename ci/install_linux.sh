#!/bin/bash

DEPS_DIR="${TRAVIS_BUILD_DIR}/deps"
mkdir -p ${DEPS_DIR} && cd ${DEPS_DIR}

function install_boost {
  BOOST_LIBRARIES="fiber,context"
  BOOST_VERSION="1.62.0"
  BOOST_URL="https://sourceforge.net/projects/boost/files/boost/${BOOST_VERSION}/boost_${BOOST_VERSION//\./_}.tar.gz"
  BOOST_DIR="${DEPS_DIR}/boost"
  echo "Downloading Boost ${BOOST_VERSION} from ${BOOST_URL}"
  mkdir -p ${BOOST_DIR} && cd ${BOOST_DIR}
  wget -O - ${BOOST_URL} | tar --strip-components=1 -xz -C ${BOOST_DIR} || exit 1
  #echo "using gcc : 5 : /usr/bin/g++-5 ; " >> tools/build/v2/user-config.jam
  ./bootstrap.sh --with-libraries=${BOOST_LIBRARIES} && ./b2 link=static threading=multi address-model=64 cxxflags=-std=c++11 toolset=$COMPILER 
  export BOOST_ROOT="$TRAVIS_BUILD_DIR/deps/boost"
  export CMAKE_MODULE_PATH="$BOOST_ROOT"
  export BOOST_INCLUDE="$BOOST_ROOT/include"
  export BOOST_LIBDIR="$BOOST_ROOT/lib"
  export BOOST_OPTS="-DBOOST_ROOT=${BOOST_ROOT} -DBOOST_INCLUDEDIR=${BOOST_INCLUDE} -DBOOST_LIBRARYDIR=${BOOST_LIBDIR}"
}

function install_cmake {
  CMAKE_VERSION="3.7.0"
  CMAKE_URL="https://cmake.org/files/v3.7/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz"
  CMAKE_DIR="${DEPS_DIR}/cmake"
  CMAKE_BIN="${CMAKE_DIR}/bin"
  echo "Downloading CMake ${CMAKE_VERSION} from ${CMAKE_URL}"
  mkdir -p ${CMAKE_DIR}
  wget --no-check-certificate -O - ${CMAKE_URL} | tar --strip-components=1 -xz -C ${CMAKE_DIR} || exit 1
  export PATH=${CMAKE_BIN}:${PATH}
}

install_boost # at least version 1.60
install_cmake
echo "Installed build dependecies."
echo "  - Boost: ${BOOST_ROOT}"

cd ${TRAVIS_BUILD_DIR}
