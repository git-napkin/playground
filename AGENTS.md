# Plugin Playground — Agent Guide

macOS Apple Silicon runtime tweak system. Injects `.dylib` tweaks into UI processes at spawn via Frida-Gum `posix_spawn`/`posix_spawnp` interception inside launchd.

## Architecture

```
grant (LaunchDaemon) ──shellcode injection──> launchd runs libfangs_hook.dylib
                                                    │
                                          intercepts posix_spawn*
                                                    │
                                          injects libplayground_opener.dylib
                                          into UI processes via DYLD_INSERT_LIBRARIES
                                                    │
                                          dlopen() tweaks from /opt/pluginplayground/tweaks/
```

- **`grant/main.m`** — LaunchDaemon that injects shellcode into launchd (PID 1) via `task_for_pid` + `mach_vm_allocate` / `thread_create_running`. macOS 14.4+ uses `thread_create_running`; older uses `thread_set_state` + `thread_resume`.
- **`syphon/fangs_hook.c`** — Frida-Gum interceptor loaded inside launchd. Hooks `posix_spawn`/`posix_spawnp`, sets `DYLD_INSERT_LIBRARIES` with opener path for UI processes (darwin role). Skips drivers, Node SEA binaries, and blacklisted processes (`/opt/pluginplayground/ammonia.blacklist`).
- **`syphon/playground_opener.c`** — Per-process loader. Uses Frida-Gum interceptor (for future API hooks), scans tweak dir via dlopen, supports whitelist/blacklist per tweak, reloads on SIGUSR1.
- **`configurator/`** — Slint GUI app (C++ + Slint Rust UI framework via FetchContent). Installed to `/Applications/Plugin Playground.app`.

## Build & Test

Requires macOS Apple Silicon, Xcode CLT, CMake 3.16+, Rust (for configurator).

```sh
# Full build + .pkg installer
./install.sh

# Development build (no configurator, skips Rust)
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release -DBUILD_CONFIGURATOR=OFF
cmake --build Build -j8

# Quick verification after build
test -f Build/libfangs_hook.dylib
test -f Build/libplayground_opener.dylib
test -f Build/grant
```

**Frida-Gum setup** (required before any build):
```sh
./setup_frida.sh
```
Pinned to Frida 17.9.11. Downloads arm64e + arm64 devkits, creates universal `libfrida-gum-arm64e-arm64.a` and `fridagum.dylib`.

**Install/uninstall:**
```sh
sudo installer -pkg PluginPlayground-1.0.0.pkg -target /
./uninstall.sh   # re-elevates via sudo, removes everything
```

## Runtime Paths

| Path | Purpose |
|------|---------|
| `/opt/pluginplayground/tweaks/` | Tweak `.dylib` files |
| `/opt/pluginplayground/lib/` | Core dylibs: `fridagum.dylib`, `libfangs_hook.dylib`, `libplayground_opener.dylib` |
| `/opt/pluginplayground/current.options` | Config plist (read by both fangs_hook and configurator) |
| `/opt/pluginplayground/bin/grant` | LaunchDaemon binary |
| `/var/log/pluginplayground/` | Log output |
| `/Applications/Plugin Playground.app/` | Configurator GUI |

## Configuration

Boolean keys in `/opt/pluginplayground/current.options` (XML plist):

- `disablePAC` — Strip PAC from spawned processes (WIP, not yet implemented)
- `useLegacyAmmonia` — Load tweaks from `/private/var/ammonia/core/tweaks/`
- `pauseInjection` — Globally pause tweak injection

CLI:
```sh
defaults write /opt/pluginplayground/current.options disablePAC -bool true
defaults read /opt/pluginplayground/current.options
```

## Tweak System

A tweak is a `.dylib` in the tweaks dir. Per-tweak metadata files alongside it:

- `<name>.dylib.whitelist` — process names to load into (one per line)
- `<name>.dylib.blacklist` — process names to skip
- `<name>.dylib.disabled` — empty marker file disables the tweak
- `<name>.dylib.options` — XML plist with keys:
  - `blacklistedApps` (string array) — skip these process names
  - `frameworkDependencies` (string array) — only load if target links to these frameworks

Tweaks must be **owned by root**, not group/world-writable (`is_tweak_safe` check).

Export a `LoadFunction(void *interceptor)` to receive the Frida-Gum interceptor on load. Hot-reload on `SIGUSR1`.

## SIP Requirement

```
csrutil enable --without debug
```
SIP must allow debugging. Check status: `csrutil status`.
The configurator detects SIP state via `csrutil status` and shows it in the UI.

## arm64e ABI Notes

CMake targets `arm64e` by default. If your system doesn't support third-party arm64e execution:
- Enable preview ABI: `sudo nvram boot-args="-arm64e_preview_abi"` + reboot, OR
- Toggle "Disable arm64e (PAC)" in configurator (strips PAC from spawned processes — feature WIP)
- Nix builds produce standard `arm64` binaries and always require the PAC bypass

## Tweak Packaging

The Configurator can package tweaks into `.pkg` files via `pkgbuild` (available in Xcode / CLT). Output goes to `/tmp/<name>.pkg`. Uses `com.pluginplayground.tweak.<name>` identifier.

## CI

`.github/workflows/build.yml` — builds on push/PR to master, skips configurator (`-DBUILD_CONFIGURATOR=OFF`), verifies the three core artifacts exist.
