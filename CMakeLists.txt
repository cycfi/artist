###############################################################################
# SPDX-FileCopyrightText:  2016-2024 Joel de Guzman
# SPDX-FileContributor:    Modified by Andrea Zanellato
# SPDX-License-Identifier: MIT
###############################################################################
cmake_minimum_required(VERSION 3.18.0)

project(artist LANGUAGES C CXX)

if (POLICY CMP0135)
   cmake_policy(SET CMP0135 NEW)
endif()

###############################################################################
# Settings
###############################################################################

# Add our CMake modules to path
list(APPEND CMAKE_MODULE_PATH
   "${CMAKE_SOURCE_DIR}/lib/infra/cmake"
   "${CMAKE_SOURCE_DIR}/cmake"
)
if(CMAKE_CXX_STANDARD LESS 17)
   set(CMAKE_CXX_STANDARD 17)
endif()

# TODO: Move compiler settings to a settings module
if (APPLE)
   if (NOT CMAKE_OSX_ARCHITECTURES)
      set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR})
   endif()
   if (${CMAKE_OSX_ARCHITECTURES} STREQUAL "arm64")
      message(STATUS "Artist lib macOS target processor: arm64.")
   elseif (${CMAKE_OSX_ARCHITECTURES} STREQUAL "x86_64")
      message(STATUS "Artist lib macOS target processor: x86_64.")
   else()
      message(FATAL_ERROR "Unsupported macOS compiler")
   endif()
endif()
message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID}")

set(ARTIST_HOST_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME})
if (ARTIST_HOST_SYSTEM_NAME STREQUAL "Darwin")
   set(ARTIST_HOST_SYSTEM_NAME "macOS")
   set(ARTIST_BACKEND_NAME "Quartz2D")
else()
   set(ARTIST_BACKEND_NAME "Skia")
endif()
message(STATUS "Building Artist library for ${ARTIST_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_VERSION}\
 using ${ARTIST_BACKEND_NAME} backend.")

###############################################################################
# Module utilities
###############################################################################

include(BuildType) # Default build type.
include(OptionEx)  # Conditional based options, adding a [default: ON/OFF] to the documentation.
include(CheckIPOSupported)

if (CYCFI_ENABLE_GIT_SUBMODULE_CHECK)
   if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
      include(GitUtilities)
      git_submodule_check(lib/external/libunibreak)
      git_submodule_check(lib/infra)
   endif()
endif()

if (CYCFI_ENABLE_LTO)
   check_ipo_supported(RESULT IPO_SUPPORTED)
   if (IPO_SUPPORTED)
      message(STATUS "Link-time optimization supported. Will be enabled in Release build type")
   endif()
endif()

###############################################################################
# Options
###############################################################################

option_ex(ARTIST_QUARTZ_2D      "Build Artist using Quartz 2D on macOS." APPLE)
option_ex(ARTIST_SKIA           "Build Artist using Skia."               NOT APPLE)
option_ex(ARTIST_BUILD_EXAMPLES "Build Artist library examples."         ON)
option_ex(ARTIST_BUILD_TESTS    "Build Artist library tests."            ON)

###############################################################################
# Sub projects
###############################################################################

add_subdirectory(lib)

if (ARTIST_BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()

if (ARTIST_BUILD_TESTS)
   enable_testing()
   add_subdirectory(test)
endif()
