# Finds the Wayland client libraries
# Also supports components "protocols" and "scanner"

find_package(PkgConfig QUIET)

pkg_check_modules(PKG_Wayland QUIET wayland-protocols wayland-scanner)

set(WaylandScanner_VERSION ${PKG_Wayland_VERSION})
set(WaylandScanner_DEFINITIONS ${PKG_Wayland_CFLAGS})
mark_as_advanced(WaylandScanner_DEFINITIONS)

pkg_get_variable(Wayland_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
mark_as_advanced(Wayland_PROTOCOLS_DATADIR)

if (NOT Wayland_PROTOCOLS_DATADIR)
        message(FATAL "The wayland-protocols data directory has not been found on your system.")
endif()

pkg_get_variable(WaylandScanner_EXECUTABLE_DIR wayland-scanner bindir)
find_program(WaylandScanner_EXECUTABLE NAMES wayland-scanner HINTS ${WaylandScanner_EXECUTABLE_DIR})
mark_as_advanced(WaylandScanner_EXECUTABLE WaylandScanner_EXECUTABLE_DIR)

set(WaylandScanner_DATADIR ${Wayland_PROTOCOLS_DATADIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(WaylandScanner
    FOUND_VAR WaylandScanner_FOUND
    VERSION_VAR WaylandScanner_VERSION
    REQUIRED_VARS WaylandScanner_EXECUTABLE ) #Wayland_LIBRARIES)

if (NOT TARGET Wayland::Scanner AND WaylandScanner_FOUND)
    add_executable(Wayland::Scanner IMPORTED)
    set_target_properties(Wayland::Scanner PROPERTIES
        IMPORTED_LOCATION "${WaylandScanner_EXECUTABLE}")
endif()

function(wayland_add_protocol_client _sources _workdir _protocol)
    if (NOT TARGET Wayland::Scanner)
        message(FATAL "The wayland-scanner executable has not been found on your system.")
    endif()

    get_filename_component(_basename ${_protocol} NAME_WLE)

    set(_source_file "${_protocol}")
    set(_client_header "${_workdir}/${_basename}-client-protocol.h")
    set(_private_code "${_workdir}/${_basename}-protocol.c")

    add_custom_command(OUTPUT ${_client_header}
        COMMAND Wayland::Scanner client-header ${_source_file} ${_client_header}
        DEPENDS ${_source_file} VERBATIM)

    add_custom_command(OUTPUT ${_private_code}
        COMMAND Wayland::Scanner private-code ${_source_file} ${_private_code}
        DEPENDS ${_source_file} ${_client_header} VERBATIM)

    list(APPEND ${_sources} ${_client_header} ${_private_code})
    set(${_sources} ${${_sources}} PARENT_SCOPE)

endfunction()
