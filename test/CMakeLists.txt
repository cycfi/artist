###############################################################################
# SPDX-FileCopyrightText:  2016-2024 Joel de Guzman
# SPDX-License-Identifier: MIT
###############################################################################
project(artist_test)

file(
   GLOB artist_fonts ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/*.*
)

if (APPLE)

   if (ARTIST_QUARTZ_2D)
      add_executable(
         artist_test MACOSX_BUNDLE
         main.cpp
         detail/osx_init.mm
         ${artist_fonts}
      )
   else()
      add_executable(
         artist_test MACOSX_BUNDLE
         main.cpp
         ${artist_fonts}
      )
   endif()

   if (ARTIST_QUARTZ_2D)
      file(
         GLOB ARTIST_FONTS ${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/*.*
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
   if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
      set_property(TARGET artist_test PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreaded"
      )
   else()
      set_property(TARGET artist_test PROPERTY
         MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
      )
   endif()

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

add_definitions(
   -DGOLDEN_PATH="${CMAKE_CURRENT_BINARY_DIR}/${GOLDEN_PATH}/"
   -DRESULTS_PATH="${CMAKE_CURRENT_BINARY_DIR}/results/"
   -DIMAGES_PATH="${CMAKE_CURRENT_BINARY_DIR}/images/"
   -DFONTS_PATH="${CMAKE_CURRENT_BINARY_DIR}/resources/fonts/"
)

include(CTest)
add_test(NAME artist_test COMMAND artist_test)

