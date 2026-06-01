# Linux Development Environment

For working on the Linux Skia backend from macOS — building, running tests,
and running GUI example apps with a live Wayland display.

---

## OrbStack

[OrbStack](https://orbstack.dev) runs lightweight Linux VMs on macOS.
GTK/Wayland windows appear as native macOS windows with zero extra
configuration — no XQuartz, no DISPLAY variable fiddling.

### 1. Install OrbStack

```sh
brew install orbstack
```

Or download from [orbstack.dev](https://orbstack.dev).

### 2. Create a Linux VM

```sh
orb create ubuntu:24.04 artist-dev
```

### 3. Mount the project and run setup

OrbStack automatically mounts your macOS home directory inside the VM at
`/Users/<you>`. Navigate to the project and run the setup script:

```sh
orb shell artist-dev
# now inside the VM:
cd /Users/<you>/dev/cycfi/artist
bash .devcontainer/setup.sh
```

Setup installs all apt packages and bootstraps vcpkg (~2 min).
The vcpkg Skia build runs on the first cmake configure (~15 min), then
the binary cache makes subsequent configures fast.

### 4. Build and test

```sh
# still inside the VM shell:
bash .devcontainer/build-skia.sh            # build + tests + examples
bash .devcontainer/build-skia.sh --no-examples   # headless only (faster)
bash .devcontainer/build-cairo.sh
```

### 5. Run example apps

```sh
./build-skia-linux/examples/shapes/shapes
./build-skia-linux/examples/typography/typography
```

The window appears on your macOS desktop via Wayland — same as any native
macOS window.

---

## VS Code + OrbStack

VS Code's **Remote - SSH** extension connects directly to OrbStack VMs.
You get full IntelliSense, debugging, and the integrated terminal — all
running natively inside the Linux VM while you edit on macOS.

### 1. Install the Remote - SSH extension

In VS Code: **Extensions → search "Remote - SSH" → Install**
(`ms-vscode-remote.remote-ssh`)

### 2. Add the OrbStack VM as an SSH host

Open the Command Palette (`⇧⌘P`) → **Remote-SSH: Open SSH Configuration File**
→ add:

```
Host artist-dev
    HostName artist-dev.orb.local
    User <your-username>
    IdentityFile ~/.orbstack/ssh/id_ed25519
```

Or let VS Code discover it automatically:
**Remote-SSH: Connect to Host…** → type `<your-username>@artist-dev.orb.local`

OrbStack adds its SSH key automatically — no password needed.

### 3. Open the project

Once connected, **File → Open Folder** → navigate to your project path
inside the VM (same path as on macOS, e.g. `/Users/<you>/dev/cycfi/artist`).

### 4. Recommended extensions (install on the remote)

VS Code will prompt to install workspace extensions on the remote host.
Useful ones:

- `ms-vscode.cpptools` — C/C++ IntelliSense and debugging
- `ms-vscode.cmake-tools` — CMake integration
- `twxs.cmake` — CMake syntax highlighting

### 5. Configure CMake Tools

In the VS Code CMake Tools kit selector, choose **GCC** (the system
compiler inside the VM). Set the CMake configure args in
`.vscode/settings.json` (create if it doesn't exist):

```json
{
  "cmake.configureArgs": [
    "-DARTIST_SKIA=ON",
    "-DARTIST_BUILD_EXAMPLES=ON",
    "-DARTIST_BUILD_TESTS=ON",
    "-DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake"
  ],
  "cmake.buildDirectory": "${workspaceFolder}/build-skia-linux"
}
```

Then **CMake: Configure** and **CMake: Build** from the Command Palette.

---

## Notes

- **vcpkg Skia build is slow the first time** (~15 min). OrbStack's VM
  disk persists between sessions, so the binary cache is preserved and
  subsequent configures are fast.
- The Linux Skia host (`examples/host/linux/skia_app.cpp`) uses
  `GtkGLArea` which automatically picks EGL/Wayland or GLX/X11 depending
  on the runtime environment.
- Cairo examples use system apt packages — no vcpkg needed, builds
  instantly.
- The macOS project files are directly accessible inside the OrbStack VM
  (mounted at `/Users/<you>/...`), so you edit the same files from both
  macOS and the VM — no sync needed.
