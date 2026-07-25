# Compilation

The build process is managed by install.sh and CMake.

## install.sh

The primary build script. Automates building components and packaging them into the final installer.

## CMakeLists.txt

The project build configuration. Specifies targets, fetches dependencies like Slint, and coordinates the compilation of grant, libfangs_hook.dylib, libplayground_opener.dylib, and the configurator.

## Nix (optional)

A declarative build environment using flake.nix. Provides a reproducible alternative to system tools. Run `nix build` or use `nix develop` to build the project. Completely optional.

It can also be wrapped into a nixpkgs package for `nix-darwin` and `home-manager` integration.

**Important note for Apple Silicon:** The Nix build uses standard open-source toolchains and compiles both Rust (Slint) and C++ components without linker conflicts. It generates standard `arm64` binaries rather than Apple's native `arm64e` ABI. When using the Nix-compiled version, you must toggle **Disable arm64e (PAC)** in the Configurator so injection works.