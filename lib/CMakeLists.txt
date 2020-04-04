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

      ExternalProject_Add(prebuilt_binaries
         PREFIX "macos"
         URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/macos.zip
         URL_MD5 3fc049d75f2524a47f6d64a9770b2b55
         CONFIGURE_COMMAND ""
         BUILD_COMMAND ""
         INSTALL_COMMAND ""
      )

      set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/macos/src/prebuilt_binaries)
      set(SKIA_LIBRARY ${ARTIST_BINARIES}/libskia.a)
      set(UNIBREAK_LIBRARY ${ARTIST_BINARIES}/libunibreak.a)
      set(HARFBUZZ_LIBRARY ${ARTIST_BINARIES}/libharfbuzz.a)

   elseif (MSVC)

      ExternalProject_Add(prebuilt_binaries
         PREFIX "windows-x64"
         URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/windows-x64.zip
         URL_MD5 5456bc759153dbc84f44ffb0d824bcbd
         CONFIGURE_COMMAND ""
         BUILD_COMMAND ""
         INSTALL_COMMAND ""
      )

      set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/windows-x64/src/prebuilt_binaries)
      set(SKIA_LIBRARY ${ARTIST_BINARIES}/skia.lib)
      set(UNIBREAK_LIBRARY ${ARTIST_BINARIES}/unibreak.lib)
      set(HARFBUZZ_LIBRARY ${ARTIST_BINARIES}/harfbuzz.lib)
      set(FONTCONFIG_LIBRARY ${ARTIST_BINARIES}/fontconfig.lib)

   endif()

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
   add_dependencies(artist prebuilt_binaries)
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
         external/fontconfig/include/
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
      "${SKIA_LIBRARY}"
      "${UNIBREAK_LIBRARY}"
      "${HARFBUZZ_LIBRARY}"
      "${FONTCONFIG_LIBRARY}"
      ${OPENGL_LIBRARIES}
   )
endif()

if (UNIX OR (WIN32 AND NOT MSVC))
endif()

