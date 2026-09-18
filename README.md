disruptor--
===========
[![Build Status](https://travis-ci.org/fsaintjacques/disruptor--.svg?branch=develop)](https://travis-ci.org/fsaintjacques/disruptor--) [![Coverage Status](https://coveralls.io/repos/fsaintjacques/disruptor--/badge.svg?branch=develop)](https://coveralls.io/r/fsaintjacques/disruptor--?branch=develop)

C++ implementation of LMAX's disruptor pattern.

Supported compilers:
  - clang-3.5
  - clang-3.6
  - gcc-4.8
  - gcc-4.9
  - gcc-5

Build instructions
------------------

This library is a header-only library and doesn't require compile step,
move/copy the `disruptor/` folder in one of the include folder.

If you want to develop and/or submit patches to `disruptor--` you need:
  - CMake >= 3.20
  - a C++11 compiler, one of: clang-3.5, clang-3.6, gcc-4.8, gcc-4.9, gcc-5

No Boost installation is required. The test suite depends on Boost.Test, which
is fetched and built by [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake)
during configure. CPM itself is bootstrapped by `cmake/get_cpm.cmake`, the
dependency set is declared in `cmake/all_dep.cmake`.

Once dependencies are met

```
# tools/deps && tools/test
```

Dependencies are downloaded into the build directory by default, so removing
`build/` downloads them again. Set `CPM_SOURCE_CACHE` to share one download
cache between build directories:

```
# export CPM_SOURCE_CACHE=${HOME}/.cache/CPM
```

Benchmarks
----------

`test/benchmark` holds two throughput benchmarks of the sequencer, they are
built along with the test suite but are not registered with ctest. Both take an
optional number of events as their first argument and default to 300 millions,
so build in Release before reading anything into the numbers:

```
# ./build/one_publisher_to_one_unicast_throughput_test_bin
# ./build/one_publisher_to_three_pipeline_throughput_test_bin 1000000
```

Each of them prints the checksum of the events it consumed, which has to be
`events * (events - 1) / 2` (three times that for the pipeline). A different
value means events were lost or processed twice.
