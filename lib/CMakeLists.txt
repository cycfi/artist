###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.7.2...3.15.0)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

if (ARTIST_SKIA)

   ExternalProject_Add(skia
      PREFIX "skia_prebuilt"
      URL https://github.com/cycfi/artist/releases/download/skia-prebuilt/libskia.a.zip
      URL_MD5 f6726f24184673db83bfa66d05c6a027
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
   )

   add_custom_target(
      libunibreak
      COMMAND make release "${CMAKE_CURRENT_LIST_DIR}/external/libunibreak/src"
      WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/external/libunibreak/src"
   )

   find_package(opengl REQUIRED)
   find_package(harfbuzz CONFIG REQUIRED)
endif()

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

if (ARTIST_SKIA)
   set(ARTIST_IMPL
      impl/skia/canvas.cpp
      impl/skia/picture.cpp
      impl/skia/font.cpp
      impl/skia/text_layout.cpp
      impl/skia/detail/harfbuzz.cpp
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

if (ARTIST_SKIA)
   add_dependencies(artist skia libunibreak)
endif()

target_sources(artist
   PRIVATE ${ARTIST_SOURCES} ${ARTIST_IMPL}
   PUBLIC ${ARTIST_HEADERS}
)

target_include_directories(artist PUBLIC include)

if (ARTIST_SKIA)
   target_include_directories(
      artist
      PUBLIC
         external/skia
         external/skia/include/core
         external/skia/include/config
         external/skia/include/utils
         external/skia/include/gpu
         external/skia/include/codec
         external/skia/include/effects
         external/skia/src/gpu
         external/skia/modules/skshaper/include
         external/libunibreak/src/
   )
   target_compile_definitions(
      artist
      PUBLIC
      ARTIST_SKIA
   )
endif()

if (ARTIST_QUARTZ_2D)
   target_compile_definitions(
      artist
      PUBLIC
         ARTIST_QUARTZ_2D
   )
endif()

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

if (ARTIST_SKIA)
   target_link_libraries(artist
   PUBLIC
      infra
   PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/skia_prebuilt/src/skia/libskia.a
      # ${CMAKE_CURRENT_BINARY_DIR}/libunibreak/src/skia/libskia.a
      ${OPENGL_LIBRARIES}
      harfbuzz::harfbuzz
      "${CMAKE_CURRENT_LIST_DIR}/external/libunibreak/src/ReleaseDir/libunibreak.a"
   )
elseif (ARTIST_QUARTZ_2D)
   target_link_libraries(artist PUBLIC
      infra
   )
endif()

if (UNIX OR (WIN32 AND NOT MSVC))
endif()

