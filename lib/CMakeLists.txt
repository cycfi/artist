###############################################################################
#  Copyright (c) 2016-2023 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.15.0)

if (POLICY CMP0135)
   cmake_policy(SET CMP0135 NEW)
endif()

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
   # Follow the configuration being built. CMAKE_BUILD_TYPE is empty for
   # multi-config generators, and keying off the compiler picked the debug CRT
   # for clang-cl even in Release, which mismatched artist and elements on
   # _ITERATOR_DEBUG_LEVEL and failed the link.
   set_property(TARGET libunibreak PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
   )
endif()

if (ARTIST_SKIA)

   ############################################################################
   # Skia dependency
   ############################################################################
   # The prebuilt bundle ships a debug and a release Skia. An optimized build
   # must link the release one: the debug Skia is far slower and, because
   # SkRefCnt is header-inline, mixing it with an NDEBUG consumer is an ODR
   # mismatch that aborts at runtime. Configurations with no Skia build of
   # their own fall back to the debug library, so map them to Release.
   set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Release;")
   set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "Release;")

   # Default (local builds): fetch a prebuilt Skia bundle from R2 and add it to
   # CMAKE_PREFIX_PATH so the find_package(unofficial-skia) calls below resolve
   # it — no Skia compile. No-op under a vcpkg toolchain (vcpkg supplies Skia).
   include(${CMAKE_CURRENT_LIST_DIR}/../cmake/SkiaPrebuilt.cmake)

   if (APPLE)

      # Skia's vcpkg config resolves its image libraries (jpeg, png, webp) with
      # find_library, which searches the system prefixes, Homebrew included,
      # before Skia's own lib directory. A Homebrew libjpeg with another API
      # version then fails every JPEG decode at runtime. Keep the search inside
      # Skia's tree (SDK libraries still resolve through the implicit link
      # directories) and drop whatever an earlier configure cached.
      get_cmake_property(_cache_vars CACHE_VARIABLES)
      foreach (_var IN LISTS _cache_vars)
         if (_var MATCHES "^z_vcpkg_skia_link_libs_")
            unset(${_var} CACHE)
         endif()
      endforeach()
      set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
      set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)
      find_package(unofficial-skia CONFIG REQUIRED)
      unset(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH)
      unset(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH)

      # vcpkg installs Skia headers in subdirectories (core/, effects/, gpu/, …).
      # Derive the base from the installed target and expose subdirs so existing
      # flat #include <SkCanvas.h> style continues to work.
      get_target_property(_skia_inc unofficial::skia::skia INTERFACE_INCLUDE_DIRECTORIES)
      set(SKIA_INCLUDE_BASE "${_skia_inc}")
      set(ARTIST_SKIA_EXTRA_INCLUDES
         ${SKIA_INCLUDE_BASE}/core
         ${SKIA_INCLUDE_BASE}/config
         ${SKIA_INCLUDE_BASE}/codec
         ${SKIA_INCLUDE_BASE}/effects
         ${SKIA_INCLUDE_BASE}/gpu
         ${SKIA_INCLUDE_BASE}/ports
         ${SKIA_INCLUDE_BASE}/utils
         ${SKIA_INCLUDE_BASE}/../harfbuzz
      )

      set(_vcpkg_lib "${SKIA_INCLUDE_BASE}/../../lib")
      find_library(FREETYPE_LIB  freetype     PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BROTLIDEC_LIB brotlidec    PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BROTLICOM_LIB brotlicommon PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BZ2_LIB       bz2          PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)

      # Skia's own harfbuzz. No GTK runs on macOS to load a second copy, and a
      # Homebrew harfbuzz would tie apps to Homebrew, which sandboxed plugin
      # hosts cannot load.
      find_library(HARFBUZZ_LIB  harfbuzz     PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)

      set(ARTIST_DEPEPENDENCIES
         unofficial::skia::skia
         ${HARFBUZZ_LIB}
         ${FREETYPE_LIB}
         ${BROTLIDEC_LIB}
         ${BROTLICOM_LIB}
         ${BZ2_LIB}
      )

   elseif (MSVC)

      find_package(unofficial-skia CONFIG REQUIRED)

      get_target_property(_skia_inc unofficial::skia::skia INTERFACE_INCLUDE_DIRECTORIES)
      set(SKIA_INCLUDE_BASE "${_skia_inc}")
      set(ARTIST_SKIA_EXTRA_INCLUDES
         ${SKIA_INCLUDE_BASE}/core
         ${SKIA_INCLUDE_BASE}/config
         ${SKIA_INCLUDE_BASE}/codec
         ${SKIA_INCLUDE_BASE}/effects
         ${SKIA_INCLUDE_BASE}/gpu
         ${SKIA_INCLUDE_BASE}/ports
         ${SKIA_INCLUDE_BASE}/utils
         ${SKIA_INCLUDE_BASE}/../harfbuzz
      )

      set(_vcpkg_lib "${SKIA_INCLUDE_BASE}/../../lib")
      find_library(HARFBUZZ_LIB  harfbuzz     PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(FREETYPE_LIB  freetype     PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BROTLIDEC_LIB brotlidec    PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BROTLICOM_LIB brotlicommon PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BZ2_LIB       bz2          PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)

      find_package(OpenGL REQUIRED)

      set(ARTIST_DEPEPENDENCIES
         unofficial::skia::skia
         ${HARFBUZZ_LIB}
         ${FREETYPE_LIB}
         ${BROTLIDEC_LIB}
         ${BROTLICOM_LIB}
         ${BZ2_LIB}
         ${OPENGL_LIBRARIES}
      )

      set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   /MTd")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /GL /MT")

   elseif (UNIX)

      find_package(unofficial-skia CONFIG REQUIRED)

      get_target_property(_skia_inc unofficial::skia::skia INTERFACE_INCLUDE_DIRECTORIES)
      set(SKIA_INCLUDE_BASE "${_skia_inc}")
      set(ARTIST_SKIA_EXTRA_INCLUDES
         ${SKIA_INCLUDE_BASE}/core
         ${SKIA_INCLUDE_BASE}/config
         ${SKIA_INCLUDE_BASE}/codec
         ${SKIA_INCLUDE_BASE}/effects
         ${SKIA_INCLUDE_BASE}/gpu
         ${SKIA_INCLUDE_BASE}/ports
         ${SKIA_INCLUDE_BASE}/utils
         ${SKIA_INCLUDE_BASE}/../harfbuzz
      )

      set(_vcpkg_lib "${SKIA_INCLUDE_BASE}/../../lib")
      # Use the system harfbuzz (shared) instead of vcpkg's static harfbuzz.
      # When GTK is loaded (window host, e.g. Elements), it brings in the system
      # harfbuzz dynamically. Having both a static vcpkg harfbuzz AND the system
      # shared harfbuzz in the same process causes symbol interposition — GTK/
      # Pango calls get routed to the static 14.x copy and crash on widget
      # realize. Using the system harfbuzz exclusively avoids the conflict.
      # (Headless Artist tests never load GTK so they don't hit it, but Elements
      # does.)
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
      find_library(FREETYPE_LIB    freetype     PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(FONTCONFIG_LIB  fontconfig   PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BROTLIDEC_LIB   brotlidec    PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BROTLICOM_LIB   brotlicommon PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(BZ2_LIB         bz2          PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
      find_library(EXPAT_LIB       expat        PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)

      find_package(Threads REQUIRED)
      find_package(OpenGL REQUIRED)

      # These static archives have cyclic symbol dependencies: fontconfig needs
      # freetype and expat, freetype needs brotli and bz2. A linker group makes
      # the linker rescan them so resolution does not depend on list order.
      set(ARTIST_DEPEPENDENCIES
         unofficial::skia::skia
         PkgConfig::HARFBUZZ
         -Wl,--start-group
         ${FONTCONFIG_LIB}
         ${FREETYPE_LIB}
         ${EXPAT_LIB}
         ${BROTLIDEC_LIB}
         ${BROTLICOM_LIB}
         ${BZ2_LIB}
         -Wl,--end-group
         ${CMAKE_DL_LIBS}
         Threads::Threads
         OpenGL::OpenGL
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
   include/artist/canvas_layer.hpp
   include/artist/circle.hpp
   include/artist/color.hpp
   include/artist/detail
   include/artist/font.hpp
   include/artist/image.hpp
   include/artist/path.hpp
   include/artist/point.hpp
   include/artist/rect.hpp
   include/artist/resources.hpp
   include/artist/text_run.hpp
)

if (APPLE AND ARTIST_QUARTZ_2D)
   set(ARTIST_IMPL
      impl/macos/quartz2d/canvas.mm
      impl/macos/quartz2d/canvas_layer.mm
      impl/macos/quartz2d/image.mm
      impl/macos/quartz2d/font.mm
      impl/macos/quartz2d/text_run.mm
      impl/macos/quartz2d/path.mm
   )
endif()

if (ARTIST_SKIA)

   set(ARTIST_IMPL
      impl/skia/canvas.cpp
      impl/skia/canvas_layer.cpp
      impl/skia/image.cpp
      impl/skia/font.cpp
      impl/skia/text_run.cpp
      impl/skia/path.cpp
      impl/skia/detail/harfbuzz.cpp
   )

endif()

if (ARTIST_CAIRO)
   set(ARTIST_IMPL
      impl/cairo/canvas.cpp
      impl/cairo/canvas_layer.cpp
      impl/cairo/shadow_blur.cpp
      impl/cairo/path.cpp
      impl/cairo/image.cpp
      impl/cairo/font.cpp
      impl/cairo/text_run.cpp
   )
endif()

if (WIN32 AND ARTIST_DIRECT2D)
   set(ARTIST_IMPL
      impl/windows/direct2d/canvas.cpp
      impl/windows/direct2d/canvas_layer.cpp
      impl/windows/direct2d/context.cpp
      impl/windows/direct2d/path.cpp
      impl/windows/direct2d/path_impl.cpp
      impl/windows/direct2d/image.cpp
      impl/windows/direct2d/font.cpp
      impl/windows/direct2d/text_run.cpp
   )
endif()

if (ARTIST_RECORDING)
   set(ARTIST_IMPL
      impl/recording/canvas.cpp
      impl/recording/canvas_layer.cpp
      impl/recording/path.cpp
      impl/recording/image.cpp
      impl/recording/font.cpp
      impl/recording/text_run.cpp
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

add_library(artist STATIC)

# The Cairo and recording backends need cairo, harfbuzz, fontconfig,
# freetype and webp. On macOS and Windows a prebuilt bundle supplies them,
# along with the pkg-config they are found with, so nothing has to be
# installed; Linux uses its own. Quartz2D and Skia need none of this: a
# macOS build with the default backend requires no pkg-config at all.
if (ARTIST_CAIRO OR ARTIST_RECORDING)
   include(${CMAKE_CURRENT_LIST_DIR}/../cmake/CairoPrebuilt.cmake)
endif()

if (ARTIST_CAIRO AND NOT MSVC)
   find_package(PkgConfig REQUIRED)
endif()


if (ARTIST_CAIRO AND NOT MSVC)
   pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
   pkg_check_modules(cairo_ft REQUIRED IMPORTED_TARGET cairo-ft)
   pkg_check_modules(fc REQUIRED IMPORTED_TARGET fontconfig)
   pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
   pkg_check_modules(webp REQUIRED IMPORTED_TARGET libwebp)
   target_link_libraries(artist PUBLIC
      PkgConfig::cairo PkgConfig::cairo_ft PkgConfig::fc PkgConfig::harfbuzz
      PkgConfig::webp)
   if (APPLE)
      target_link_libraries(artist PUBLIC
         "-framework CoreGraphics" "-framework CoreFoundation")
   endif()
endif()

if (ARTIST_CAIRO AND MSVC)
   # cairo, harfbuzz and fontconfig install pkg-config files only (no cmake
   # config), so they are found the same way on every platform. pkgconf
   # supplies the pkg-config executable on Windows: from the prebuilt bundle,
   # or from vcpkg.json under a vcpkg toolchain.
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
   pkg_check_modules(cairo_ft REQUIRED IMPORTED_TARGET cairo-ft)
   pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
   pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)
   pkg_check_modules(webp REQUIRED IMPORTED_TARGET libwebp)
   target_link_libraries(artist PUBLIC
      PkgConfig::cairo PkgConfig::cairo_ft PkgConfig::harfbuzz PkgConfig::fontconfig
      PkgConfig::webp)
endif()

if (ARTIST_RECORDING)
   # Nothing is rasterized, but text is measured and shaped with real fonts,
   # so layouts built on this backend match the drawing backends.
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(freetype REQUIRED IMPORTED_TARGET freetype2)
   pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)
   pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
   target_link_libraries(artist PUBLIC
      PkgConfig::freetype PkgConfig::fontconfig PkgConfig::harfbuzz)
   target_compile_definitions(artist PUBLIC ARTIST_RECORDING)
endif()

add_dependencies(artist libunibreak)


target_sources(artist
   PRIVATE ${ARTIST_SOURCES} ${ARTIST_IMPL}
   PUBLIC ${ARTIST_HEADERS}
)

target_include_directories(
   artist
   PUBLIC
      include
      infra/include
      external/libunibreak/include/
)

if (ARTIST_SKIA)
   # vcpkg Skia puts headers in subdirectories; expose them so flat
   # #include <SkCanvas.h> style continues to resolve.
   # ARTIST_SKIA_EXTRA_INCLUDES is set in the APPLE/MSVC/UNIX blocks above.
   target_include_directories(
      artist
      PUBLIC
         ${ARTIST_SKIA_EXTRA_INCLUDES}
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

if (ARTIST_CAIRO)
   target_compile_definitions(
      artist
      PUBLIC
         ARTIST_CAIRO
   )
endif()

if (WIN32 AND ARTIST_DIRECT2D)
   target_compile_definitions(
      artist
      PUBLIC
         ARTIST_DIRECT2D
   )
   # Expose the backend's low-level header so the example host can create the
   # HWND render target + wrap it in a d2d::context (mirrors the 2020 branch).
   target_include_directories(artist PUBLIC impl/windows/direct2d)
   # Direct2D / DirectWrite / WIC are system libs — the whole point: no Skia
   # blob. d2d1.lib also provides the D2D 1.1 (ID2D1DeviceContext) API, and
   # dxguid the D2D effect CLSIDs (Gaussian blur / 2D affine transform).
   target_link_libraries(artist PUBLIC
      d2d1 dwrite windowscodecs dxgi dxguid ole32
   )

   # The one non-system dependency: WIC has no WebP codec (it ships as an
   # optional Store package), so the backend decodes WebP itself with libwebp.
   # A libwebp on CMAKE_PREFIX_PATH is used when there is one. Otherwise only
   # libwebp's decoder is built from source, so nothing has to be installed.
   find_package(WebP CONFIG QUIET)
   if (NOT TARGET WebP::webpdecoder)
      include(FetchContent)
      foreach (_tool ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO
            LIBWEBPMUX WEBPMUX EXTRAS)
         set(WEBP_BUILD_${_tool} OFF CACHE BOOL "" FORCE)
      endforeach()
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
      FetchContent_Declare(libwebp
         GIT_REPOSITORY https://github.com/webmproject/libwebp.git
         GIT_TAG v1.6.0
         GIT_SHALLOW TRUE
      )
      FetchContent_MakeAvailable(libwebp)
      unset(CMAKE_MSVC_RUNTIME_LIBRARY)
      # Build only what Artist links, not the encoder and the rest.
      set_property(DIRECTORY ${libwebp_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL TRUE)
      target_include_directories(webpdecoder INTERFACE
         $<BUILD_INTERFACE:${libwebp_SOURCE_DIR}/src>)
      add_library(WebP::webpdecoder ALIAS webpdecoder)
   endif()
   target_link_libraries(artist PUBLIC WebP::webpdecoder)
endif()

if (ARTIST_SKIA AND NOT APPLE)
   target_compile_definitions(
      artist
      PUBLIC
         SK_GL
         SK_GANESH
   )
endif()

target_compile_features(artist PUBLIC cxx_std_17)

if (IPO_SUPPORTED AND CMAKE_BUILD_TYPE STREQUAL "Release")
   message(STATUS "Enabling LTO for artist")
   set_target_properties(artist PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
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
      # Follow the configuration being built. CMAKE_BUILD_TYPE is empty for
      # multi-config generators, and keying off the compiler picked the debug
      # CRT for clang-cl even in Release, which mismatched artist and elements
      # on _ITERATOR_DEBUG_LEVEL and failed the link.
      set_property(TARGET artist PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
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
   if (APPLE)
      target_link_libraries(artist PUBLIC ${ARTIST_DEPEPENDENCIES})
   else()
      target_link_libraries(artist PRIVATE "${ARTIST_DEPEPENDENCIES}")

      # Keep the bundled static harfbuzz out of the consuming executable's
      # dynamic symbol table. GTK and Pango call the system libharfbuzz, and an
      # exported static copy of a different version interposes on it, crashing
      # in Pango shaping when a widget is realized. This has to be a link
      # option, not a link library, so it precedes the archives on the command
      # line. ELF only: symbol interposition is not a thing on Windows, and
      # link.exe would read the flag as an input file name.
      if (NOT MSVC)
         target_link_options(artist INTERFACE "LINKER:--exclude-libs,libharfbuzz.a")
      endif()
   endif()
endif()


