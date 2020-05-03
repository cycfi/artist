# Setup and Installation

## Table of Contents
* [Requirements](#requirements)
* [MacOS Installation](#macos)
* [Windows Installation](#windows)
* [Linux Installation](#linux)
* [Building and Running the examples](#building-and-running-the-examples)

-------------------------------------------------------------------------------

## Requirements

Here are the basic requirements and dependencies that you need to satisfy in
order to use the library:

1. A C++17 compiler
2. Git
3. [CMake](https://cmake.org/) 3.9.6 or higher
4. [GTK3](https://www.gtk.org/) (See note 4)

On the MacOS, if the backend is Quartz-2D (default build) there are no
additional dependencies.

If the backend is Skia, using the cmake option `ARTIST_SKIA=ON`, there are
additional requirements and dependencies:

1. [Skia](https://skia.org/) (See note 1)
2. [HarfBuzz](https://www.freedesktop.org/wiki/Software/HarfBuzz/) (See note 2)
3. [libunibreak](https://github.com/adah1972/libunibreak) (See note 2)
4. [OpenGL](https://www.opengl.org/) (See note 3)
5. [fontconfig](https://www.freedesktop.org/wiki/Software/fontconfig/) (See note 2)

#### Notes:
1. No setup required. Platform specific binary automatically downloaded by
   cmake.
2. On Linux, this library is a required dependency that needs to be installed
   via `apt-get`. Otherwise, no setup required. Platform specific binary
   automatically downloaded by cmake.
3. This library is a required dependency that needs to be installed.
4. Linux only. This library is a required dependency that needs to be
   installed.

Additionally, the following library is dragged as a submodule:

1. The [Cycfi infra library](https://github.com/cycfi/infra/)

Infra provides some basic groundwork common to Cycfi libraries, including
Artist.

### C++17

Artist currently supports the MacOS, Windows and Linux. In the Mac, we
support both [XCode](https://developer.apple.com/xcode/) and
[CLion](https://www.jetbrains.com/clion/) IDEs. Artist is tested with XCode
10 and XCode 11. In Windows, we support Windows 10 with [Visual Studio
2019](https://visualstudio.microsoft.com/vs/), although it will probably also
work with older versions of the Visual Studio IDE. In Linux, we support both
[Clang](https://clang.llvm.org/) and [g++](https://gcc.gnu.org/) Get the
latest version with a C++17 compiler.

### Git

Artist 2D Canvas Library, plus the Cycfi Infra library:

```
git clone --recurse-submodules https://github.com/cycfi/artist.git
```

### CMake

Make sure you have [CMake](https://cmake.org/) 3.7.2 or higher. Follow the
installation procedure for your platform, or follow the instructions below
for specific environments.

-------------------------------------------------------------------------------

## MacOS

### **Optional**. If the desired backend is Skia, using the cmake option `-DARTIST_SKIA=ON`, install required libraries using [Homebrew](https://brew.sh/):

```
brew install fontconfig
```

The MacOS port is configured to have cmake download Skia, precompiled, as
needed, so there is no need to have the library manually installed. CMake
will take care of downloading and setting up dependencies.

### Install CMake

```
brew install cmake
```

### Generating the Project using CMake

There are multiple ways to generate a project file using CMake depending on
your platform and desired IDE, but here are some examples for the MacOS:

### Using [XCode](https://developer.apple.com/xcode/):

1. CD to the artist library.
2. Make a build directory inside the artist directory.
3. CD to the build directory.
4. invoke cmake.

```
cd artist
mkdir build
cd build
cmake -GXcode ../
```

**Optional**. If the desired backend is Skia, use the cmake option `-DARTIST_SKIA=ON`:

```
cmake -GXcode -DARTIST_SKIA=ON ../
```

By default, the backend on MacOS is Quartz-2D.

If successful, cmake will generate an XCode project in the build directory.
Open the project file artist.xcodeproj and build all. You should see a couple
of example applications.

### Using [CLion](https://www.jetbrains.com/clion/):

Simply open the CMakeLists.txt file using CLion and build the project.

**Optional**. If the desired backend is Skia, use the cmake option
`ARTIST_SKIA=ON`. This option can be set in Preferences->Build, Execution,
Deployment->CMake->CMake Options.

-------------------------------------------------------------------------------

## Windows

### Install required libraries

The Windows port is configured to have cmake download all the required
libraries, precompiled, so there is no need to have these libraries manually
installed. CMake will take care of downloading and setting up dependencies.

### Install CMake

Follow the instructions provided here: https://cmake.org/install/

### Generating the Project using CMake

Assuming you have [Visual Studio
2019](https://visualstudio.microsoft.com/vs/) installed.

#### Visual Studio 2019 GUI

1. CD to the artist library.
2. Make a build directory inside the artist directory.
3. CD to the build directory.
4. invoke cmake.

```
cd artist
mkdir build
cd build
cmake -G"Visual Studio 16 2019" ..//
```

If successful, cmake will generate a Visual Studio solution in the build
directory. Open the project file artist.sln and build all (or invoke
`nmake`). You should see a couple of example applications.

#### NMake

If you prefer to use the command-line, you may use the `-G"NMake Makefiles"` generator,
instead.

1. Open a *Command Prompt for VS 2019* ({x64/x86-64} {Native/Cross} Tools Command Prompt for VS 2019) in your start menu.
2. CD to the artist library.
3. Make a build directory inside the artist directory.
4. CD to the build directory.
5. invoke cmake.

```
cd artist
mkdir build
cd build
cmake -G"NMake Makefiles" ..//
```

If successful, cmake will generate NMake Make files in the build directory.
Invoke `nmake` to build the binary.

-------------------------------------------------------------------------------

## Linux

### Install required libraries using [apt-get](https://linux.die.net/man/8/apt-get) (requires `sudo`).

```
sudo apt-get install libharfbuzz-dev
sudo apt-get install libunibreak-dev
sudo apt-get install fontconfig
sudo apt-get install libgtkglext1-dev
sudo apt-get install libgtk-3-dev
```

The Linux port is configured to have cmake download Skia, precompiled, as
needed, so there is no need to have the library manually installed. CMake
will take care of downloading and setting up dependencies.

### Install CMake

```
sudo apt-get -y install cmake
```

### Generating the Project using CMake

There are multiple ways to generate a project file using CMake depending on
your platform and desired IDE, but here are some examples for Linux:

### Using UNIX makefiles:

1. CD to the artist library.
2. Make a build directory inside the artist directory.
3. CD to the build directory.
4. invoke cmake.

```
cd artist
mkdir build
cd build
cmake -G "Unix Makefiles" ../
```

If successful, cmake will generate Unix Make files in the build directory.

### Using [CLion](https://www.jetbrains.com/clion/):

Simply open the CMakeLists.txt file using CLion and build the project.

-------------------------------------------------------------------------------

## Building and Running the examples

If successful, cmake will generate a project file or makefiles in the build
directory. Build the library and example programs using the generated
makefiles or open the project file using your IDE and build all.

You should see a couple of example applications in there that you can run.
These examples are provided as starting points to get you up to speed in
learning how to use the library. For simplicity, each example is contained in
a single source file, plus specific platform-specific drivers for setting up
the library on different OSes for the backend 2D graphics library. These
drivers are single .cpp file (or .mm file for MacOS) that contains the
minimal code for setting up the application and a single window for viewing
the graphics.

Feel free to inspect and mess with the examples. Each example demonstrates
different aspects of the Artist library.

-------------------------------------------------------------------------------

*Copyright (c) 2014-2020 Joel de Guzman. All rights reserved.*
*Distributed under the [MIT License](https://opensource.org/licenses/MIT)*
