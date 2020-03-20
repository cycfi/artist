###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.7.2...3.15.0)

###############################################################################
# Sources (and Resources)

set(ARTIST_SOURCES
   src/artist/rect.cpp
)

set(ARTIST_HEADERS
   include/artist/canvas.hpp
   include/artist/circle.hpp
   include/artist/color.hpp
   include/artist/font.hpp
   include/artist/picture.hpp
   include/artist/point.hpp
   include/artist/rect.hpp
   include/artist/text_layout.hpp
)

if (APPLE)
   if (ARTIST_QUARTZ_2D)
      set(ARTIST_IMPL
         impl/macos/quartz2d/canvas.mm
         impl/macos/quartz2d/picture.mm
         impl/macos/quartz2d/font.mm
         impl/macos/quartz2d/text_layout.mm
      )
   endif()
elseif (UNIX)
   set(ARTIST_IMPL
   )
elseif (WIN32)
   set(ARTIST_IMPL
   )
endif()

source_group("Source Files\\artist"
   FILES
   ${ARTIST_SOURCES}
)

source_group("Source Files\\canvas_host"
   FILES
   ${ARTIST_IMPL}
)

source_group("Header Files\\artist"
   FILES
   ${ARTIST_HEADERS}
)

add_library(artist)

target_sources(artist
   PRIVATE ${ARTIST_SOURCES} ${ARTIST_IMPL}
   PUBLIC ${ARTIST_HEADERS}
)

target_include_directories(artist PUBLIC include)

target_compile_features(artist PUBLIC cxx_std_17)

if (IPO_SUPPORTED AND CMAKE_BUILD_TYPE STREQUAL "Release")
   message(STATUS "Enabling LTO for artist")
   set_target_properties(artist PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if (NOT MSVC)
   find_package(PkgConfig REQUIRED)
endif()

###############################################################################
# Get rid of certain warnings

target_compile_options(artist PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -ftemplate-backtrace-limit=0>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -ftemplate-backtrace-limit=0>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4244 /wd4305 /wd4996 /wd4267 /wd4018>
)

###############################################################################
# Global options

if (APPLE)
   target_compile_definitions(artist PRIVATE
      ARTIST_CLASS_PREFIX=${ARTIST_CLASS_PREFIX}
   )
   target_compile_options(artist PUBLIC "-fobjc-arc")
elseif (WIN32)
   target_compile_definitions(artist PUBLIC
      WIN32_LEAN_AND_MEAN
      NOMINMAX
      _UNICODE
   )
endif()

###############################################################################
# Libraries and linking

target_link_libraries(artist PUBLIC
   infra
)

if (UNIX OR (WIN32 AND NOT MSVC))
endif()

add_library(cycfi::artist ALIAS artist)
