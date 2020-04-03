###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.9.6...3.15.0)

project(artist LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

set(DEFAULT_BUILD_TYPE "Release")
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE
        STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if (IPO_SUPPORTED)
    message(STATUS "Link-time optimization supported. Will be enabled in Release build type")
endif()

if (APPLE)
  option(ARTIST_QUARTZ_2D "build Artist using quartz 2d on MacOS" ON)
  option(ARTIST_SKIA "build Artist using skia" OFF)
else()
  option(ARTIST_SKIA "build Artist using skia" ON)
endif()

if (ARTIST_SKIA)
  set(ARTIST_QUARTZ_2D OFF)
endif()

add_subdirectory(lib)
add_subdirectory(examples)
add_subdirectory(test)
