# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  description = "VILLASnode is a client/server application to connect simulation equipment and software.";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    { self, nixpkgs, ... }:
    let
      inherit (nixpkgs) lib;

      nixDir = ./packaging/nix;

      # Add separateDebugInfo to a derivation
      addSeparateDebugInfo = d: d.overrideAttrs { separateDebugInfo = true; };

      # Supported systems for native compilation
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      # Generate attributes corresponding to all the supported systems
      forSupportedSystems = lib.genAttrs supportedSystems;

      # Initialize nixpkgs for the specified `system`
      pkgsFor =
        system:
        import nixpkgs {
          inherit system;
          overlays = with self.overlays; [ default ];
        };

      # Initialize development nixpkgs for the specified `system`
      devPkgsFor =
        system:
        import nixpkgs {
          inherit system;
          overlays = with self.overlays; [
            default
            debug
          ];
        };

      # Build villas and its dependencies for the specified `pkgs`
      packagesWith = pkgs: rec {
        default = villas-node;

        villas-node-python = pkgs.callPackage (nixDir + "/python.nix") { src = ./.; };

        villas-node-minimal = pkgs.callPackage (nixDir + "/villas.nix") {
          src = ./.;
          version = "minimal";
        };

        villas-node = villas-node-minimal.override {
          version = "full";
          withAllExtras = true;
          withAllFormats = true;
          withAllHooks = true;
          withAllNodes = true;
        };

        clang-include-graph = pkgs.callPackage ({
          fetchFromGitHub,
          cmake,
          boost,
          pkg-config,
          clangStdenv,
          llvmPackages,
        }: clangStdenv.mkDerivation {
          pname = "clang-include-graph";
          version = "0.1.1-unstable-2024-09-23";
          src = fetchFromGitHub {
            owner = "bkryza";
            repo = "clang-include-graph";
            rev = "7d3ddc826a668c7cebde5ee6070545ed9d1e2ca8";
            hash = "sha256-v6jVCFhe8ZrMOFNVWOt46e8sy8QqKg5pXIE8pD0fwBQ=";
          };
          nativeBuildInputs = [
            cmake
            pkg-config
            llvmPackages.llvm
          ];
          buildInputs = [
            boost
            llvmPackages.libclang
          ];
        }) {};
      };
    in
    {
      # Standard flake attribute for normal packages (not cross-compiled)
      packages = forSupportedSystems (system: packagesWith (pkgsFor system));

      # Standard flake attribute allowing you to add the villas packages to your nixpkgs
      overlays = {
        default = final: prev: packagesWith final;
        debug = final: prev: {
          jansson = addSeparateDebugInfo prev.jansson;
          libmodbus = addSeparateDebugInfo prev.libmodbus;
        };
        minimal = final: prev: {
          mosquitto = prev.mosquitto.override { systemd = final.systemdMinimal; };
          rdma-core = prev.rdma-core.override { udev = final.systemdMinimal; };
        };
      };

      # Standard flake attribute for defining developer environments
      devShells = forSupportedSystems (
        system:
        let
          pkgs = devPkgsFor system;
          shellHook = ''[ -z "$PS1" ] || exec "$SHELL"'';
          hardeningDisable = [ "all" ];
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
            clang-include-graph
          ];
        in
        rec {
          default = full;

          full = pkgs.mkShell {
            inherit shellHook hardeningDisable packages;
            name = "full";
            inputsFrom = with pkgs; [ villas-node ];
          };

          python = pkgs.mkShell {
            inherit shellHook hardeningDisable;
            name = "python";
            inputsFrom = with pkgs; [ villas-node-python ];
            packages =
              with pkgs;
              packages
              ++ [
                (python3.withPackages (python-pkgs: [
                  python-pkgs.build
                  python-pkgs.twine
                ]))
              ];
          };
        }
      );

      # Standard flake attribute to add additional checks to `nix flake check`
      checks = forSupportedSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          fmt = pkgs.runCommand "check-fmt" { } ''
            cd ${self}
            "${pkgs.nixfmt}/bin/nixfmt" --check . 2>> $out
          '';
        }
      );

      # Standard flake attribute specifying the formatter invoked on `nix fmt`
      formatter = forSupportedSystems (system: (pkgsFor system).alejandra);

      # Standard flake attribute for NixOS modules
      nixosModules = rec {
        default = villas;

        villas = {
          imports = [ (nixDir + "/module.nix") ];
          nixpkgs.overlays = [ self.overlays.default ];
        };
      };
    };
}
