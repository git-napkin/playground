# Plugin Playground — Agent Guide

## Build

```sh
./setup_frida.sh              # download Frida-Gum devkit (auto-if missing)
sh ./install.sh                # full build → PluginPlayground-1.0.0.pkg
sudo installer -pkg "PluginPlayground-1.0.0.pkg" -target /
```

Manual build (without installer):
```sh
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release [-DBUILD_CONFIGURATOR=OFF]
cmake --build .build
```

Quick rebuild of just the test tweak:
```sh
make -C testing    # builds testing/testing.dylib (arm64 bundle)
```

Nix: `nix build` (produces arm64-only binaries, requires PAC-stripping toggle).

## Architecture

- `libfangs_hook.dylib` — Frida-Gum hook injected into launchd via shellcode. Intercepts `posix_spawn`/`posix_spawnp` to inject the tweak loader into child UI processes.
- `libplayground_opener.dylib` — per-process tweak loader. Injected via `DYLD_INSERT_LIBRARIES` by fangs_hook. Loads tweaks from `/opt/pluginplayground/tweaks/`.
- `grant` — launchd shellcode injector. Runs as `com.pluginplayground.grant` LaunchDaemon. Uses `task_for_pid`, `mach_vm_allocate`, PAC-authenticated thread creation.
- `configurator` — Slint (Rust) GUI in `/Applications/Plugin Playground.app`. Optional: `-DBUILD_CONFIGURATOR=OFF`.

## System requirements

- macOS Apple Silicon (arm64)
- SIP partially disabled: `csrutil enable --without debug`
- Default ABI is arm64e. Toggle "Disable arm64e (PAC)" in configurator if building without native arm64e support.
- Xcode CLT, CMake 3.16+, git

## Config (`/opt/pluginplayground/current.options`)

XML plist. Modify via `defaults` or `plutil`:

```sh
defaults write /opt/pluginplayground/current.options disablePAC -bool true
defaults write /opt/pluginplayground/current.options enabledTweaks -array "MyTweak.dylib"
defaults read /opt/pluginplayground/current.options
```

Keys: `disablePAC`, `useLegacyAmmonia`, `pauseInjection`, `enabledTweaks`

## Entitlements

`Master.entitlements` enables `task_for_pid-allow`, `com.apple.system-task-ports`, `com.apple.private.thread-set-state`, `get-task-allow`, `platform-application`. All binaries are codesigned with this file (ad-hoc by default, override with `-DCODESIGN_IDENTITY=...`).

## Testing

```sh
# requires installed system (sudo ./install.sh)
./testing.sh
```

Launches test tweak via SIGUSR1 to a whitelisted process (Terminal/Finder/Dock/Safari). Results in `~/pluginplayground_test_results.txt`. CI does NOT run tests — they need SIP-off + launchd injection.

## Source layout

| Path | Purpose |
|------|---------|
| `syphon/` | Core C library: fangs_hook, playground_opener, envbuf, exe, options_loader, tweak_utils |
| `grant/` | main.m — launchd shellcode injector (ObjC, ARC) |
| `configurator/` | Slint GUI: main.cpp, controller, options, tweaks |
| `testing/` | Test tweak dylib + Makefile |
| `docs/` | Markdown docs for each subsystem |
| `installer/` | Distribution.xml + HTML resources for .pkg |
| `setup_frida.sh` | Downloads Frida 17.9.11, builds universal fat lib + dylib |

## Gotchas

- `fridagum.dylib` is a universal arm64e+arm64 fat dylib built from static libs via `lipo`.
- The `configurator` target fetches Slint v1.16.1 via `FetchContent` (pulls Rust toolchain into build). Link-cache managed in `.build-cargo/`.
- `grant/main.m` has hardcoded ARM64e shellcode with offsets tuned for arm64e ABI (`ptrauth` instructions).
- Install path `/opt/pluginplayground/` is hardcoded throughout (`SUPPORT_PATH`).
- Uninstall: `./uninstall.sh` (re-elevates via sudo). Reboot recommended after.
