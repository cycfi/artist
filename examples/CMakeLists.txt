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

   if (MSVC)
      add_executable(
         ${name}
         ${name}.cpp
         host/windows/app.cpp
      )
      set_property(TARGET ${name} PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreaded"
      )
      file(
         COPY ${${name}_fonts}
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources/fonts
      )
      file(
         COPY ${${name}_images}
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources/images
      )
      target_link_options(${name} PRIVATE
         /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup shcore.lib
      )
   endif()

   target_link_libraries(${name} artist)

endfunction()

file(
   GLOB typography_fonts ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/*.*
)

set(space_images
   ${CMAKE_CURRENT_LIST_DIR}/images/space.jpg
)

set(composite_ops_images
   ${CMAKE_CURRENT_LIST_DIR}/images/dest.png
   ${CMAKE_CURRENT_LIST_DIR}/images/src.png
)

if (MSVC)
   set(FONTCONFIG_DLL ${CMAKE_CURRENT_BINARY_DIR}/../lib/windows-x64/src/prebuilt_binaries/fontconfig.dll)
   file(COPY ${FONTCONFIG_DLL} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

   set(ICONV_DLL ${CMAKE_CURRENT_BINARY_DIR}/../lib/windows-x64/src/prebuilt_binaries/libiconv.dll)
   file(COPY ${ICONV_DLL} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

   set(XML2 ${CMAKE_CURRENT_BINARY_DIR}/../lib/windows-x64/src/prebuilt_binaries/libxml2.dll)
   file(COPY ${XML2} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

   set(FREETYPE_DLL ${CMAKE_CURRENT_BINARY_DIR}/../lib/windows-x64/src/prebuilt_binaries/freetype.dll)
   file(COPY ${FREETYPE_DLL} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

#add_example(shapes)
#add_example(typography)
#add_example(rain)
#add_example(tauri)
#add_example(space)
#add_example(composite_ops)


