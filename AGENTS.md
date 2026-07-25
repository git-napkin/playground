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
- **`syphon/playground_opener.c`** — Per-process loader. Loads fridagum.dylib, scans tweak dir via dlopen, supports whitelist/blacklist per tweak, reloads on SIGUSR1 and auto-reloads when `current.options` changes (watched via dispatch VNODE).
- **`syphon/exe.c`** — PAC stripping: copies `.app` bundles to `/tmp/RuntimeApplications/`, strips arm64e cpusubtype, removes `LC_CODE_SIGNATURE`, re-ad-hoc signs.
- **`configurator/`** — Slint GUI app (C++ + Slint Rust UI framework via FetchContent). Installed to `/Applications/Plugin Playground.app`.

## Build

```sh
# Prerequisite (creates libfrida-gum-arm64e-arm64.a + fridagum.dylib)
./setup_frida.sh

# Development build (no configurator, skips Rust)
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release -DBUILD_CONFIGURATOR=OFF
cmake --build Build -j8

# Full build + .pkg installer
./install.sh

# Quick verification
test -f Build/libfangs_hook.dylib
test -f Build/libplayground_opener.dylib
test -f Build/grant
```

- Frida-Gum pinned to 17.9.11. `setup_frida.sh` downloads arm64e + arm64 devkits, creates a universal static lib + fat shared dylib via `lipo` + `clang -arch arm64e -arch arm64`.
- CMake targets `arm64e` by default. Nix builds produce standard `arm64` and always need PAC bypass.
- Configurator builds symlink the persistent `.build-cargo/` into the build dir for Rust crate cache (Slint FetchContent).
- CI (`.github/workflows/build.yml`): runs on push/PR to `master`/`backup`, `-DBUILD_CONFIGURATOR=OFF`, verifies the three core artifacts.

## Install / Uninstall

```sh
sudo installer -pkg PluginPlayground-1.0.0.pkg -target /
./uninstall.sh   # re-elevates via sudo, removes everything
```

## SIP & Entitlements

SIP must allow debugging: `csrutil enable --without debug`. The grant binary requires these entitlements: `task_for_pid-allow`, `com.apple.system-task-ports`, `com.apple.private.thread-set-state`, `get-task-allow` (see `Master.entitlements`).

## Runtime Paths

| Path | Purpose |
|------|---------|
| `/opt/pluginplayground/tweaks/` | Tweak `.dylib` files |
| `/opt/pluginplayground/lib/` | Core dylibs: `fridagum.dylib`, `libfangs_hook.dylib`, `libplayground_opener.dylib` |
| `/opt/pluginplayground/current.options` | Config plist |
| `/opt/pluginplayground/bin/grant` | LaunchDaemon binary |
| `/opt/pluginplayground/ammonia.blacklist` | Process blacklist (process names, one per line) |
| `/opt/pluginplayground/disable-xpcproxy` | Flag file — if present, skip propagation into xpcproxy |
| `/var/log/pluginplayground/` | Log output (all components log via syslog with LOG_DAEMON) |
| `/tmp/RuntimeApplications/` | PAC-stripped bundle copies |

## Configuration (`current.options`)

XML plist. Keys:

- `disablePAC` (bool) — Strip PAC from spawned processes (copies bundle to `/tmp/RuntimeApplications/`, strips arm64e cpusubtype, re-signs)
- `useLegacyAmmonia` (bool) — Load tweaks from `/private/var/ammonia/core/tweaks/`
- `pauseInjection` (bool) — Globally pause tweak injection
- `enabledTweaks` (string array) — Only these `.dylib` names are loaded

```sh
defaults write /opt/pluginplayground/current.options enabledTweaks -array "MyTweak.dylib"
```

The opener watches `current.options` via dispatch VNODE and reloads on change — no restart needed.

## Tweak System

A tweak is a `.dylib` in the tweaks dir. Only tweaks in `enabledTweaks` are loaded; all others ignored.

Per-tweak metadata files alongside it:

- `<name>.dylib.whitelist` — process names to load into (one per line)
- `<name>.dylib.blacklist` — process names to skip
- `<name>.dylib.options` — XML plist with keys `blacklistedApps` (string array) and `frameworkDependencies` (string array, only load if target links to these frameworks)

Tweaks must be **owned by root**, not group/world-writable. Export `LoadFunction(void *interceptor)` to receive the Frida-Gum interceptor on load. Hot-reload on `SIGUSR1`.
