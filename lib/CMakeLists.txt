###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.7.2...3.15.0)

###############################################################################
# Sources (and Resources)

set(CANVAS_SOURCES
   src/canvas/rect.cpp
)

set(CANVAS_HEADERS
   include/canvas/canvas.hpp
   include/canvas/circle.hpp
   include/canvas/color.hpp
   include/canvas/font.hpp
   include/canvas/picture.hpp
   include/canvas/point.hpp
   include/canvas/rect.hpp
   include/canvas/text_layout.hpp
)

if (APPLE)
   set(CANVAS_HOST
      host/macos/canvas.mm
      host/macos/picture.mm
      host/macos/font.mm
      host/macos/text_layout.mm
   )
elseif (UNIX)
   set(CANVAS_HOST
   )
elseif (WIN32)
   set(CANVAS_HOST
   )
endif()

source_group("Source Files\\canvas"
   FILES
   ${CANVAS_SOURCES}
)

source_group("Source Files\\canvas_host"
   FILES
   ${CANVAS_HOST}
)

source_group("Header Files\\canvas"
   FILES
   ${CANVAS_HEADERS}
)

add_library(canvas)

target_sources(canvas
   PRIVATE ${CANVAS_SOURCES} ${CANVAS_HOST}
   PUBLIC ${CANVAS_HEADERS}
)

target_include_directories(canvas PUBLIC include)

target_compile_features(canvas PUBLIC cxx_std_17)

if (IPO_SUPPORTED AND CMAKE_BUILD_TYPE STREQUAL "Release")
   message(STATUS "Enabling LTO for canvas")
   set_target_properties(canvas PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if (NOT MSVC)
   find_package(PkgConfig REQUIRED)
endif()

###############################################################################
# Get rid of certain warnings

target_compile_options(canvas PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -ftemplate-backtrace-limit=0>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -ftemplate-backtrace-limit=0>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4244 /wd4305 /wd4996 /wd4267 /wd4018>
)

###############################################################################
# Global options

if (APPLE)
   target_compile_definitions(canvas PRIVATE
      CANVAS_CLASS_PREFIX=${CANVAS_CLASS_PREFIX}
   )
   target_compile_options(canvas PUBLIC "-fobjc-arc")
elseif (WIN32)
   target_compile_definitions(canvas PUBLIC
      WIN32_LEAN_AND_MEAN
      NOMINMAX
      _UNICODE
   )
endif()

###############################################################################
# Libraries and linking

target_link_libraries(canvas PUBLIC
   infra
)

if (UNIX OR (WIN32 AND NOT MSVC))
endif()

add_library(cycfi::canvas ALIAS canvas)
