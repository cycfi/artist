###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################

project(canvas_test)

set(CANVAS_TEST_RESOURCES
   ${CMAKE_CURRENT_LIST_DIR}/images/logo.png
)

###############################################################################
# The executable App

if (APPLE)
   set(CANVAS_HOST
      host/macos/app.mm
   )
elseif (UNIX)
   set(CANVAS_HOST
   )
elseif (WIN32)
   set(CANVAS_HOST
   )
endif()

add_executable(
   canvas_test MACOSX_BUNDLE
   main.cpp
   ${CANVAS_TEST_RESOURCES}
   ${CANVAS_HOST}
   )

set_target_properties(
   canvas_test
   PROPERTIES
   MACOSX_BUNDLE_INFO_PLIST
   ${CMAKE_CURRENT_LIST_DIR}/plist.in
)

if (APPLE)
   target_link_options(canvas_test PRIVATE -framework AppKit)
endif()

if (APPLE)
   set_source_files_properties(
      ${CANVAS_TEST_RESOURCES}
      PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources
   )
endif()

target_link_libraries(canvas_test infra canvas)

