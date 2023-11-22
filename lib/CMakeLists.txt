###############################################################################
#  Copyright (c) 2016-2023 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.7.2...3.15.0)

add_library(libunibreak
   external/libunibreak/src/emojidef.c
   external/libunibreak/src/graphemebreak.c
   external/libunibreak/src/linebreak.c
   external/libunibreak/src/linebreakdata.c
   external/libunibreak/src/linebreakdef.c
   external/libunibreak/src/unibreakbase.c
   external/libunibreak/src/unibreakdef.c
   external/libunibreak/src/wordbreak.c
)

target_include_directories(
   libunibreak
   PUBLIC
      external/libunibreak/src
)

if (WIN32)
   target_compile_definitions(libunibreak
      PUBLIC
         WIN32_LEAN_AND_MEAN
         NOMINMAX
         _UNICODE
   )
   set_property(TARGET libunibreak PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded"
   )
endif()

if (ARTIST_SKIA)

   ############################################################################
   # Prebuilt binaries
   ############################################################################
   if (APPLE)

      find_package(PkgConfig REQUIRED)
      pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)

      set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/macos/src/prebuilt_binaries)
      set(SKIA_LIBRARY ${ARTIST_BINARIES}/libskia.a)
      set(HARFBUZZ_LIBRARY ${ARTIST_BINARIES}/libharfbuzz.a)

      ExternalProject_Add(prebuilt_binaries
         PREFIX "macos"
         URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/macos.zip
         URL_MD5 66ffbf1c8272642bec4db4161f538b01
         CONFIGURE_COMMAND ""
         BUILD_COMMAND ""
         INSTALL_COMMAND ""
         BUILD_BYPRODUCTS ${SKIA_LIBRARY} ${HARFBUZZ_LIBRARY}
      )

      find_package(opengl REQUIRED)

      set(ARTIST_DEPEPENDENCIES
         "${SKIA_LIBRARY}"
         "${HARFBUZZ_LIBRARY}"
         PkgConfig::fontconfig
         ${OPENGL_LIBRARIES}
      )

   elseif (MSVC)

      set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/windows-x64/src/prebuilt_binaries)
      set(SKIA_LIBRARY ${ARTIST_BINARIES}/skia.lib)
      set(HARFBUZZ_LIBRARY ${ARTIST_BINARIES}/harfbuzz.lib)
      set(FONTCONFIG_LIBRARY ${ARTIST_BINARIES}/fontconfig.lib)

      ExternalProject_Add(prebuilt_binaries
         PREFIX "windows-x64"
         URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/windows-x64.zip
         URL_MD5 69c3e3d54a065491f9a1915ece4c2a72
         CONFIGURE_COMMAND ""
         BUILD_COMMAND ""
         INSTALL_COMMAND ""
         BUILD_BYPRODUCTS ${SKIA_LIBRARY} ${HARFBUZZ_LIBRARY} ${FONTCONFIG_LIBRARY}
      )

      find_package(opengl REQUIRED)

      set(ARTIST_DEPEPENDENCIES
         "${SKIA_LIBRARY}"
         "${HARFBUZZ_LIBRARY}"
         "${FONTCONFIG_LIBRARY}"
         ${OPENGL_LIBRARIES}
      )

   elseif (UNIX)

      set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/linux-x64/src/prebuilt_binaries)
      set(SKIA_LIBRARY ${ARTIST_BINARIES}/libskia.a)

      find_package(Threads)
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
      pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)
      pkg_check_modules(freetype2 REQUIRED IMPORTED_TARGET freetype2)

      ExternalProject_Add(prebuilt_binaries
         PREFIX "linux-x64"
         URL https://github.com/cycfi/artist/releases/download/prebuilt-binaries/linux-x64.zip
         URL_MD5 2032e39233c9b627550fc0b5558fe335
         CONFIGURE_COMMAND ""
         BUILD_COMMAND ""
         INSTALL_COMMAND ""
         BUILD_BYPRODUCTS ${SKIA_LIBRARY}
      )

      set(ARTIST_DEPEPENDENCIES
         "${SKIA_LIBRARY}"
         ${CMAKE_DL_LIBS}
         PkgConfig::harfbuzz
         PkgConfig::fontconfig
         PkgConfig::freetype2
         Threads::Threads
      )

   endif()

endif()

###############################################################################
# Sources (and Resources)

set(ARTIST_SOURCES
   src/artist/rect.cpp
   src/artist/resources.cpp
   src/artist/svg_path.cpp
)

set(ARTIST_HEADERS
   include/artist/affine_transform.hpp
   include/artist/canvas.hpp
   include/artist/circle.hpp
   include/artist/color.hpp
   include/artist/detail
   include/artist/font.hpp
   include/artist/image.hpp
   include/artist/path.hpp
   include/artist/point.hpp
   include/artist/rect.hpp
   include/artist/resources.hpp
   include/artist/text_layout.hpp
)

if (APPLE AND ARTIST_QUARTZ_2D)
   set(ARTIST_IMPL
      impl/macos/quartz2d/canvas.mm
      impl/macos/quartz2d/image.mm
      impl/macos/quartz2d/font.mm
      impl/macos/quartz2d/text_layout.mm
      impl/macos/quartz2d/path.mm
   )
endif()

if (ARTIST_SKIA)

   if (APPLE)
      set(ARTIST_SKIA_PLATFORM_WINDOW_CONTEXT
         external/skia/src/GLWindowContext_mac.mm
      )
   elseif (MSVC)
      set(ARTIST_SKIA_PLATFORM_WINDOW_CONTEXT
         external/skia/src/GLWindowContext_win.cpp
      )
   endif()

   set(ARTIST_IMPL
      impl/skia/canvas.cpp
      impl/skia/image.cpp
      impl/skia/font.cpp
      impl/skia/text_layout.cpp
      impl/skia/path.cpp
      impl/skia/detail/harfbuzz.cpp
      external/skia/src/GLWindowContext.cpp
      ${ARTIST_SKIA_PLATFORM_WINDOW_CONTEXT}
   )

elseif (ARTIST_CAIRO)

   set(ARTIST_IMPL
      impl/cairo/canvas.cpp
      impl/cairo/image.cpp
      impl/cairo/font.cpp
      impl/cairo/text_layout.cpp
      impl/cairo/path.cpp
   )

endif()

source_group("Source Files\\artist"
   FILES
   ${ARTIST_SOURCES}
)

source_group("Source Files\\impl"
   FILES
   ${ARTIST_IMPL}
)

source_group("Header Files\\artist"
   FILES
   ${ARTIST_HEADERS}
)

add_library(artist)

if (NOT MSVC)
   find_package(PkgConfig REQUIRED)
endif()

# Additional dependencies
if (ARTIST_SKIA AND UNIX AND NOT APPLE)
   pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)
   target_link_libraries(artist PUBLIC PkgConfig::fontconfig)
endif()

add_dependencies(artist libunibreak)

if (ARTIST_SKIA)
   add_dependencies(artist prebuilt_binaries)
elseif (ARTIST_CAIRO AND NOT MSVC)
   pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
   target_link_libraries(artist PUBLIC PkgConfig::cairo)
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
      infra/external/filesystem/include/
      external/libunibreak/include/
)

if (ARTIST_SKIA)
   target_include_directories(
      artist
      PUBLIC
         external/skia
         external/skia/include
         external/skia/include/core
         external/skia/include/config
         external/skia/include/utils
         external/skia/include/gpu
         external/skia/include/codec
         external/skia/include/effects
         external/skia/include/ports
         external/skia/src/gpu
         external/skia/modules/skshaper/include
         external/filesystem/include/
         external/harfbuzz/include/
         external/fontconfig/include/
   )
   target_compile_definitions(
      artist
      PUBLIC
         ARTIST_SKIA
         SK_RELEASE
   )
endif()

if (ARTIST_CAIRO)
   target_compile_definitions(
      artist
      PUBLIC
         ARTIST_CAIRO
   )
elseif (ARTIST_QUARTZ_2D)
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
   $<$<CXX_COMPILER_ID:AppleClang>:-Wall -Wpedantic -ftemplate-backtrace-limit=0>
   $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4244 /wd4305 /wd4996 /wd4267 /wd4018 /wd5030 /wd4201 /utf-8>
)

###############################################################################
# Global options

if (APPLE)
   target_compile_definitions(artist PRIVATE
      ARTIST_CLASS_PREFIX=${ARTIST_CLASS_PREFIX}
   )
   target_compile_options(artist PUBLIC "-fobjc-arc")
elseif (WIN32)
   target_compile_definitions(artist
      PUBLIC
         WIN32_LEAN_AND_MEAN
         NOMINMAX
         _UNICODE
   )
   set_property(TARGET artist PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded"
   )
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(DEBUG_FLAG true)
else()
   set(DEBUG_FLAG false)
endif()

target_compile_definitions(artist
   PUBLIC
      ARTIST_DEBUG=${DEBUG_FLAG}
)

###############################################################################
# Libraries and linking

target_link_libraries(artist
   PRIVATE
   libunibreak
)

if (ARTIST_SKIA)
   target_link_libraries(artist
   PRIVATE
      "${SKIA_LIBRARY}"
      "${ARTIST_DEPEPENDENCIES}"
   )
endif()

if (UNIX OR (WIN32 AND NOT MSVC))
endif()

