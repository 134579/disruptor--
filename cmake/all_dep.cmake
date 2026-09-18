# Third-party dependencies of disruptor--, fetched and built by CPM.cmake.
# The CPM bootstrap itself lives in cmake/get_cpm.cmake.
#
# Only what this project really uses is listed here:
#   - Boost.Test : the unit test framework of the test suite

# Boost.Test, built from the official CMake enabled release archive.
# A system wide Boost is deliberately not used: most distributions package the
# headers without the compiled unit_test_framework library, which used to make
# find_package(Boost COMPONENTS unit_test_framework REQUIRED) fail outright.
# BOOST_INCLUDE_LIBRARIES takes libs/ directory names, not target names, so the
# Boost.Test library is 'test' and it exports the Boost::unit_test_framework
# target. Keeping the list short keeps the build small, the transitive
# dependencies of the listed libraries are pulled in automatically.
#
# The library is static and its target publishes BOOST_TEST_STATIC_LINK, which
# selects the 'test_suite* (*)(int, char**)' flavour of init_unit_test_func. The
# test translation units therefore must not define BOOST_TEST_DYN_LINK: that
# macro would switch them to the 'bool (*)()' flavour and the two signatures do
# not link together.
CPMAddPackage(
  NAME Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "BOOST_SKIP_INSTALL_RULES ON"
    "CMAKE_POSITION_INDEPENDENT_CODE ON"
    "BOOST_ENABLE_CMAKE ON"
    "BOOST_INCLUDE_LIBRARIES test"
)
