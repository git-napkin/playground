# Grant

What it is: A privilege escalator that injects `libfangs_hook.dylib` into `launchd` (PID 1).

Why it is: Required to get the Frida-Gum `posix_spawn` interceptor running inside launchd before any user processes start.

Role:
1. Uses `task_for_pid(mach_task_self(), 1, &task)` to attach to launchd.
2. Allocates stack + shellcode memory in launchd's address space via `mach_vm_allocate`.
3. Patches ARM64 shellcode with runtime-resolved addresses for `pthread_create_from_mach_thread` and `dlopen`.
4. Creates a new thread inside launchd that calls `dlopen("libfangs_hook.dylib")`.
5. Waits for a sentinel return value (`0x79616265`) confirming the dylib loaded.
6. Handles macOS 14.4+ `thread_create_running` vs older `thread_set_state` + `thread_resume` differences.

How it is used: Runs as a LaunchDaemon (`com.pluginplayground.grant`) at boot. Requires SIP to be disabled and the `com.apple.system-task-ports` entitlement.

Optional: No.
