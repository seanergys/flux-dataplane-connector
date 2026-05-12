cmake_minimum_required(VERSION 3.18)
project(libdataplaneconnect LANGUAGES CXX C)
message(STATUS "Building libdataplaneconnect")

if(POLICY CMP0140)
  cmake_policy(SET CMP0140 NEW)
endif()

set(CMAKE_CXX_STANDARD 23 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# RPATH setup
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
# the RPATH to be used when installing, but only if it's not a system directory
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib64:${CMAKE_INSTALL_PREFIX}/lib")
endif("${isSystemDir}" STREQUAL "-1")

# Setting the include directory for the application to find config.h
include_directories( ${CMAKE_BINARY_DIR} )

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})

# external dependencies
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson>=2.10)
pkg_check_modules(seanerbus REQUIRED IMPORTED_TARGET seanerbus)
find_package(FluxCore REQUIRED)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  message(STATUS "flux core found and no specific prefix specified, using ${FLUX_CORE_PREFIX}")
  set(CMAKE_INSTALL_PREFIX ${FLUX_CORE_PREFIX} CACHE PATH "..." FORCE)
endif()

if("${FLUX_CORE_LIBDIR}" MATCHES "^${FLUX_CORE_PREFIX}/(.+)$")
  set(FLUX_CORE_LIBEXT "${CMAKE_MATCH_1}")
else()
  set(FLUX_CORE_LIBEXT "${CMAKE_INSTALL_LIBDIR}")
endif()

set(CMAKE_INSTALL_LIBDIR ${FLUX_CORE_LIBEXT} CACHE PATH "..." FORCE)
message(STATUS "Forcing libdir selection to match flux-core: prefix subdir (${FLUX_CORE_LIBEXT})")
message(STATUS "    flux-core library path: (${FLUX_CORE_LIBDIR})")
message(STATUS "    libdataplaneconnect library path: (${CMAKE_INSTALL_PREFIX}/${FLUX_CORE_LIBEXT})")

#pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)


add_library(libdataplaneconnect "src/DataplaneConnection.cpp")

target_link_libraries(libdataplaneconnect PUBLIC flux::core PkgConfig::JANSSON)
target_link_libraries(libdataplaneconnect PUBLIC PkgConfig::seanerbus)
target_include_directories(libdataplaneconnect PRIVATE "include")
