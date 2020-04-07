###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
project(artist_test)

if (APPLE)

   add_executable(
      artist_test MACOSX_BUNDLE
      main.cpp
   )

   if (ARTIST_QUARTZ_2D)
      file(
         GLOB ARTIST_FONTS ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/*.*
      )
      set_target_properties(
         artist_test
         PROPERTIES
         MACOSX_BUNDLE_INFO_PLIST
         ${CMAKE_CURRENT_LIST_DIR}/../resources/macos/plist.in
      )
      set_source_files_properties(
         ${ARTIST_FONTS}
         PROPERTIES
         MACOSX_PACKAGE_LOCATION Resources
      )
   endif()

   target_link_options(artist_test PRIVATE -framework AppKit)

elseif (MSVC)

   add_executable(
      artist_test
      main.cpp
   )
   set_property(TARGET artist_test PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded"
   )

elseif (UNIX)

   add_executable(
      artist_test
      main.cpp
   )

endif()

file(
   COPY ../resources/fonts
   DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources
)

target_link_libraries(artist_test artist)

if (APPLE)
   if (ARTIST_SKIA)
      set(GOLDEN_PATH macos_golden/skia)
   elseif (ARTIST_QUARTZ_2D)
      set(GOLDEN_PATH macos_golden/quartz2d)
   endif()

   set(GOLDEN_DIR macos_golden)

   # Copy golden files to the binary dir
   file(
      COPY macos_golden
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
   )

elseif (MSVC)
   if (ARTIST_SKIA)
      set(GOLDEN_PATH windows_golden/skia)
   endif()

   set(GOLDEN_DIR windows_golden)

   # Copy golden files to the binary dir
   file(
      COPY windows_golden
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
   )

elseif (UNIX)
   if (ARTIST_SKIA)
      set(GOLDEN_PATH linux_golden/skia)
   endif()

   set(GOLDEN_DIR linux_golden)

   # Copy golden files to the binary dir
   file(
      COPY linux_golden
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
   )

endif()

# Copy results folder to the binary dir
file(
  COPY results
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

# Copy images folder to the binary dir
file(
  COPY images
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
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

add_definitions(
   -DGOLDEN_PATH="${CMAKE_CURRENT_BINARY_DIR}/${GOLDEN_PATH}/"
   -DRESULTS_PATH="${CMAKE_CURRENT_BINARY_DIR}/results/"
   -DIMAGES_PATH="${CMAKE_CURRENT_BINARY_DIR}/images/"
   -DFONTS_PATH="${CMAKE_CURRENT_BINARY_DIR}/resources/fonts/"
)

