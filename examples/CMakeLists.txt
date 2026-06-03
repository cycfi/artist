###############################################################################
#  Copyright (c) 2016-2023 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
project(examples)

if (UNIX AND NOT APPLE)
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(wayland_client REQUIRED IMPORTED_TARGET wayland-client)
   pkg_check_modules(cairo_pkg      REQUIRED IMPORTED_TARGET cairo)

   # Locate wayland-scanner and the protocol XML directory
   find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
   pkg_get_variable(WL_PROTOCOLS_DIR wayland-protocols pkgdatadir)

   # Generate xdg-shell client header + private code into the build tree
   set(WL_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland-gen")
   file(MAKE_DIRECTORY "${WL_GEN_DIR}")

   set(XDG_SHELL_XML    "${WL_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
   set(XDG_SHELL_HEADER "${WL_GEN_DIR}/xdg-shell-client-protocol.h")
   set(XDG_SHELL_SRC    "${WL_GEN_DIR}/xdg-shell-protocol.c")

   add_custom_command(
      OUTPUT  "${XDG_SHELL_HEADER}"
      COMMAND "${WAYLAND_SCANNER}" client-header "${XDG_SHELL_XML}" "${XDG_SHELL_HEADER}"
      DEPENDS "${XDG_SHELL_XML}"
      COMMENT "Generating xdg-shell client header"
   )
   add_custom_command(
      OUTPUT  "${XDG_SHELL_SRC}"
      COMMAND "${WAYLAND_SCANNER}" private-code "${XDG_SHELL_XML}" "${XDG_SHELL_SRC}"
      DEPENDS "${XDG_SHELL_XML}"
      COMMENT "Generating xdg-shell protocol code"
   )

   # Small static lib so the generated files are built once and shared
   add_library(wayland_xdg_shell STATIC "${XDG_SHELL_SRC}" "${XDG_SHELL_HEADER}")
   target_include_directories(wayland_xdg_shell PUBLIC "${WL_GEN_DIR}")
   target_link_libraries(wayland_xdg_shell PUBLIC PkgConfig::wayland_client)

   if (NOT ARTIST_CAIRO)
      find_package(OpenGL REQUIRED COMPONENTS OpenGL)
      pkg_check_modules(wayland_egl REQUIRED IMPORTED_TARGET wayland-egl)
      pkg_check_modules(egl_pkg     REQUIRED IMPORTED_TARGET egl)
   endif()
endif()

file(
   GLOB artist_fonts ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/*.*
)

function(add_example name)
   if (APPLE)

      if (ARTIST_SKIA)
         set(MAIN_APP
            host/macos/skia_app.mm
            print_elapsed.cpp
         )
      elseif (ARTIST_CAIRO)
         set(MAIN_APP
            host/macos/cairo_app.mm
            print_elapsed.cpp
         )
      else()
         set(MAIN_APP
            host/macos/quartz2d_app.mm
            print_elapsed.cpp
         )
      endif()

      add_executable(
         ${name} MACOSX_BUNDLE
         ${name}.cpp
         ${MAIN_APP}
         ${artist_fonts}
         ${${name}_images}
      )

      set_target_properties(
         ${name}
         PROPERTIES
         MACOSX_BUNDLE_INFO_PLIST
         ${CMAKE_CURRENT_LIST_DIR}/../resources/macos/plist.in
      )

      target_link_options(${name} PRIVATE -framework AppKit)
      if (ARTIST_SKIA)
         target_link_libraries(${name}
            "-framework Metal"
            "-framework QuartzCore"
         )
      endif()

      set_source_files_properties(
         ${artist_fonts}
         ${${name}_images}
         PROPERTIES
         MACOSX_PACKAGE_LOCATION Resources
      )

   elseif (MSVC)

      if (ARTIST_CAIRO)
         set(WINDOWS_HOST host/windows/cairo_app.cpp)
      else()
         set(WINDOWS_HOST host/windows/skia_app.cpp)
      endif()

      add_executable(
         ${name}
         ${name}.cpp
         ${WINDOWS_HOST}
         print_elapsed.cpp
      )

      if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
         set_property(TARGET ${name} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded"
         )
      else()
         set_property(TARGET ${name} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
         )
      endif()

      target_link_options(${name} PRIVATE
         /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup shcore.lib
      )

      # The Skia host uses WGL + OpenGL directly (wgl*/gl* in opengl32,
      # ChoosePixelFormat/SetPixelFormat/SwapBuffers in gdi32). The old host
      # got these transitively via the now-removed sk_app, so link explicitly.
      if (NOT ARTIST_CAIRO)
         find_package(OpenGL REQUIRED)
         target_link_libraries(${name} OpenGL::GL gdi32)
      endif()

   elseif (UNIX)

      if (ARTIST_CAIRO)
         set(LINUX_HOST host/linux/cairo_app.cpp)
      else()
         set(LINUX_HOST host/linux/skia_app.cpp)
      endif()

      add_executable(
         ${name}
         ${name}.cpp
         ${LINUX_HOST}
         print_elapsed.cpp
      )

      target_include_directories(${name} PRIVATE "${WL_GEN_DIR}")

      if (ARTIST_CAIRO)
         target_link_libraries(
            ${name}
            wayland_xdg_shell
            PkgConfig::cairo_pkg
         )
      else()
         target_link_libraries(
            ${name}
            wayland_xdg_shell
            PkgConfig::wayland_egl
            PkgConfig::egl_pkg
            OpenGL::GL
         )
      endif()

   endif()

   if (UNIX OR MSVC)

      file(
         COPY ${artist_fonts}
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources/fonts
      )

      file(
         COPY ${${name}_images}
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources/images
      )

   endif()

   target_link_libraries(${name} artist)

endfunction()

set(space_images
   ${CMAKE_CURRENT_LIST_DIR}/images/space.jpg
)

set(composite_ops_images
   ${CMAKE_CURRENT_LIST_DIR}/images/dest.png
   ${CMAKE_CURRENT_LIST_DIR}/images/src.png
)

add_example(shapes)
add_example(typography)
add_example(typo_perf)
add_example(rain)
add_example(tauri)
add_example(space)
add_example(composite_ops)
add_example(paths)
add_example(shadow)

