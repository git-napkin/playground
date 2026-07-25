# PAC Stripping (`arm64e` Bypass)

## What it is there for

Apple Silicon Macs natively run system processes using the `arm64e` ABI, which incorporates **Pointer Authentication Codes (PAC)**. PAC cryptographically signs function pointers in memory to prevent exploits.

When Plugin Playground injects custom tweaks (`.dylib` files) into a system process, the injected library must load into the target process's memory space. If the target is running as `arm64e` with PAC enabled, loading unauthenticated binaries can cause kernel panics or PAC violations.

## Current status

PAC stripping is currently a **Work In Progress** (the configurator has a toggle labeled as such). The feature is not yet implemented in the fork — the original `exe.c` module that performed Mach-O header modification (`depacify`, `strip_code_signature`, resign) has been removed as part of the migration from the HW breakpoint architecture to Frida-Gum.

## Future plan

When implemented, PAC stripping will:
1. Intercept the target executable before spawn (at the `posix_spawn` hook level).
2. Modify the raw Mach-O header to remove the `CPUTYPE_ARM64E` flag, downgrading the executable to standard `arm64`.
3. Strip the `LC_CODE_SIGNATURE` load command.
4. Spawn the modified executable.

## Alternative: Native `arm64e` Support

If you prefer to compile Plugin Playground natively as `arm64e` (using Xcode) and do NOT want to rely on PAC stripping, you must enable the `arm64e` preview ABI on your Mac. Apple disables third-party `arm64e` execution by default.

To enable the native `arm64e` ABI:
1. Run: `sudo nvram boot-args="-arm64e_preview_abi"`
2. Reboot.

*(Note: Modifying `boot-args` requires SIP to be appropriately disabled or adjusted from Recovery Mode.)*
