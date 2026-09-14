# SkiaPrebuilt.cmake
#
# Fetch a prebuilt Skia bundle (Skia + its deps, debug AND release) from the
# Cloudflare R2 cache and make it discoverable by find_package(unofficial-skia).
# This is the DEFAULT path for local builds: it links a prebuilt Skia that is
# compatible across compiler versions (unlike vcpkg's ABI-exact binary cache),
# so a fresh clone builds fast without compiling Skia.
#
# No-op when a vcpkg toolchain is in use (VCPKG_TOOLCHAIN) — there vcpkg provides
# Skia (the producer/CI path, and the "source" CMake preset). So once this code
# runs, there is NO toolchain; if we cannot supply a usable bundle we stop with a
# clear instruction to use the source path ("simple" fallback — we never switch
# the toolchain mid-configure).
#
# Bundles live at ${ARTIST_SKIA_PREBUILT_URL}/${version}/<triplet>-<abi>.tar.zst,
# named by <triplet>.current, and are the tarred vcpkg installed/<triplet> tree.
#
# COMPATIBILITY: <triplet> (os+arch) is necessary but not sufficient, so a gate
# checks the bundle's baseline against the local toolchain and refuses an
# incompatible bundle (else the user gets cryptic link/loader errors):
#   Linux   - bundles built on glibc 2.35 (ubuntu-22.04); need local glibc >= that
#   Windows - bundles built with MSVC v143; need the same toolset major
#   macOS   - bundles target macOS 11.0 (libc++ stable across clang versions)
#
# Controls: ARTIST_SKIA_PREBUILT (ON), ARTIST_SKIA_PREBUILT_VERSION (148),
#           ARTIST_SKIA_PREBUILT_URL, ARTIST_SKIA_PREBUILT_DIR (cache dir).

option(ARTIST_SKIA_PREBUILT
  "Fetch a prebuilt Skia bundle from R2 instead of building Skia via vcpkg" ON)
set(ARTIST_SKIA_PREBUILT_VERSION "148" CACHE STRING
  "Prebuilt Skia bundle version (Skia milestone)")
set(ARTIST_SKIA_PREBUILT_URL "https://media.cycfi.com/skia-prebuilt" CACHE STRING
  "Base URL for prebuilt Skia bundles")

# vcpkg in charge? then it supplies Skia; nothing to do here.
if(VCPKG_TOOLCHAIN OR NOT ARTIST_SKIA_PREBUILT)
  return()
endif()

set(_help "Build Skia from source instead: run 'git submodule update --init --checkout lib/external/vcpkg', then configure with the 'source' CMake preset, or pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake (elements also needs -DVCPKG_MANIFEST_DIR=lib/artist). Set -DARTIST_SKIA_PREBUILT=OFF to silence the prebuilt path.")

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
else()
  if(_arch MATCHES "arm64|aarch64")
    set(_triplet "arm64-linux")
  elseif(_arch MATCHES "x86_64|amd64|x64")
    set(_triplet "x64-linux")
  endif()
endif()

if(NOT _triplet)
  message(FATAL_ERROR "Artist: no prebuilt Skia bundle for arch '${_arch}'. ${_help}")
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
else() # Linux: compare runtime glibc to the bundle's build floor
  execute_process(COMMAND getconf GNU_LIBC_VERSION
    OUTPUT_VARIABLE _glibc OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  string(REGEX MATCH "[0-9]+\\.[0-9]+" _glibc "${_glibc}")
  if(_glibc AND _glibc VERSION_LESS "2.35")
    set(_incompat "glibc ${_glibc} < bundle floor 2.35")
  endif()
endif()

if(_incompat)
  message(FATAL_ERROR
    "Artist: prebuilt Skia for ${_triplet} is incompatible with this toolchain "
    "(${_incompat}). ${_help}")
endif()

# --- local cache location -------------------------------------------------
if(DEFINED ARTIST_SKIA_PREBUILT_DIR)
  set(_cache "${ARTIST_SKIA_PREBUILT_DIR}")
elseif(DEFINED ENV{XDG_CACHE_HOME})
  set(_cache "$ENV{XDG_CACHE_HOME}/cycfi/skia-prebuilt")
elseif(DEFINED ENV{HOME})
  set(_cache "$ENV{HOME}/.cache/cycfi/skia-prebuilt")
elseif(DEFINED ENV{LOCALAPPDATA})
  set(_cache "$ENV{LOCALAPPDATA}/cycfi/skia-prebuilt")
else()
  set(_cache "${CMAKE_BINARY_DIR}/skia-prebuilt")
endif()

set(_ver "${ARTIST_SKIA_PREBUILT_VERSION}")
set(_root "${_cache}/${_ver}")
set(_dest "${_root}/${_triplet}")
set(_marker "${_dest}/share/unofficial-skia/unofficial-skia-config.cmake")

# --- download + verify + extract ------------------------------------------
# <triplet>.current on the server holds "<file> <sha256>" for the latest
# bundle, whose file name carries its Skia ABI and is never replaced, so a
# CDN cannot pair a new checksum with an old file. Servers without the pointer
# fall back to <triplet>.tar.zst and its .sha256. The pointer of the extracted
# bundle is kept beside it and compared on every configure, so a new bundle
# replaces the local copy. When the server can't be reached, the local copy is
# used as is.
set(_base_url "${ARTIST_SKIA_PREBUILT_URL}/${_ver}")
set(_tar "${_root}/${_triplet}.tar.zst")
set(_stored "${_root}/${_triplet}.current")
set(_shafile "${_root}/${_triplet}.tar.zst.sha256")
set(_fetched "${_root}/${_triplet}.fetched")
file(MAKE_DIRECTORY "${_root}")

set(_file "")
set(_sha "")
file(DOWNLOAD "${_base_url}/${_triplet}.current" "${_fetched}" STATUS _st TIMEOUT 30)
list(GET _st 0 _code)
if(_code EQUAL 0)
  file(READ "${_fetched}" _pointer)
  string(STRIP "${_pointer}" _pointer)
  if(_pointer MATCHES "^([^ ]+) +([0-9a-f]+)$")
    set(_file "${CMAKE_MATCH_1}")
    set(_sha "${CMAKE_MATCH_2}")
  endif()
endif()
if(NOT _sha)
  file(DOWNLOAD "${_base_url}/${_triplet}.tar.zst.sha256" "${_fetched}" STATUS _st TIMEOUT 30)
  list(GET _st 0 _code)
  if(_code EQUAL 0)
    file(READ "${_fetched}" _sha)
    string(STRIP "${_sha}" _sha)
    set(_file "${_triplet}.tar.zst")
  endif()
endif()
file(REMOVE "${_fetched}")

if(NOT _sha AND NOT EXISTS "${_marker}")
  message(FATAL_ERROR
    "Artist: no prebuilt Skia bundle for ${_triplet}@${_ver} "
    "(${_base_url}/${_triplet}.tar.zst.sha256: ${_st}). ${_help}")
elseif(NOT _sha)
  message(STATUS "Artist: cannot check prebuilt Skia ${_triplet}@${_ver} "
    "for updates (${_st}); using the local copy")
endif()

set(_local "")
if(EXISTS "${_stored}")
  file(READ "${_stored}" _local)
  string(STRIP "${_local}" _local)
endif()

if(_sha AND (NOT EXISTS "${_marker}" OR NOT "${_file} ${_sha}" STREQUAL _local))
  if(EXISTS "${_dest}")
    message(STATUS "Artist: prebuilt Skia ${_triplet}@${_ver} changed on the server")
  endif()
  message(STATUS "Artist: downloading prebuilt Skia ${_triplet}@${_ver} …")
  # Verified by hand: a failed EXPECTED_HASH raises a CMake error, which would
  # fail the configure even when the local copy is kept.
  file(DOWNLOAD "${_base_url}/${_file}" "${_tar}" STATUS _dlst SHOW_PROGRESS)
  list(GET _dlst 0 _dlcode)
  if(_dlcode EQUAL 0)
    file(SHA256 "${_tar}" _got)
    if(NOT _got STREQUAL _sha)
      set(_dlcode 1)
      set(_dlst "sha256 ${_got}, expected ${_sha}")
    endif()
  endif()
  if(NOT _dlcode EQUAL 0)
    file(REMOVE "${_tar}")
    if(EXISTS "${_marker}")
      message(WARNING
        "Artist: prebuilt Skia download/verify failed for ${_triplet}@${_ver} "
        "(${_dlst}); using the local copy")
    else()
      message(FATAL_ERROR
        "Artist: prebuilt Skia download/verify failed for ${_triplet}@${_ver} "
        "(${_dlst}). Retry, or: ${_help}")
    endif()
  else()
    # The old copy goes only once the new bundle is verified, and the pointer
    # is written only after a complete extract, so an interrupted update is
    # retried on the next configure.
    file(REMOVE "${_stored}")
    file(REMOVE "${_shafile}")
    file(REMOVE_RECURSE "${_dest}")
    file(ARCHIVE_EXTRACT INPUT "${_tar}" DESTINATION "${_root}")
    file(REMOVE "${_tar}")
    if(EXISTS "${_marker}")
      file(WRITE "${_stored}" "${_file} ${_sha}\n")
      # The same bundle's checksum, for older checkouts sharing this cache.
      file(WRITE "${_shafile}" "${_sha}\n")
    endif()
  endif()
endif()

if(NOT EXISTS "${_marker}")
  message(FATAL_ERROR
    "Artist: prebuilt Skia bundle for ${_triplet}@${_ver} is corrupt "
    "(missing unofficial-skia config after extract). ${_help}")
endif()

# include() runs in the caller's scope, so this updates the includer's
# CMAKE_PREFIX_PATH directly (no PARENT_SCOPE needed).
list(PREPEND CMAKE_PREFIX_PATH "${_dest}")

# On Windows the bundle's Skia and its dependencies are DLLs, but Skia is
# imported as an UNKNOWN library, so they never show up in an executable's
# TARGET_RUNTIME_DLLS. Record them (release or debug, by configuration) for
# the post-build copy next to the executables.
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
  set_property(GLOBAL PROPERTY ARTIST_SKIA_RUNTIME_DLLS "${_dlls}")
endif()
message(STATUS "Artist: using prebuilt Skia ${_triplet}@${_ver} (${_dest})")
