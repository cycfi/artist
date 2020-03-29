###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
project(examples)

function(add_example name)
   if (APPLE)
      add_executable(
         ${name} MACOSX_BUNDLE
         ${name}.cpp
         host/macos/app.mm
         ${${name}_fonts}
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
         ${${name}_fonts}
         ${${name}_images}
         PROPERTIES
         MACOSX_PACKAGE_LOCATION Resources
      )
   endif()

   target_link_libraries(${name} infra artist)
endfunction()

set(typography_fonts
   ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/OpenSansCondensed-Light.ttf
   ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/OpenSansCondensed-LightItalic.ttf
)

set(space_fonts
   ${CMAKE_CURRENT_LIST_DIR}/images/space.jpg
)

add_example(shapes)
add_example(typography)
add_example(rain)
add_example(tauri)
add_example(space)


