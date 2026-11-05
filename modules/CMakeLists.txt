cmake_minimum_required(VERSION 3.18)
project(flux-modules LANGUAGES C)
message(STATUS "Building Seanergys Flux Modules")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson>=2.10)
find_package(FluxCore REQUIRED)

set(CMAKE_INSTALL_LIBDIR ${FLUX_CORE_LIBEXT} CACHE PATH "..." FORCE)
message(STATUS "Forcing libdir selection to match flux-core: prefix subdir (${FLUX_CORE_LIBEXT})")
message(STATUS "    flux-core library path: (${FLUX_CORE_LIBDIR})")
message(STATUS "    plugin library path: (${CMAKE_INSTALL_PREFIX}/${FLUX_CORE_LIBEXT})")

#pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)

set(CMAKE_SHARED_LIBRARY_PREFIX "")
add_subdirectory(../libdataplaneconnect libdataplaneconnect) # this should pull in flux core

add_library(seanergys-dataplane-log SHARED "dataplane-log.c")
target_link_libraries(seanergys-dataplane-log PUBLIC flux::core PkgConfig::JANSSON dataplaneconnect)

add_library(interim-node-drain SHARED "interim-node-drain.c")
target_link_libraries(interim-node-drain PUBLIC flux::core PkgConfig::JANSSON dataplaneconnect)