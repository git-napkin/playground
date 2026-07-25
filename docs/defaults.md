# Configuration Defaults

The Configurator saves preferences in a standard macOS XML Property List (plist) file. Configure the environment from the terminal or scripts without opening the GUI.

Configuration file: `/opt/pluginplayground/current.options`

## Available keys

### Boolean keys

- `disablePAC`: Disables arm64e PAC signing for spawned processes. Required when compiling without native arm64e ABI.
- `useLegacyAmmonia`: Uses the legacy tweak path (`/private/var/ammonia/core/tweaks/`) instead of `/opt/pluginplayground/tweaks/`.
- `pauseInjection`: Pauses tweak injection for all processes.

### Array keys

- `enabledTweaks`: Array of tweak filenames (e.g. `MyTweak.dylib`) that the user has enabled. Tweaks in the directory not listed here are skipped. Manage from the Configurator GUI or command line.

## Command-line usage

Since the config is a standard plist, modify it with `defaults` or `plutil`.

### Examples using `defaults`

**Enable PAC stripping:**
```bash
defaults write /opt/pluginplayground/current.options disablePAC -bool true
```

**Turn on legacy Ammonia tweaks:**
```bash
defaults write /opt/pluginplayground/current.options useLegacyAmmonia -bool true
```

**Pause injection:**
```bash
defaults write /opt/pluginplayground/current.options pauseInjection -bool true
```

**Enable a specific tweak:**
```bash
defaults write /opt/pluginplayground/current.options enabledTweaks -array "MyTweak.dylib"
```

**Enable multiple tweaks:**
```bash
defaults write /opt/pluginplayground/current.options enabledTweaks -array "TweakA.dylib" "TweakB.dylib"
```

**Add a tweak to the existing enabled list:**
```bash
defaults write /opt/pluginplayground/current.options enabledTweaks -array-add "NewTweak.dylib"
```

**Read the current configuration:**
```bash
defaults read /opt/pluginplayground/current.options
```

> [!NOTE]
> If the file does not exist yet, use `sudo defaults write ...` the first time. After the Configurator GUI creates it, the file is writable by all users (`chmod 666`).
