# SkiaPrebuilt.cmake
#
# Fetch a prebuilt Skia bundle (Skia + its deps, debug AND release) from the
# Cloudflare R2 cache and make it discoverable by find_package(unofficial-skia).
# This is the DEFAULT path for local builds: it links a prebuilt Skia that is
# compatible across compiler versions (unlike vcpkg's ABI-exact binary cache),
# so a fresh clone builds fast without compiling Skia.
#
# It is a no-op when a vcpkg toolchain is in use (VCPKG_TOOLCHAIN) — there vcpkg
# provides Skia (and its own ABI-exact binary cache). The producer (CI / a local
# seed) uses the vcpkg path; everyone else uses these bundles.
#
# Bundles live at:
#   ${ARTIST_SKIA_PREBUILT_URL}/${version}/<triplet>.tar.zst  (+ .sha256)
# and are the tarred vcpkg installed/<triplet> tree.
#
# Controls (cache vars):
#   ARTIST_SKIA_PREBUILT          ON  - master switch
#   ARTIST_SKIA_PREBUILT_VERSION  148 - bundle version (Skia milestone)
#   ARTIST_SKIA_PREBUILT_URL          - base URL
#   ARTIST_SKIA_PREBUILT_DIR          - local cache dir (downloaded/extracted)

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

# --- derive the compatibility triplet (os + arch) -------------------------
set(_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
  # honour an explicit cross/target arch (e.g. x86_64 on an arm64 host)
  list(GET CMAKE_OSX_ARCHITECTURES 0 _arch)
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
else() # Linux / other Unix
  if(_arch MATCHES "arm64|aarch64")
    set(_triplet "arm64-linux")
  elseif(_arch MATCHES "x86_64|amd64|x64")
    set(_triplet "x64-linux")
  endif()
endif()

if(NOT _triplet)
  message(STATUS
    "Artist: no prebuilt Skia triplet for arch '${_arch}' — "
    "falling back to vcpkg (pass -DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake).")
  return()
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

# --- download + verify + extract (once) -----------------------------------
if(NOT EXISTS "${_marker}")
  set(_base "${ARTIST_SKIA_PREBUILT_URL}/${_ver}/${_triplet}.tar.zst")
  set(_tar "${_root}/${_triplet}.tar.zst")
  file(MAKE_DIRECTORY "${_root}")

  # fetch the expected checksum first
  set(_shafile "${_root}/${_triplet}.tar.zst.sha256")
  file(DOWNLOAD "${_base}.sha256" "${_shafile}" STATUS _shast)
  list(GET _shast 0 _shacode)
  if(NOT _shacode EQUAL 0)
    message(STATUS
      "Artist: no prebuilt Skia bundle for ${_triplet}@${_ver} "
      "(${_base}.sha256: ${_shast}). Falling back to vcpkg "
      "(-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake to build from source).")
    return()
  endif()
  file(READ "${_shafile}" _sha)
  string(STRIP "${_sha}" _sha)

  message(STATUS "Artist: downloading prebuilt Skia ${_triplet}@${_ver} …")
  file(DOWNLOAD "${_base}" "${_tar}"
    EXPECTED_HASH "SHA256=${_sha}"
    STATUS _dlst SHOW_PROGRESS)
  list(GET _dlst 0 _dlcode)
  if(NOT _dlcode EQUAL 0)
    file(REMOVE "${_tar}")
    message(STATUS
      "Artist: prebuilt Skia download/verify failed for ${_triplet}@${_ver} "
      "(${_dlst}). Falling back to vcpkg.")
    return()
  endif()

  file(ARCHIVE_EXTRACT INPUT "${_tar}" DESTINATION "${_root}")
  file(REMOVE "${_tar}")
endif()

if(EXISTS "${_marker}")
  # include() runs in the caller's scope, so this updates the includer's
  # CMAKE_PREFIX_PATH directly (no PARENT_SCOPE needed).
  list(PREPEND CMAKE_PREFIX_PATH "${_dest}")
  message(STATUS "Artist: using prebuilt Skia ${_triplet}@${_ver} (${_dest})")
else()
  message(STATUS
    "Artist: prebuilt Skia not available after fetch; falling back to vcpkg.")
endif()
