# PAC Stripping (`arm64e` Bypass)

## What it is there for

Apple Silicon Macs natively run system processes using the `arm64e` ABI, which incorporates **Pointer Authentication Codes (PAC)**. PAC cryptographically signs function pointers in memory to prevent exploits.

When Plugin Playground injects custom tweaks (`.dylib` files) into a system process, the injected library must load into the target process's memory space. If the target is running as `arm64e` with PAC enabled, loading unauthenticated binaries can cause kernel panics or PAC violations.

## How it works

PAC stripping is implemented in `syphon/exe.c` (the "Depacify" engine):

1. In `fangs_hook` (loaded inside launchd), the `disablePAC` option is read from `/opt/pluginplayground/current.options` on startup.
2. On each `posix_spawn`/`posix_spawnp` interception, if `disablePAC` is true, `getready_process()` is called with the target path.
3. If the target is an `.app` bundle, it is copied to `/tmp/RuntimeApplications/` to avoid modifying the original.
4. The Mach-O header is scanned: if `cpusubtype` indicates `arm64e` (`0x2`), it is zeroed to `0` (plain `arm64`). FAT binaries are handled recursively for each slice.
5. The `LC_CODE_SIGNATURE` load command is removed (invalidated by the header change).
6. The executable is re-signed with ad-hoc SHA-256 via `SecCodeSignerCreate`.
7. `fangs_hook` spawns the depacified copy instead of the original.

## Alternative: Native `arm64e` Support

If you prefer to compile Plugin Playground natively as `arm64e` (using Xcode) and do NOT want to rely on PAC stripping, you must enable the `arm64e` preview ABI on your Mac. Apple disables third-party `arm64e` execution by default.

To enable the native `arm64e` ABI:
1. Run: `sudo nvram boot-args="-arm64e_preview_abi"`
2. Reboot.

*(Note: Modifying `boot-args` requires SIP to be appropriately disabled or adjusted from Recovery Mode.)*
