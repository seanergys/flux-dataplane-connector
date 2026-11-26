# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: LGPL-3.0-only
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.18)
project(job-manager-plugins LANGUAGES CXX C)

if(POLICY CMP0140)
  cmake_policy(SET CMP0140 NEW)
endif()

set(CMAKE_CXX_STANDARD 23 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Setting the include directory for the application to find config.h
include_directories( ${CMAKE_BINARY_DIR} )

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})

# external dependencies
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson>=2.10)
find_package(FluxCore REQUIRED)


if("${FLUX_CORE_LIBDIR}" MATCHES "^${FLUX_CORE_PREFIX}/(.+)$")
  set(FLUX_CORE_LIBEXT "${CMAKE_MATCH_1}")
else()
  set(FLUX_CORE_LIBEXT "${CMAKE_INSTALL_LIBDIR}")
endif()

set(CMAKE_INSTALL_LIBDIR ${FLUX_CORE_LIBEXT} CACHE PATH "..." FORCE)
message(STATUS "Forcing libdir selection to match flux-core: prefix subdir (${FLUX_CORE_LIBEXT})")
message(STATUS "    flux-core library path: (${FLUX_CORE_LIBDIR})")
message(STATUS "    plugin library path: (${CMAKE_INSTALL_PREFIX}/${FLUX_CORE_LIBEXT})")


set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_subdirectory(../libdataplaneconnect libdataplaneconnect) # this should pull in flux core

add_library(publish-jobs-dataplane SHARED "publish-jobs-dataplane.c" "jj.c")
target_link_libraries(publish-jobs-dataplane PUBLIC flux::core PkgConfig::JANSSON dataplaneconnect)

add_library(publish-jobs-dataplane2 SHARED "publish-jobs-dataplane2.c" "jj.c")
target_link_libraries(publish-jobs-dataplane2 PUBLIC flux::core PkgConfig::JANSSON dataplaneconnect)


enable_testing()


set(ALL_TESTS t1000-setup.t t1001-lifecycle-events.t)
foreach(test ${ALL_TESTS})
  add_test(NAME ${test}
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test})
  # TIMEOUT: a stuck test must fail with its output instead of stalling the whole
  # ctest run (a test that leaves jobs running blocks broker shutdown for as long
  # as those jobs run).
  set_tests_properties(${test} PROPERTIES ENVIRONMENT
    "SHARNESS_BUILD_DIRECTORY=${CMAKE_BINARY_DIR};SHARNESS_TEST_SRCDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests;PATH=$ENV{PATH}"
    TIMEOUT 300)
endforeach()
