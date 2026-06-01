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

## Linux — VMware Fusion (GUI / real GPU stack)

OrbStack is great for headless build+test, but it has **no display server**, so
GTK windows can't open (only X11 forwarding, which is laggy). When you need to
actually *see* and interact with the example apps on a real Wayland/X11
compositor, a full VM in **VMware Fusion** is the better tool.

### 1. Create the VM

- Install VMware Fusion (free for personal use) and create an **Ubuntu** VM.
- **Important (Apple Silicon / arm64):** use **Ubuntu 25.10 or newer**
  (26.04 LTS confirmed working). Ubuntu 24.04 arm64 ships **harfbuzz 8.3.0**,
  which has a bug that **crashes any GTK app** on widget realize (even a plain
  hello-world) — not an Artist bug. harfbuzz ≥ 10.2 (25.10) / 12.x (26.04)
  fixes it. If you already installed 24.04, `sudo do-release-upgrade` works
  (set `Prompt=normal` in `/etc/update-manager/release-upgrades` for non-LTS).

### 2. GPU expectations

VMware Fusion on Apple Silicon gives you a **software** GL stack (Mesa
`llvmpipe`), reported as OpenGL 4.x — which is plenty for Skia Ganesh GL. You'll
see harmless startup warnings like:

```
libEGL warning: failed to get driver name for fd -1
MESA: error: ZINK: failed to choose pdev
```

These are just Zink (Vulkan-on-GL) failing and falling back to llvmpipe. The
window still renders correctly (the `typo` demo hits ~3000 fps on llvmpipe).
Check the renderer with `glxinfo | grep "OpenGL renderer"` (`sudo apt install
mesa-utils`).

### 3. Dependencies (Ubuntu 26.04, arm64)

```sh
sudo apt install -y \
    cmake ninja-build g++ git curl zip unzip tar pkg-config \
    autoconf autoconf-archive automake libtool \
    libgtk-3-dev libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev \
    libharfbuzz-dev libfontconfig1-dev libfreetype6-dev \
    x11proto-dev libx11-dev libxext-dev shared-mime-info
```

Notes vs. OrbStack list: `libgtkglext1-dev` was **dropped in 26.04** and is not
needed (Skia uses `GtkGLArea`, built into GTK3). `x11proto-dev libx11-dev
libxext-dev shared-mime-info` are needed for GTK's pkg-config to resolve.

### 4. Build (arm64, vcpkg)

vcpkg's toolchain overwrites `PKG_CONFIG_PATH`, hiding system GTK from cmake.
Pass it through, and note ninja is `/usr/bin/ninja` on Ubuntu (not
`ninja-build`):

```sh
PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig cmake -S . -B build-skia \
    -DARTIST_SKIA=ON -DARTIST_BUILD_EXAMPLES=ON -DARTIST_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=arm64-linux \
    -DVCPKG_ENV_PASSTHROUGH_UNTRACKED=PKG_CONFIG_PATH \
    -G Ninja -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja
cmake --build build-skia
```

Run the apps from a terminal **inside the VM's desktop session** (not over SSH —
SSH has no `$DISPLAY`/X11 auth):

```sh
./build-skia/examples/shapes
```

---

## Driving a VM/remote box with the agent (over SSH, using your keys)

This is the workflow used for the Linux VM and the Windows box in the recent
port work. **Key point:** the agent cannot open its own SSH connection — it has
no identity, keys, or network of its own. It operates entirely through the
**`bash` tool on your Mac**, which uses **your** SSH client and **your** keys.
So "let the agent drive the VM" really means: the agent runs `ssh you@vm ...`
from your Mac, authenticated by your existing key.

### 1. Give your Mac key passwordless access to the guest

On the **guest** (Linux VM or Windows), append your Mac's public key:

```sh
# on the Mac, show your public key:
cat ~/.ssh/id_rsa.pub        # or id_ed25519.pub
```

Linux guest:
```sh
mkdir -p ~/.ssh && chmod 700 ~/.ssh
echo "ssh-rsa AAAA...your-mac-key... you@mac" >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
sudo apt install -y openssh-server && sudo systemctl enable --now ssh
```

Find the guest IP with `ip addr show | grep "inet " | grep -v 127.0.0.1`.

Verify from the Mac: `ssh you@<vm-ip> echo ok`. Once that works, the agent can
run any build/test command on the guest via `ssh`.

### 2. Editing: Mac is the source of truth

The agent edits files in the **Mac checkout** (where git push access lives),
then gets them onto the guest one of two ways:

- **git** — commit/push on the Mac, `git pull` on the guest. Cleanest, but a
  round-trip per change.
- **Shared folder** — VMware Fusion shared folders (or an SMB share like
  `smb://<host>/dev/...`, mounted on the Mac at `/Volumes/dev/...`) expose the
  guest's working tree directly. The agent can `cp` an edited file straight
  onto the share and rebuild without a commit, then make one clean commit at
  the end. The share is also handy for the agent to **read build outputs and
  result images** directly (e.g. inspect rendered golden PNGs).

> Tip: never `git pull` after a history rewrite (e.g. a squash + force-push) —
> on the guest run `git fetch && git reset --hard origin/<branch>` instead.

---

## Windows — Remote Machine

### 1. Enable OpenSSH on the Windows machine

`Settings → System → Optional Features → Add a feature → OpenSSH Server`

Then start the service:

```powershell
Start-Service sshd
Set-Service -Name sshd -StartupType Automatic
```

Install the server if the optional feature isn't present, from an **admin**
PowerShell:

```powershell
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
Start-Service sshd
Set-Service -Name sshd -StartupType Automatic
```

### 2. Connect from macOS (key login)

Find the Windows IP with `ipconfig` (the `IPv4 Address` of the active adapter).

**Gotcha — administrator accounts.** `ssh-copy-id` / `~/.ssh/authorized_keys`
do **not** work for accounts in the Administrators group; OpenSSH on Windows
reads admin keys from a different file. Put your Mac's public key there and fix
its ACL, in an **admin** PowerShell:

```powershell
Set-Content -Path "C:\ProgramData\ssh\administrators_authorized_keys" `
  -Value "ssh-rsa AAAA...your-mac-key... you@mac"
icacls "C:\ProgramData\ssh\administrators_authorized_keys" /inheritance:r `
  /grant "Administrators:F" /grant "SYSTEM:F"
# ensure the file is enabled in sshd_config (it's usually present but commented):
(Get-Content C:\ProgramData\ssh\sshd_config) `
  -replace '#\s*AuthorizedKeysFile __PROGRAMDATA__','AuthorizedKeysFile __PROGRAMDATA__' `
  | Set-Content C:\ProgramData\ssh\sshd_config
Restart-Service sshd
```

For a **non-admin** account, the normal `~/.ssh/authorized_keys` (or
`ssh-copy-id`) works as usual. Verify from the Mac: `ssh <user>@<windows-ip>
"ver"`.

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
- **Windows GUI apps need an interactive desktop session.** A Skia example
  launched over SSH lands in the isolated services session 0, which has no
  GPU/WGL — run it from the desktop, not the SSH shell.
- **Avoid building Windows through a directory junction that crosses drive
  letters** (e.g. `C:\dev\…` junctioned to `D:\…`). vcpkg's harfbuzz/meson
  install step calls Python `os.path.relpath`, which throws across drives.
  Build from the real physical path. See `ai/build_setup_guide.md` for this
  and other per-platform build gotchas.
