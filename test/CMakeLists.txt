###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################
project(artist_test)

if (APPLE)
   file(
      GLOB ARTIST_FONTS ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/*.*
   )
   add_executable(
      artist_test MACOSX_BUNDLE
      main.cpp
      ${ARTIST_FONTS}
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
   target_link_options(artist_test PRIVATE -framework AppKit)
endif()

if (MSVC)
   add_executable(
      artist_test
      main.cpp
   )
   set_property(TARGET artist_test PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded"
   )
   file(
      COPY ../resources/fonts
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources
   )
endif()

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
endif()

if (MSVC)
   if (ARTIST_SKIA)
      set(GOLDEN_PATH windows_golden/skia)
   endif()

   set(GOLDEN_DIR windows_golden)

   # Copy golden files to the binary dir
   file(
   COPY windows_golden
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

add_definitions(
   -DGOLDEN_PATH="${CMAKE_CURRENT_BINARY_DIR}/${GOLDEN_PATH}/"
   -DRESULTS_PATH="${CMAKE_CURRENT_BINARY_DIR}/results/"
   -DIMAGES_PATH="${CMAKE_CURRENT_BINARY_DIR}/images/"
)

