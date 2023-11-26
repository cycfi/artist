###############################################################################
#  Copyright (c) 2016-2023 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.7.2...3.15.0)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

project(artist LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

# Ensure presence of Git submodules (when not using a source tarball)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
   find_package(Git REQUIRED)
   function(git_submodule_check dir)
      if(NOT EXISTS "${dir}/CMakeLists.txt")
         execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- ${dir}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY)
      endif()
   endfunction()
   git_submodule_check(lib/external/libunibreak)
   git_submodule_check(lib/infra)
endif()

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

if (ARTIST_SKIA AND WIN32)
   ExternalProject_Add(windows_dlls
      PREFIX "windows-x64-dll"
      URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/windows-x64-dll.zip
      URL_MD5 4c07a5aa4bfddff268535244196f900f
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
   )
   set(FONTCONFIG_DLL ${CMAKE_CURRENT_BINARY_DIR}/windows-x64-dll/src/windows_dlls/fontconfig.dll)
   set(ICONV_DLL ${CMAKE_CURRENT_BINARY_DIR}/windows-x64-dll/src/windows_dlls/libiconv.dll)
   set(XML2_DLL ${CMAKE_CURRENT_BINARY_DIR}/windows-x64-dll/src/windows_dlls/libxml2.dll)
   set(FREETYPE_DLL ${CMAKE_CURRENT_BINARY_DIR}/windows-x64-dll/src/windows_dlls/freetype.dll)
endif()

if (ARTIST_SKIA AND WIN32)
   message(STATUS "Building Artist lib for Win32 with Skia.")
elseif (ARTIST_SKIA AND APPLE)
   message(STATUS "Building Artist lib for MacOS with Skia.")
elseif (ARTIST_QUARTZ_2D AND APPLE)
   message(STATUS "Building Artist lib for MacOS with Quartz2D.")
endif()

if (APPLE)
   if (NOT CMAKE_OSX_ARCHITECTURES)
      set(CMAKE_OSX_ARCHITECTURES "x86_64")
   endif()
   if (${CMAKE_OSX_ARCHITECTURES} STREQUAL "arm64")
      message(STATUS "Artist lib MacOS target processor: arm64.")
   elseif (${CMAKE_OSX_ARCHITECTURES} STREQUAL "x86_64")
   message(STATUS "Artist lib MacOS target processor: x86_64.")
   else()
      message(FATAL_ERROR "Unsupported MacOS compiler")
   endif()
endif()

add_subdirectory(lib)

option(ARTIST_BUILD_EXAMPLES "build Artist library examples" ON)
option(ARTIST_BUILD_TESTS "build Artist library tests" ON)

if (ARTIST_BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()

if (ARTIST_BUILD_TESTS)
   enable_testing()
   add_subdirectory(test)
endif()
