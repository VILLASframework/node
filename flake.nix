# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  description = "VILLASnode is a client/server application to connect simulation equipment and software.";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    ethercat = {
      url = "gitlab:etherlab.org/ethercat/stable-1.5";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    ...
  } @ inputs: let
    inherit (nixpkgs) lib;

    nixDir = ./packaging/nix;

    # Add separateDebugInfo to a derivation
    addSeparateDebugInfo = d:
      d.overrideAttrs {
        separateDebugInfo = true;
      };

    # Supported systems for native compilation
    supportedSystems = ["x86_64-linux" "aarch64-linux"];

    # Supported systems to cross compile to
    supportedCrossSystems = ["aarch64-multiplatform"];

    # Generate attributes corresponding to all the supported systems
    forSupportedSystems = lib.genAttrs supportedSystems;

    # Generate attributes corresponding to all supported combinations of system and crossSystem
    forSupportedCrossSystems = f: forSupportedSystems (system: lib.genAttrs supportedCrossSystems (f system));

    # Initialize nixpkgs for the specified `system`
    pkgsFor = system:
      import nixpkgs {
        inherit system;
        overlays = with self.overlays; [default];
      };

    # Initialize nixpkgs for cross-compiling from `system` to `crossSystem`
    crossPkgsFor = system: crossSystem:
      (import nixpkgs {
        inherit system;
        overlays = with self.overlays; [
          default
          minimal
        ];
      })
      .pkgsCross
      .${crossSystem};

    # Initialize development nixpkgs for the specified `system`
    devPkgsFor = system:
      import nixpkgs {
        inherit system;
        overlays = with self.overlays; [default debug];
      };

    # Build villas and its dependencies for the specified `pkgs`
    packagesWith = pkgs: rec {
      default = villas;

      villas-python = pkgs.callPackage (nixDir + "/python.nix") {
        src = ./.;
      };

      villas-minimal = pkgs.callPackage (nixDir + "/villas.nix") {
        src = ./.;
        version = "minimal";
      };

      villas = villas-minimal.override {
        version = "full";
        withAllExtras = true;
        withAllFormats = true;
        withAllHooks = true;
        withAllNodes = true;
      };

      ethercat = pkgs.callPackage (nixDir + "/ethercat.nix") {
        src = inputs.ethercat;
      };
    };
  in {
    # Standard flake attribute for normal packages (not cross-compiled)
    packages = forSupportedSystems (
      system:
        packagesWith (pkgsFor system)
    );

    # Non-standard attribute for cross-compilated packages
    crossPackages = forSupportedCrossSystems (
      system: crossSystem:
        packagesWith (crossPkgsFor system crossSystem)
    );

    # Standard flake attribute allowing you to add the villas packages to your nixpkgs
    overlays = {
      default = final: prev: packagesWith final;
      debug = final: prev: {
        jansson = addSeparateDebugInfo prev.jansson;
        libmodbus = addSeparateDebugInfo prev.libmodbus;
      };
      minimal = final: prev: {
        mosquitto = prev.mosquitto.override {systemd = final.systemdMinimal;};
        rdma-core = prev.rdma-core.override {udev = final.systemdMinimal;};
      };
    };

    # Standard flake attribute for defining developer environments
    devShells = forSupportedSystems (
      system: let
        pkgs = devPkgsFor system;
        shellHook = ''[ -z "$PS1" ] || exec "$SHELL"'';
        hardeningDisable = ["all"];
        packages = with pkgs; [
          bashInteractive
          bc
          boxfort
          clang-tools
          criterion
          jq
          libffi
          libgit2
          pcre
          reuse
          cppcheck
        ];
      in rec {
        default = full;

        minimal = pkgs.mkShell {
          inherit shellHook hardeningDisable packages;
          name = "minimal";
          inputsFrom = with pkgs; [villas-minimal];
        };

        full = pkgs.mkShell {
          inherit shellHook hardeningDisable packages;
          name = "full";
          inputsFrom = with pkgs; [villas];
        };
      }
    );

    # Standard flake attribute to add additional checks to `nix flake check`
    checks = forSupportedSystems (
      system: let
        pkgs = pkgsFor system;
      in {
        fmt = pkgs.runCommand "check-fmt" {} ''
          cd ${self}
          "${pkgs.alejandra}/bin/alejandra" --check . 2>> $out
        '';
      }
    );

    # Standard flake attribute specifying the formatter invoked on `nix fmt`
    formatter = forSupportedSystems (system: (pkgsFor system).alejandra);

    # Standard flake attribute for NixOS modules
    nixosModules = rec {
      default = villas;

      villas = {
        imports = [(nixDir + "/module.nix")];
        nixpkgs.overlays = [
          self.overlays.default
        ];
      };
    };
  };
}
