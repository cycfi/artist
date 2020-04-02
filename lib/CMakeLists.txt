###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.7.2...3.15.0)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

if (ARTIST_SKIA)

   ############################################################################
   # Prebuilt binaries
   ############################################################################
   if (APPLE)
      set(SKIA_LIB_URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/macos-libskia.a.zip)
      set(SKIA_LIB_MD5 f6726f24184673db83bfa66d05c6a027)
      set(SKIA_LIB_NAME libskia.a)

      set(UNIBREAK_LIB_URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/macos-libunibreak.a.zip)
      set(UNIBREAK_LIB_MD5 e1ec1a1001427efda2a036d058609a72)
      set(UNIBREAK_LIB_NAME libunibreak.a)

      set(HARFBUZZ_LIB_URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/macos-libharfbuzz.a.zip)
      set(HARFBUZZ_LIB_MD5 d267800c2acd66827e4cc2ead2121c28)
      set(HARFBUZZ_LIB_NAME libharfbuzz.a)
   elseif (MSVC)
      set(SKIA_LIB_URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/windows-skia.zip)
      set(SKIA_LIB_MD5 3b667a31b4a9ba1f6a6f732468cc942e)
      set(SKIA_LIB_NAME skia.lib)

      set(UNIBREAK_LIB_URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/unibreak.zip)
      set(UNIBREAK_LIB_MD5 157edb5085ae90d485795c21841c4455)
      set(UNIBREAK_LIB_NAME unibreak.lib)

      set(HARFBUZZ_LIB_URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/windows-harfbuzz.zip)
      set(HARFBUZZ_LIB_MD5 48f3f27aca8d1d4a81bda8c3ae6f01ee)
      set(HARFBUZZ_LIB_NAME harfbuzz.lib)
   endif()

   ############################################################################
   # Skia
   ############################################################################
   ExternalProject_Add(skia
      PREFIX "skia_prebuilt"
      URL ${SKIA_LIB_URL}
      URL_MD5 ${SKIA_LIB_MD5}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
   )

   ############################################################################
   # harfbuzz
   ############################################################################
   ExternalProject_Add(harfbuzz
      PREFIX "harfbuzz_prebuilt"
      URL ${HARFBUZZ_LIB_URL}
      URL_MD5 ${HARFBUZZ_LIB_MD5}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
   )

   ############################################################################
   # libunibreak
   ############################################################################
   ExternalProject_Add(libunibreak
      PREFIX "libunibreak_prebuilt"
      URL ${UNIBREAK_LIB_URL}
      URL_MD5 ${UNIBREAK_LIB_MD5}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
   )

   find_package(opengl REQUIRED)
endif()

###############################################################################
# Sources (and Resources)

set(ARTIST_SOURCES
   src/artist/rect.cpp
   src/artist/resources.cpp
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
   include/artist/resources.hpp
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
   add_dependencies(artist skia harfbuzz libunibreak)
endif()

target_sources(artist
   PRIVATE ${ARTIST_SOURCES} ${ARTIST_IMPL}
   PUBLIC ${ARTIST_HEADERS}
)

target_include_directories(
   artist
   PUBLIC
      include
      infra/include
      external/filesystem/include/
)

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
         external/skia/include/ports
         external/skia/src/gpu
         external/skia/modules/skshaper/include
         external/libunibreak/include/
         external/filesystem/include/
         external/harfbuzz/include/
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
   set_property(TARGET artist PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded"
   )

endif()

###############################################################################
# Libraries and linking
if (ARTIST_SKIA)
   target_link_libraries(artist
   PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}/skia_prebuilt/src/skia/${SKIA_LIB_NAME}"
      "${CMAKE_CURRENT_BINARY_DIR}/libunibreak_prebuilt/src/libunibreak/${UNIBREAK_LIB_NAME}"
      "${CMAKE_CURRENT_BINARY_DIR}/harfbuzz_prebuilt/src/harfbuzz/${HARFBUZZ_LIB_NAME}"
      ${OPENGL_LIBRARIES}
   )
endif()

if (UNIX OR (WIN32 AND NOT MSVC))
endif()

