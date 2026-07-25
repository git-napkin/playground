<p align="center">
  <img src=".pics/PlainLogo.png" width="128" alt="Plugin Playground">
</p>

# Plugin Playground

An open-source runtime tweak system for macOS Apple Silicon.

> [!WARNING]
> System Integrity Protection (SIP) must be partially disabled — `csrutil enable --without debug` — this lets us access `initproc` and set hardware breakpoints on other processes. SIP only needs to allow debugging, not fully off.

Plugin Playground intercepts and modifies running processes. Build runtime plugins, introspection tools, and behavior-modification tweaks with it.

Plugin Playground must run as **arm64e** (the system ABI for Apple Silicon) to attach
to launchd. If arm64e is not available on your system, toggle **Disable arm64e (PAC)** in the
configurator — this strips PAC signing from spawned processes so injection works without the
native arm64e ABI.

The configuration app is installed to `/Applications/Plugin Playground.app`.

![Configurator](.pics/Configurator.png)

## What tweaks do

Tweaks are `.dylib` libraries injected into processes at spawn time, before `main()` runs. They can change UI rendering, alter window management, override system controls, or replace framework behavior. The injection is transparent and needs no modification to the target app. Below are two tweaks built with the runtime:

- **Classic Dock** — replaces the modern Dock with a pre-Yosemite style (3D shelf, reflective icons, unified minimize).
![Classic Dock](.pics/ClassicDock.png)
- **Classic Scrollbars** — restores legacy scrollbars with up/down arrows and the classic aqua thumb.
<img src=".pics/ClassicScrollbars.png" height="260" alt="Classic Scrollbars">


## Ammonia legacy usage

The configurator's **Use legacy Ammonia tweaks folder** option loads tweaks from the old path `/private/var/ammonia/core/tweaks/` instead of `/opt/pluginplayground/tweaks/`. Useful when migrating from an existing Ammonia setup.

If you use this option, disable or remove the Ammonia daemon at `/private/var/ammonia/core/ammonia` first. Otherwise Ammonia and Plugin Playground conflict over injection control. Adding or removing tweaks via the legacy folder often requires a reboot.

## Build requirements

- macOS Apple Silicon (ARM64)
- Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.16+
- git
- Internet connection (first build fetches Slint via FetchContent)

## Build and install

```sh
sh ./install.sh
```

Produces `PluginPlayground-1.0.0.pkg`. Run the `.pkg` to install, or pass a custom prefix path to install without the GUI installer. Uninstall with `./uninstall.sh`.

## Documentation

- [Ammonia (Legacy)](docs/ammonia.md)
- [Compilation](docs/compilation.md)
- [Configurator](docs/configurator.md)
- [Defaults CLI](docs/defaults.md)
- [Fangs](docs/fangs.md)
- [Grant](docs/grant.md)
