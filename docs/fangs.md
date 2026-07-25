# Fangs hook

A Frida-Gum interceptor library (`libfangs_hook.dylib`) injected into launchd at runtime. Intercepts `posix_spawn`/`posix_spawnp` calls and injects the tweak loader into child UI processes.

1. Loads Frida-Gum embedded runtime (`gum_init_embedded`).
2. Replaces `posix_spawn` and `posix_spawnp` with wrappers via `gum_interceptor_replace`.
3. On spawn, filters by darwin role to identify UI processes.
4. Skips blacklisted processes (loaded from `ammonia.blacklist`).
5. Detects and skips Node.js SEA (Single Executable Application) binaries.
6. Propagates itself into `xpcproxy` children for recursive hook coverage.
7. Injects `DYLD_INSERT_LIBRARIES=libplayground_opener.dylib` into eligible processes.

Loaded automatically inside launchd by the `grant` daemon's shellcode injection. Required, no opt-out.
