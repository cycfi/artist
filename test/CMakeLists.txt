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

target_link_libraries(artist_test infra artist)

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
   -DGOLDEN_PATH="${CMAKE_CURRENT_BINARY_DIR}/golden/"
   -DRESULTS_PATH="${CMAKE_CURRENT_BINARY_DIR}/results/"
   -DIMAGES_PATH="${CMAKE_CURRENT_BINARY_DIR}/images/"
)

