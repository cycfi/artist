# Remote Development Setup

---

## Linux — OrbStack

[OrbStack](https://orbstack.dev) runs lightweight Linux VMs on macOS. It is
great for building and running the headless test suite, but note: OrbStack
does **not** provide a built-in display server for Linux GUI apps. It bridges
CLI tools to macOS (`open`, `screencapture`, `pbcopy`, `osascript`) but a
GTK app run in the VM fails with `cannot open display`.

To see GUI examples on macOS you need an X11 server:

```sh
brew install --cask xquartz   # then log out/in once
# connect with X forwarding:
ssh -X <user>@artist-dev.orb.local
# GTK falls back to X11 automatically (software GL — fine for the demos)
```

Headless build + test needs no display at all.

### 1. Install OrbStack

```sh
brew install orbstack
```

### 2. Create a Linux VM

```sh
orb create ubuntu:24.04 artist-dev
```

### 3. Install dependencies

OrbStack mounts your macOS home at `/Users/<you>/` inside the VM — same
files, no sync needed.

```sh
orb shell artist-dev

# Inside the VM:
sudo apt-get update && sudo apt-get install -y \
    cmake ninja-build build-essential pkg-config git curl zip unzip tar \
    python3 gperf autoconf autoconf-archive automake libtool \
    libgtk-3-dev libgtkglext1-dev \
    libgl1-mesa-dev libglu1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libepoxy-dev \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libx11-dev libxrandr-dev libxi-dev \
    libharfbuzz-dev libfontconfig1-dev libfreetype6-dev \
    libcairo2-dev

# Bootstrap vcpkg (one-time)
cd /Users/<you>/dev/cycfi/artist
lib/external/vcpkg/bootstrap-vcpkg.sh -disableMetrics
```

`autoconf autoconf-archive automake libtool` are needed by the vcpkg
`gperf` port (a transitive Skia dependency on Linux).

### 4. Build and test

OrbStack VMs on Apple Silicon are arm64, so vcpkg must target `arm64-linux`.
vcpkg also needs the system `gl.pc` exposed via `PKG_CONFIG_PATH` (it
sanitizes the environment, so pass it through):

```sh
# Skia (first configure builds Skia via vcpkg ~15 min; cached after that)
PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
cmake -S . -B build-skia-linux \
    -DARTIST_SKIA=ON \
    -DARTIST_BUILD_EXAMPLES=ON \
    -DARTIST_BUILD_TESTS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=arm64-linux \
    -DVCPKG_ENV_PASSTHROUGH_UNTRACKED=PKG_CONFIG_PATH \
    -G Ninja
cmake --build build-skia-linux
ctest --verbose --test-dir build-skia-linux

# Cairo (uses system packages — fast)
cmake -S . -B build-cairo-linux \
    -DARTIST_CAIRO=ON \
    -DARTIST_BUILD_EXAMPLES=ON \
    -DARTIST_BUILD_TESTS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja
cmake --build build-cairo-linux
ctest --verbose --test-dir build-cairo-linux
```

### 5. Run example apps

```sh
./build-skia-linux/examples/shapes/shapes
```

Windows appear on your macOS desktop via Wayland.

---

## Windows — Remote Machine

### 1. Enable OpenSSH on the Windows machine

`Settings → System → Optional Features → Add a feature → OpenSSH Server`

Then start the service:

```powershell
Start-Service sshd
Set-Service -Name sshd -StartupType Automatic
```

### 2. Connect from macOS

```sh
ssh <user>@<windows-ip>
```

Copy your SSH public key for passwordless login:

```sh
ssh-copy-id <user>@<windows-ip>
```

### 3. Install dependencies on Windows

Run in a **Developer Command Prompt** (or PowerShell with MSVC in PATH):

```bat
# vcpkg bootstrap (one-time)
lib\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

Install ninja via winget or choco:
```bat
winget install Ninja-build.Ninja
```

### 4. Build and test

Run in a **Developer Command Prompt for VS 2022**:

```bat
cmake -S . -B build-skia-win ^
    -DARTIST_SKIA=ON ^
    -DARTIST_BUILD_EXAMPLES=ON ^
    -DARTIST_BUILD_TESTS=ON ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -G Ninja
cmake --build build-skia-win
ctest --verbose --test-dir build-skia-win
```

### 5. Run example apps

```bat
build-skia-win\examples\shapes\shapes.exe
```

---

## Notes

- **vcpkg Skia build is slow the first time** (~15 min on Linux, longer on
  Windows). The binary cache persists on disk so subsequent builds are fast.
- On Linux, `GtkGLArea` automatically picks EGL/Wayland or GLX/X11 — no
  manual backend selection needed.
- Cairo on Linux uses system apt packages — no vcpkg, builds instantly.
- OrbStack mounts the macOS home at `/Users/<you>/` — edit files on macOS,
  build in the VM, no sync needed.
