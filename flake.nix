# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  description = "VILLASnode is a client/server application to connect simulation equipment and software.";

  nixConfig = {
    extra-substituters = [
      "https://villas.cachix.org"
    ];
    extra-trusted-public-keys = [
      "villas.cachix.org-1:vCWp9IzwxFT6ovZivQAvn5ZuLST01bpAGXWwlGTZ9fA="
    ];
  };

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    {
      self,
      nixpkgs,
      ...
    }:
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
          overlays = with self.overlays; [
            default
            patches
          ];
        };

      # Initialize development nixpkgs for the specified `system`
      devPkgsFor =
        system:
        import nixpkgs {
          inherit system;
          overlays = with self.overlays; [
            default
            patches
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

        villas-node-clang = pkgs.villas-node.override {
          stdenv = pkgs.clangStdenv;
        };

        dockerImage = pkgs.dockerTools.buildLayeredImage {
          name = "villas-node";
          tag = "latest-nix";
          contents = [ villas-node ];
          config.ENTRYPOINT = "/bin/villas";
        };

        # Cross-compiled packages
        villas-node-x86_64-linux =
          if pkgs.system == "x86_64-linux" then pkgs.villas-node else pkgs.pkgsCross.x86_64-linux.villas-node;
        villas-node-aarch64-linux =
          if pkgs.system == "aarch64-linux" then
            pkgs.villas-node
          else
            pkgs.pkgsCross.aarch64-multiplatform.villas-node;

        dockerImage-x86_64-linux = pkgs.dockerTools.buildLayeredImage {
          name = "villas-node";
          tag = "latest-nix-x86_64-linux";
          contents = [ villas-node-x86_64-linux ];
          config.ENTRYPOINT = "/bin/villas";
        };

        dockerImage-aarch64-linux = pkgs.dockerTools.buildLayeredImage {
          name = "villas-node";
          tag = "latest-nix-aarch64-linux";
          contents = [ villas-node-aarch64-linux ];
          config.ENTRYPOINT = "/bin/villas";
        };

        # Third-party dependencies

        opendssc = pkgs.callPackage (nixDir + "/opendssc.nix") { };
        orchestra = pkgs.callPackage (nixDir + "/orchestra.nix") { };
        grpc-server-reflection = pkgs.callPackage (nixDir + "/grpc_server_reflection.nix") { };
      };
    in
    {
      # Standard flake attribute for normal packages (not cross-compiled)
      packages = forSupportedSystems (system: packagesWith (pkgsFor system));

      # Standard flake attribute allowing you to add the villas packages to your nixpkgs
      overlays = {
        default = final: prev: packagesWith final;

        patches = import ./packaging/nix/patches.nix;

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

          packages = with pkgs; [
            bashInteractive
            bc
            boxfort
            clang-tools
            criterion
            gdb
            jq
            libffi
            libgit2
            pcre
            reuse
            cppcheck
            pre-commit
            ruby # for pre-commit markdownlint hook
          ];

          mkShellFor = stdenv: pkg: stdenv.mkDerivation {
            name = "${pkg.pname}-${stdenv.cc.cc.pname}-devShell";

            # disable all hardening to suppress warnings in debug builds
            hardeningDisable = [ "all" ];

            # inherit inputs from pkg
            buildInputs = pkg.buildInputs ++ packages;
            nativeBuildInputs = pkg.nativeBuildInputs ++ packages;
            propagatedBuildInputs = pkg.propagatedBuildInputs;
            propagatedNativeBuildInputs = pkg.propagatedNativeBuildInputs;

            # configure nix-ld for pre-commit
            env = {
              NIX_LD = lib.fileContents "${stdenv.cc}/nix-support/dynamic-linker";
              NIX_LD_LIBRARY_PATH = lib.makeLibraryPath [ pkgs.gcc-unwrapped.lib ];
            };
          };
        in
        rec {
          default = gcc;

          gcc = mkShellFor pkgs.stdenv pkgs.villas-node;
          clang = mkShellFor pkgs.clangStdenv pkgs.villas-node;

          python = pkgs.mkShell {
            name = "villas-python-devShell";
            hardeningDisable = [ "all" ];
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
          nixpkgs.overlays = [
            self.overlays.default
            self.overlays.patches
          ];
        };
      };
    };
}
