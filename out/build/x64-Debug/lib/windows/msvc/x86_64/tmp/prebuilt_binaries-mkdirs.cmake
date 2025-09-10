# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src/prebuilt_binaries")
  file(MAKE_DIRECTORY "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src/prebuilt_binaries")
endif()
file(MAKE_DIRECTORY
  "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src/prebuilt_binaries-build"
  "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64"
  "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/tmp"
  "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src/prebuilt_binaries-stamp"
  "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src"
  "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src/prebuilt_binaries-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src/prebuilt_binaries-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/source/repos/silovaa/artist/out/build/x64-Debug/lib/windows/msvc/x86_64/src/prebuilt_binaries-stamp${cfgdir}") # cfgdir has leading slash
endif()
