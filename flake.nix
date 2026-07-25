{
  description = "Plugin Playground - An open-source general-purpose runtime tweak system for macOS";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        
        slintSrc = pkgs.fetchFromGitHub {
          owner = "slint-ui";
          repo = "slint";
          rev = "v1.16.1";
          hash = "sha256-3M0uHMGJq249yUtIBiwx3zZODc+OH61QbhOW2gwQR8g=";
        };

        plugin-playground = pkgs.stdenv.mkDerivation {
          pname = "plugin-playground";
          version = "1.0-pre";

          src = ./.;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.darwin.sigtool
            pkgs.rustPlatform.cargoSetupHook
            pkgs.cargo
            pkgs.rustc
            pkgs.corrosion
          ];

          buildInputs = [
            pkgs.apple-sdk_26
          ];

          cargoDeps = pkgs.rustPlatform.importCargoLock {
            lockFile = "${slintSrc}/Cargo.lock";
          };

          postUnpack = ''
            cp -R ${slintSrc} $sourceRoot/slintSrc
            chmod -R +w $sourceRoot/slintSrc
            cp $sourceRoot/slintSrc/Cargo.lock $sourceRoot/Cargo.lock

            substituteInPlace $sourceRoot/slintSrc/api/cpp/CMakeLists.txt \
              --replace-fail 'list(APPEND slint_compiler_features "jemalloc")' ""
          '';

          postPatch = ''
            substituteInPlace CMakeLists.txt \
              --replace-fail "--timestamp=none" ""
          '';

          cmakeFlags = [
            "-DBUILD_CONFIGURATOR=ON"
            "-DFETCHCONTENT_SOURCE_DIR_SLINT=../slintSrc"
          ];

          preInstall = ''
            # ensure fridagum.dylib is available in build tree
            cp "$src/fridagum.dylib" "$PWD/fridagum.dylib" 2>/dev/null || true
          '';

          meta = with pkgs.lib; {
            description = "General-purpose runtime tweak system for macOS Apple Silicon";
            homepage = "https://github.com/CoreBedtime/playground";
            license = licenses.mit;
            maintainers = [ ];
            platforms = [ "aarch64-darwin" ];
          };
        };
      in
      {
        packages.default = plugin-playground;
        packages.plugin-playground = plugin-playground;

        devShells.default = pkgs.mkShell {
          inputsFrom = [ plugin-playground ];
          buildInputs = with pkgs; [
            clang-tools
            git
            cargo
            rustc
          ];
        };
      }
    ) // {
      darwinModules.default = { config, lib, pkgs, ... }:
        with lib;
        let
          cfg = config.services.plugin-playground;
        in {
          options.services.plugin-playground = {
            enable = mkEnableOption "Plugin Playground runtime tweak system";
            package = mkOption {
              type = types.package;
              default = self.packages.${pkgs.system}.default;
              description = "The pluginplayground package to use.";
            };
          };

          config = mkIf cfg.enable {
            environment.systemPackages = [ cfg.package ];

            system.activationScripts.preUserActivation.text = ''
              sudo mkdir -p /opt/pluginplayground/tweaks
              sudo mkdir -p /opt/pluginplayground/lib
              sudo mkdir -p /var/log/pluginplayground
              sudo chmod 755 /opt/pluginplayground/tweaks
              sudo chmod 755 /opt/pluginplayground/lib
              sudo chmod 755 /var/log/pluginplayground
              if [ ! -f /opt/pluginplayground/current.options ]; then
                sudo touch /opt/pluginplayground/current.options
                sudo chmod 644 /opt/pluginplayground/current.options
              fi
            '';

            launchd.daemons."com.pluginplayground.grant" = {
              serviceConfig = {
                Label = "com.pluginplayground.grant";
                ProgramArguments = [ "${cfg.package}/bin/grant" ];
                RunAtLoad = true;
                StandardOutPath = "/var/log/pluginplayground/grant.log";
                StandardErrorPath = "/var/log/pluginplayground/grant.err";
              };
            };
          };
        };
    };
}
