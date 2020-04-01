###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################

project(artist_test)

set(ARTIST_TEST_RESOURCES
   ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/OpenSansCondensed-Light.ttf
   ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/OpenSansCondensed-LightItalic.ttf
)

###############################################################################
# The executable App

if (APPLE)
   add_executable(
      artist_test MACOSX_BUNDLE
      main.cpp
      ${ARTIST_TEST_RESOURCES}
   )
   set_target_properties(
      artist_test
      PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST
      ${CMAKE_CURRENT_LIST_DIR}/../resources/macos/plist.in
      )
elseif (MSVC)
   add_executable(
      artist_test
      main.cpp
      ${ARTIST_TEST_RESOURCES}
   )
endif()

if (APPLE)
   target_link_options(artist_test PRIVATE -framework AppKit)
endif()

if (APPLE)
   set_source_files_properties(
      ${ARTIST_TEST_RESOURCES}
      PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources
   )
endif()

target_link_libraries(artist_test artist)

if (WIN32)
   set_property(TARGET artist_test PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded"
   )
endif()

if (ARTIST_SKIA)
   set(GOLDEN_PATH golden/skia)
elseif (ARTIST_QUARTZ_2D)
   set(GOLDEN_PATH golden/quartz2d)
endif()

# Copy golden files to the binary dir
file(
  COPY golden
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Copy results folder to the binary dir
file(
  COPY results
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Copy images folder to the binary dir
file(
  COPY images
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_definitions(
   -DGOLDEN_PATH="${CMAKE_CURRENT_BINARY_DIR}/${GOLDEN_PATH}/"
   -DRESULTS_PATH="${CMAKE_CURRENT_BINARY_DIR}/results/"
   -DIMAGES_PATH="${CMAKE_CURRENT_BINARY_DIR}/images/"
)

