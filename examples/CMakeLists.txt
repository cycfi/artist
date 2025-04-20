###############################################################################
#  Copyright (c) 2016-2023 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
project(examples)

if (UNIX AND NOT APPLE)
   find_package(OpenGL REQUIRED COMPONENTS OpenGL)
   find_package(PkgConfig REQUIRED)
   
   option(X_Win "Use x11 window" ON)

  set(SRC host/linux/skia_app.cpp
          host/linux/skia_context.cpp)

  if (X_Win)
    pkg_check_modules(depend REQUIRED IMPORTED_TARGET
                      x11 glx
                     )
    set(SRC ${SRC}
      host/linux/x11/application.cpp
    )

  else()
    pkg_check_modules(depend REQUIRED IMPORTED_TARGET
                        wayland-client wayland-cursor wayland-egl xkbcommon egl
                      )

    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(WaylandScanner REQUIRED)

    set(WORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/host/linux/wayland")

    wayland_add_protocol_client(SRC_GEN ${WORK_DIR}
      "${WaylandScanner_DATADIR}/stable/xdg-shell/xdg-shell.xml")
    wayland_add_protocol_client(SRC_GEN ${WORK_DIR}
      "${WaylandScanner_DATADIR}/unstable/tablet/tablet-unstable-v2.xml")
    wayland_add_protocol_client(SRC_GEN ${WORK_DIR}
      "${WaylandScanner_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml")
    wayland_add_protocol_client(SRC_GEN ${WORK_DIR}
      "${WaylandScanner_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml")
    wayland_add_protocol_client(SRC_GEN ${WORK_DIR}
      "${WaylandScanner_DATADIR}/stable/viewporter/viewporter.xml")

    set(SRC ${SRC} ${SRC_GEN}
      host/linux/wayland/core.cpp
      host/linux/wayland/display.cpp
      host/linux/wayland/xdgshell.cpp
      host/linux/wayland/contextegl.cpp
      host/linux/wayland/application.cpp
    )
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

      set_source_files_properties(
         ${artist_fonts}
         ${${name}_images}
         PROPERTIES
         MACOSX_PACKAGE_LOCATION Resources
      )

   elseif (MSVC)

      add_executable(
         ${name}
         ${name}.cpp
         host/windows/skia_app.cpp
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

   elseif (UNIX)
        
      add_executable(
         ${name}
         ${name}.cpp
         ${SRC}
         print_elapsed.cpp
      )

      target_link_libraries(
         ${name}
         PkgConfig::depend
         OpenGL::GL
      )

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
add_example(rain)
add_example(tauri)
add_example(space)
add_example(composite_ops)
add_example(paths)
add_example(shadow)
add_example(chessboard)

