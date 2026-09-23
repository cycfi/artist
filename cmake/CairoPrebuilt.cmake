# CairoPrebuilt.cmake
#
# Fetch a prebuilt bundle of the Cairo backend's dependencies (cairo,
# harfbuzz, fontconfig, freetype, libwebp and the pkgconf tool) from the
# Cloudflare R2 cache, and point pkg-config at it. This is the DEFAULT path
# on macOS and Windows, where those libraries are otherwise Homebrew's or a
# vcpkg source build.
#
# Linux is not served: its libraries come from the distribution, which also
# supplies the ones the window hosts load.
#
# No-op when a vcpkg toolchain is in use (VCPKG_TOOLCHAIN) — there vcpkg
# provides the libraries (the producer/CI path).
#
# Bundles live at ${ARTIST_CAIRO_PREBUILT_URL}/${version}/<triplet>-<abi>.tar.zst,
# named by <triplet>.current, and are the tarred vcpkg installed/<triplet> tree.
#
# Controls: ARTIST_CAIRO_PREBUILT (ON), ARTIST_CAIRO_PREBUILT_VERSION (1),
#           ARTIST_CAIRO_PREBUILT_URL, ARTIST_CAIRO_PREBUILT_DIR (cache dir).

option(ARTIST_CAIRO_PREBUILT
  "Fetch the Cairo backend's dependencies from a prebuilt bundle" ON)
set(ARTIST_CAIRO_PREBUILT_VERSION "1" CACHE STRING
  "Prebuilt Cairo dependency bundle version")
set(ARTIST_CAIRO_PREBUILT_URL "https://media.cycfi.com/cairo-prebuilt" CACHE STRING
  "Base URL for prebuilt Cairo dependency bundles")

# vcpkg in charge, Linux, or turned off? then the libraries come from there.
if(VCPKG_TOOLCHAIN OR NOT ARTIST_CAIRO_PREBUILT OR (UNIX AND NOT APPLE))
  return()
endif()

set(_help "Install the libraries yourself instead (macOS: brew install cairo harfbuzz fontconfig freetype webp; Windows: the vcpkg toolchain, see README), and configure with -DARTIST_CAIRO_PREBUILT=OFF.")

# --- derive the os+arch triplet -------------------------------------------
set(_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
  list(GET CMAKE_OSX_ARCHITECTURES 0 _arch)   # honour an explicit target arch
endif()
string(TOLOWER "${_arch}" _arch)

set(_triplet "")
if(APPLE)
  if(_arch MATCHES "arm64|aarch64")
    set(_triplet "arm64-osx")
  elseif(_arch MATCHES "x86_64|amd64|x64")
    set(_triplet "x64-osx")
  endif()
elseif(WIN32)
  if(_arch MATCHES "arm64|aarch64")
    set(_triplet "arm64-windows")
  else()
    set(_triplet "x64-windows")
  endif()
endif()

if(NOT _triplet)
  message(FATAL_ERROR
    "Artist: no prebuilt Cairo dependency bundle for arch '${_arch}'. ${_help}")
endif()

# --- compatibility gate (bundle baseline vs local toolchain) --------------
set(_incompat "")
if(WIN32)
  if(DEFINED MSVC_TOOLSET_VERSION AND NOT MSVC_TOOLSET_VERSION STREQUAL "143")
    set(_incompat "MSVC toolset v${MSVC_TOOLSET_VERSION} != bundle v143")
  endif()
elseif(APPLE)
  if(CMAKE_OSX_DEPLOYMENT_TARGET AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "11.0")
    set(_incompat "deployment target ${CMAKE_OSX_DEPLOYMENT_TARGET} < bundle 11.0")
  endif()
endif()

if(_incompat)
  message(FATAL_ERROR
    "Artist: the prebuilt Cairo dependency bundle for ${_triplet} is "
    "incompatible with this toolchain (${_incompat}). ${_help}")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/PrebuiltBundle.cmake)

artist_fetch_bundle(
  NAME       "Cairo dependency"
  URL        "${ARTIST_CAIRO_PREBUILT_URL}"
  VERSION    "${ARTIST_CAIRO_PREBUILT_VERSION}"
  TRIPLET    "${_triplet}"
  CACHE_NAME "cairo-prebuilt"
  CACHE_DIR  "${ARTIST_CAIRO_PREBUILT_DIR}"
  MARKER     "lib/pkgconfig/cairo.pc"
  HELP       "${_help}"
  OUT_DIR    _dest
)

# The bundle's own pkgconf, so nothing has to be installed, reading only the
# bundle's .pc files. Their prefix is the machine that built them, so
# --define-prefix has pkgconf derive it from where the file actually is.
find_program(ARTIST_PKG_CONFIG_EXECUTABLE
  NAMES pkgconf pkg-config
  PATHS "${_dest}/tools/pkgconf" "${_dest}/tools/pkgconfig"
  NO_DEFAULT_PATH
)
if(ARTIST_PKG_CONFIG_EXECUTABLE)
  set(PKG_CONFIG_EXECUTABLE "${ARTIST_PKG_CONFIG_EXECUTABLE}" CACHE FILEPATH
    "pkg-config from Artist's prebuilt Cairo bundle" FORCE)
endif()

# PKG_CONFIG_ARGN reaches pkg_check_modules from CMake 3.22 on; below that the
# .pc files' own prefix is used, which is why the bundle is extracted to a
# fixed cache path.
list(APPEND PKG_CONFIG_ARGN --define-prefix)
set(ENV{PKG_CONFIG_LIBDIR} "${_dest}/lib/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "${_dest}/lib/pkgconfig")

list(PREPEND CMAKE_PREFIX_PATH "${_dest}")

# Windows builds the bundle's libraries as DLLs, and pkg-config names their
# import libraries, so they never show up in TARGET_RUNTIME_DLLS. Record them
# for the post-build copy next to the executables.
if(WIN32)
  file(GLOB _rel_dlls "${_dest}/bin/*.dll")
  file(GLOB _dbg_dlls "${_dest}/debug/bin/*.dll")
  set(_dlls "")
  foreach(_dll IN LISTS _rel_dlls)
    list(APPEND _dlls "$<$<NOT:$<CONFIG:Debug>>:${_dll}>")
  endforeach()
  foreach(_dll IN LISTS _dbg_dlls)
    list(APPEND _dlls "$<$<CONFIG:Debug>:${_dll}>")
  endforeach()
  set_property(GLOBAL PROPERTY ARTIST_CAIRO_RUNTIME_DLLS "${_dlls}")
endif()

message(STATUS
  "Artist: using the prebuilt Cairo dependencies ${_triplet}@${ARTIST_CAIRO_PREBUILT_VERSION} (${_dest})")
