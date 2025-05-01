###############################################################################
#  Copyright (c) 2016-2023 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
cmake_minimum_required(VERSION 3.7.2...3.15.0)

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
   if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
      set_property(TARGET libunibreak PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
      )
   else()
      set_property(TARGET libunibreak PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreaded"
      )
   endif()
endif()

if (ARTIST_SKIA)

   ############################################################################
   # Prebuilt binaries
   ############################################################################
   if (APPLE)

      find_package(PkgConfig REQUIRED)
      pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)

      if (${CMAKE_OSX_ARCHITECTURES} STREQUAL "arm64")
         set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/macos/arm64/src/prebuilt_binaries)
         set(ARTIST_BINARIES_URL https://github.com/cycfi/artist/raw/prebuilt_binaries_0.90/macos/skia/arm64/arm64.zip)
         set(ARTIST_BINARIES_MD5 5a8b2ef69844bdfff8f384538b63a962)
      elseif (${CMAKE_OSX_ARCHITECTURES} STREQUAL "x86_64")
         set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/macos/x86_64/src/prebuilt_binaries)
         set(ARTIST_BINARIES_URL https://github.com/cycfi/artist/raw/prebuilt_binaries_0.90/macos/skia/x86_64/x86_64.zip)
         set(ARTIST_BINARIES_MD5 553b7073295cf9650c54a79478658761)
      endif()

      set(SKIA_LIBRARIES
         ${ARTIST_BINARIES}/libskia.a
         ${ARTIST_BINARIES}/libharfbuzz.a
         ${ARTIST_BINARIES}/libthird_party__libpng.a
         ${ARTIST_BINARIES}/libthird_party__zlib.a
         ${ARTIST_BINARIES}/libthird_party__libwebp.a
         ${ARTIST_BINARIES}/libthird_party__libwebp_libwebp_sse41.a
         ${ARTIST_BINARIES}/libthird_party__libjpeg-turbo_libjpeg.a
         ${ARTIST_BINARIES}/libmodules__skcms.a
      )

      ExternalProject_Add(prebuilt_binaries
         PREFIX "macos/${CMAKE_OSX_ARCHITECTURES}"
         URL ${ARTIST_BINARIES_URL}
         URL_MD5 ${ARTIST_BINARIES_MD5}
         CONFIGURE_COMMAND ""
         BUILD_COMMAND ""
         INSTALL_COMMAND ""
         BUILD_BYPRODUCTS ${SKIA_LIBRARIES}
      )

      find_package(OpenGL REQUIRED)

      set(ARTIST_DEPEPENDENCIES
         "${SKIA_LIBRARIES}"
         PkgConfig::fontconfig
         ${OPENGL_LIBRARIES}
      )

   elseif (MSVC)

      if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
         set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/windows/msvc/x86_64/src/prebuilt_binaries)
         if (CMAKE_BUILD_TYPE STREQUAL "Release")
            set(ARTIST_BINARIES_URL https://github.com/cycfi/artist/raw/prebuilt_binaries_0.90/windows/skia/msvc/x86_64/x86_64.zip)
            set(ARTIST_BINARIES_MD5 93b0a532f87c1068bc2adabf19da086d)
            set(ARTIST_BINARIES_PREFIX windows/msvc/x86_64)
         else()
            set(ARTIST_BINARIES_URL https://github.com/cycfi/artist/raw/prebuilt_binaries_0.90/windows/skia/msvc/x86_64-dbg/x86_64-dbg.zip)
            set(ARTIST_BINARIES_MD5 b2e5f3927c3b6b3f41ca1c51c8b6a55f)
            set(ARTIST_BINARIES_PREFIX windows/msvc/x86_64)
         endif()
      elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
         if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(WARNING "Warning: There are no precompiled binaries built using MTd for Clang. Use MT.")
         endif()
         set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/windows/clang/x86_64/src/prebuilt_binaries)
         set(ARTIST_BINARIES_URL https://github.com/cycfi/artist/raw/prebuilt_binaries_0.90/windows/skia/clang/x86_64/x86_64.zip)
         set(ARTIST_BINARIES_MD5 b2bc06c19cd61f93505b7e37eaf489db)
         set(ARTIST_BINARIES_PREFIX windows/clang/x86_64)
      endif()

      set(SKIA_LIBRARIES
         ${ARTIST_BINARIES}/skia.lib
         ${ARTIST_BINARIES}/harfbuzz.lib
         ${ARTIST_BINARIES}/fontconfig.lib
         ${ARTIST_BINARIES}/freetype.lib
         ${ARTIST_BINARIES}/bz2.lib
         ${ARTIST_BINARIES}/brotlicommon.lib
         ${ARTIST_BINARIES}/brotlienc.lib
         ${ARTIST_BINARIES}/brotlidec.lib
         ${ARTIST_BINARIES}/libjpeg.lib
         ${ARTIST_BINARIES}/libpng.lib
         ${ARTIST_BINARIES}/libwebp_sse41.lib
         ${ARTIST_BINARIES}/libwebp.lib
         ${ARTIST_BINARIES}/zlib.lib
         ${ARTIST_BINARIES}/skcms.lib
         ${ARTIST_BINARIES}/expat.lib
      )

      ExternalProject_Add(prebuilt_binaries
         PREFIX "${ARTIST_BINARIES_PREFIX}"
         URL ${ARTIST_BINARIES_URL}
         URL_MD5 ${ARTIST_BINARIES_MD5}
         CONFIGURE_COMMAND ""
         BUILD_COMMAND ""
         INSTALL_COMMAND ""
         BUILD_BYPRODUCTS ${SKIA_LIBRARIES}
      )

      find_package(OpenGL REQUIRED)

      set(ARTIST_DEPEPENDENCIES
         "${SKIA_LIBRARIES}"
         ${OPENGL_LIBRARIES}
      )

      set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
      set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /GL /MT")

   elseif (UNIX)

      if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
         set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/linux/gcc/x86_64/src/prebuilt_binaries)
         set(ARTIST_BINARIES_URL https://github.com/cycfi/artist/raw/prebuilt_binaries_0.90/linux/skia/gcc/x86_64/libskia.a.zip)
         set(ARTIST_BINARIES_MD5 18def70ddce24ea79a7a308163dfbda3)
         set(ARTIST_BINARIES_PREFIX linux/gcc/x86_64)
      elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
      set(ARTIST_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/linux/clang/x86_64/src/prebuilt_binaries)
      set(ARTIST_BINARIES_URL https://github.com/cycfi/artist/raw/prebuilt_binaries_0.90/linux/skia/clang/x86_64/libskia.a.zip)
      set(ARTIST_BINARIES_MD5 db92deccb22589f2551955003ef37ce6)
      set(ARTIST_BINARIES_PREFIX linux/clang/x86_64)
   endif()

      find_package(Threads)
      find_package(PkgConfig REQUIRED)
      find_package(OpenGL REQUIRED)

      pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
      pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)
      pkg_check_modules(freetype2 REQUIRED IMPORTED_TARGET freetype2)

      set(SKIA_LIBRARY ${ARTIST_BINARIES}/libskia.a)

      ExternalProject_Add(prebuilt_binaries
         PREFIX "${ARTIST_BINARIES_PREFIX}"
         URL ${ARTIST_BINARIES_URL}
         URL_MD5 ${ARTIST_BINARIES_MD5}
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
         OpenGL::OpenGL
         OpenGL::GLX
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
         external/skia/tools/window/mac/GLWindowContext_mac.mm
         external/skia/tools/window/mac/RasterWindowContext_mac.mm
      )
      set_source_files_properties(${ARTIST_SKIA_PLATFORM_WINDOW_CONTEXT}
         PROPERTIES COMPILE_FLAGS -fno-objc-arc
      )
   elseif (MSVC)
      set(ARTIST_SKIA_PLATFORM_WINDOW_CONTEXT
         external/skia/tools/window/win/GLWindowContext_win.cpp
         external/skia/tools/window/win/RasterWindowContext_win.cpp
      )
   endif()

   set(ARTIST_IMPL
      impl/skia/canvas.cpp
      impl/skia/image.cpp
      impl/skia/font.cpp
      impl/skia/text_layout.cpp
      impl/skia/path.cpp
      impl/skia/detail/harfbuzz.cpp
      external/skia/tools/window/GLWindowContext.cpp
      external/skia/tools/window/WindowContext.cpp
      #external/skia/tools/ToolUtils.cpp
      #external/skia/tools/SkMetaData.cpp
      ${ARTIST_SKIA_PLATFORM_WINDOW_CONTEXT}
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
         #external/filesystem/include/
         external/harfbuzz/include/
         external/fontconfig/include/
   )
   target_compile_definitions(
      artist
      PUBLIC
         ARTIST_SKIA
         SK_RELEASE
         SK_GL
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
   if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
      set_property(TARGET artist PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreaded"
      )
   else()
      set_property(TARGET artist PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
      )
   endif()


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
      "${ARTIST_DEPEPENDENCIES}"
   )
endif()


