# AGENTS.md

## What this repo actually is
- **Plugin Playground** — a macOS (Apple Silicon) runtime code injection framework that uses Frida-Gum to intercept `posix_spawn`/`posix_spawnp` at runtime. Successor to the Ammonia tweak injector.
- Two shared libraries + one daemon binary + one GUI app:
  - `libfangs_hook.dylib` — Frida-Gum posix_spawn hook, injected into launchd via shellcode
  - `libplayground_opener.dylib` — per-process tweak loader, injected via `DYLD_INSERT_LIBRARIES`
  - `grant` — privilege escalator that injects shellcode into launchd (PID 1) to load `libfangs_hook.dylib`
  - `configurator` — Slint/C++ GUI for managing tweaks, options, and daemon lifecycle
- `syphon/tweak_utils` — shared C library of path matching, Mach-O parsing, plist parsing, and safety checks used by both `fangs_hook` and `playground_opener`
- Pure Xcode/clang toolchain. arm64e only (Apple Silicon).

## Build commands
```sh
./setup_frida.sh   # only needed once to download Frida-Gum devkit
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release
cmake --build Build -j8
# or for a distributable package:
./install.sh
```

## Verification
- CI (`.github/workflows/build.yml`) builds with `BUILD_CONFIGURATOR=OFF` (no Slint fetch required for CI) and asserts `Build/libfangs_hook.dylib`, `Build/libplayground_opener.dylib`, and `Build/grant` exist.
- No formal test suite exists. "It builds and the two dylibs + binary exist" is the automated gate.

## Security notes
- Tweak dylibs must be owned by root and not group/world-writable (`tweak_utils.c:is_tweak_safe`)
- Tweak filenames are validated against path traversal (`is_safe_filename` rejects `..` and `/`)
- Blacklist/whitelist matching uses strict path matching (`path_matches_entry`) instead of substring `strstr` — prevents bypass via name embedding
- Configurator's `packageTweak` validates tweak names against a safe character set before shell execution

## Runtime requirements
- SIP disabled or partially disabled (required for `task_for_pid(1, ...)`)
- `com.apple.system-task-ports` entitlement
- Library validation disabled (moot when SIP is off, but `grant` no longer patches amfid)

## Architecture (three-stage injection)
```
Boot → launchd → grant (LaunchDaemon)
                   └─ injects shellcode into launchd
                      └─ dlopen(libfangs_hook.dylib) inside launchd
                           └─ Frida-Gum hooks posix_spawn/posix_spawnp
                                └─ on UI process spawn → DYLD_INSERT_LIBRARIES=libplayground_opener.dylib
                                     └─ playground_opener scans tweaks/ and loads matching .dylib files
```
