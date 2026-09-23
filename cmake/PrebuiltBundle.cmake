# PrebuiltBundle.cmake
#
# Fetch a prebuilt bundle (a tarred vcpkg installed/<triplet> tree) from the
# Cloudflare R2 cache and extract it, so a fresh clone builds without
# compiling the dependency. Used by SkiaPrebuilt.cmake and CairoPrebuilt.cmake.
#
# artist_fetch_bundle(
#    NAME <label>        what to call it in messages, e.g. "Skia"
#    URL <base>          bundles live at <base>/<version>/<file>
#    VERSION <v>         one directory of bundles
#    TRIPLET <t>         os+arch, e.g. x64-windows
#    CACHE_NAME <dir>    cache directory under ~/.cache/cycfi
#    MARKER <path>       file inside the bundle that proves a good extract
#    HELP <text>         what to do when no bundle can be had
#    OUT_DIR <var>       set to the extracted tree
# )
#
# <triplet>.current on the server holds "<file> <sha256>" for the latest
# bundle, whose file name carries its vcpkg ABI and is never replaced, so a
# CDN cannot pair a new checksum with an old file. Servers without the pointer
# fall back to <triplet>.tar.zst and its .sha256. The pointer of the extracted
# bundle is kept beside it and compared on every configure, so a new bundle
# replaces the local copy. When the server cannot be reached, the local copy
# is used as is.

function(artist_fetch_bundle)
  cmake_parse_arguments(_b ""
    "NAME;URL;VERSION;TRIPLET;CACHE_NAME;MARKER;HELP;OUT_DIR;CACHE_DIR" "" ${ARGN})

  # --- local cache location -----------------------------------------------
  if(_b_CACHE_DIR)
    set(_cache "${_b_CACHE_DIR}")
  elseif(DEFINED ENV{XDG_CACHE_HOME})
    set(_cache "$ENV{XDG_CACHE_HOME}/cycfi/${_b_CACHE_NAME}")
  elseif(DEFINED ENV{HOME})
    set(_cache "$ENV{HOME}/.cache/cycfi/${_b_CACHE_NAME}")
  elseif(DEFINED ENV{LOCALAPPDATA})
    set(_cache "$ENV{LOCALAPPDATA}/cycfi/${_b_CACHE_NAME}")
  else()
    set(_cache "${CMAKE_BINARY_DIR}/${_b_CACHE_NAME}")
  endif()

  set(_ver "${_b_VERSION}")
  set(_triplet "${_b_TRIPLET}")
  set(_what "${_b_NAME}")
  set(_help "${_b_HELP}")
  set(_root "${_cache}/${_ver}")
  set(_dest "${_root}/${_triplet}")
  set(_marker "${_dest}/${_b_MARKER}")
  set(${_b_OUT_DIR} "${_dest}" PARENT_SCOPE)

  set(_base_url "${_b_URL}/${_ver}")
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
      "Artist: no prebuilt ${_what} bundle for ${_triplet}@${_ver} "
      "(${_base_url}/${_triplet}.tar.zst.sha256: ${_st}). ${_help}")
  elseif(NOT _sha)
    message(STATUS "Artist: cannot check prebuilt ${_what} ${_triplet}@${_ver} "
      "for updates (${_st}); using the local copy")
  endif()

  set(_local "")
  if(EXISTS "${_stored}")
    file(READ "${_stored}" _local)
    string(STRIP "${_local}" _local)
  endif()

  if(_sha AND (NOT EXISTS "${_marker}" OR NOT "${_file} ${_sha}" STREQUAL _local))
    if(EXISTS "${_dest}")
      message(STATUS "Artist: prebuilt ${_what} ${_triplet}@${_ver} changed on the server")
    endif()
    message(STATUS "Artist: downloading prebuilt ${_what} ${_triplet}@${_ver} …")
    # Verified by hand: a failed EXPECTED_HASH raises a CMake error, which
    # would fail the configure even when the local copy is kept.
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
          "Artist: prebuilt ${_what} download/verify failed for ${_triplet}@${_ver} "
          "(${_dlst}); using the local copy")
      else()
        message(FATAL_ERROR
          "Artist: prebuilt ${_what} download/verify failed for ${_triplet}@${_ver} "
          "(${_dlst}). Retry, or: ${_help}")
      endif()
    else()
      # The old copy goes only once the new bundle is verified, and the
      # pointer is written only after a complete extract, so an interrupted
      # update is retried on the next configure.
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
      "Artist: prebuilt ${_what} bundle for ${_triplet}@${_ver} is corrupt "
      "(missing ${_b_MARKER} after extract). ${_help}")
  endif()
endfunction()
