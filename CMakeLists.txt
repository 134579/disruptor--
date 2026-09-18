# Copyright (c) 2011-2015, François Saint-Jacques
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the disruptor-- nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL FRANÇOIS SAINT-JACQUES BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# 3.20 is required by the CMake support of Boost, which CPM fetches below.
cmake_minimum_required(VERSION 3.20)

# The version lives in the .version file and is managed by tools/semver, which
# walks up the tree from its working directory until it finds .version. It thus
# has to run with the source directory as working directory: the source dir is
# simply not known yet, project() has not been called at this point.
execute_process(
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/semver"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE SEMVER_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE SEMVER_ERROR
  RESULT_VARIABLE SEMVER_RESULT)
if (NOT SEMVER_RESULT EQUAL 0)
  message(FATAL_ERROR "tools/semver failed (${SEMVER_RESULT}): ${SEMVER_ERROR}")
endif ()

project(disruptor VERSION ${SEMVER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

# third party dependencies are fetched and built by CPM.cmake
include(cmake/get_cpm.cmake)
include(cmake/all_dep.cmake)

# the disruptor headers use std::thread and std::condition_variable, which need
# an explicit libpthread on most distributions
find_package(Threads REQUIRED)

# header only library, consumers only need the include path and libpthread
add_library(disruptor INTERFACE)
target_include_directories(disruptor INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(disruptor INTERFACE Threads::Threads)

# options
option(COVERALLS "Generate coverage data" OFF)
option(COVERALLS_UPLOAD "Upload the generated coveralls json" OFF)

# code coverage
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/tools/coveralls-cmake/cmake")
if (COVERALLS)
  set(COVERAGE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/disruptor/sequence.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/disruptor/ring_buffer.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/disruptor/wait_strategy.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/disruptor/claim_strategy.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/disruptor/sequence_barrier.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/disruptor/sequencer.h)
  include(Coveralls)
  coveralls_turn_on_coverage()
  coveralls_setup(
    "${COVERAGE_SRCS}"
    ${COVERALLS_UPLOAD}
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/coveralls-cmake/cmake")
endif()

# tests
include(CTest)

if (BUILD_TESTING)
  set(DISRUPTOR_TESTS
      sequence_test
      ring_buffer_test
      wait_strategy_test
      sequence_barrier_test
      claim_strategy_test
      sequencer_test)

  foreach (test_name IN LISTS DISRUPTOR_TESTS)
    add_executable(${test_name}_bin test/${test_name}.cc)
    target_link_libraries(${test_name}_bin PRIVATE disruptor Boost::unit_test_framework)
    add_test(NAME ${test_name} COMMAND ${test_name}_bin)
  endforeach ()

  # Throughput benchmarks. They are plain programs taking an optional iteration
  # count on the command line, and they run for tens of seconds with their
  # default, so they are built but deliberately not registered with ctest.
  set(DISRUPTOR_BENCHMARKS
      one_publisher_to_one_unicast_throughput_test
      one_publisher_to_three_pipeline_throughput_test)

  foreach (benchmark_name IN LISTS DISRUPTOR_BENCHMARKS)
    add_executable(${benchmark_name}_bin test/benchmark/${benchmark_name}.cc)
    target_link_libraries(${benchmark_name}_bin PRIVATE disruptor)
  endforeach ()
endif ()
