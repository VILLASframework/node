{
  description = "a tool for connecting real-time power grid simulation equipment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/release-22.11";

    common = {
      url = "github:VILLASframework/common";
      flake = false;
    };

    fpga = {
      type = "git";
      url = "https://github.com/VILLASframework/fpga.git";
      ref = "refs/heads/villas-node";
      submodules = true;
      flake = false;
    };

    lib60870 = {
      url = "github:mz-automation/lib60870/v2.3.2";
      flake = false;
    };

    libdatachannel = {
      type = "git";
      url = "https://github.com/paullouisageneau/libdatachannel.git";
      ref = "refs/tags/v0.18.4";
      submodules = true;
      flake = false;
    };

    libiec61850 = {
      url = "github:mz-automation/libiec61850/v1.5.1";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    ...
  } @ inputs: let
    inherit (nixpkgs) lib;

    # supported systems for native compilation
    supportedSystems = ["x86_64-linux" "aarch64-linux"];

    # supported systems to cross compile to
    supportedCrossSystems = ["aarch64-multiplatform"];

    # generate attributes corresponding to all the supported systems
    forSupportedSystems = lib.genAttrs supportedSystems;

    # generate attributes corresponding to all supported combinations of system and crossSystem
    forSupportedCrossSystems = f: forSupportedSystems (system: lib.genAttrs supportedCrossSystems (f system));

    # this overlay can be applied to nixpkgs (see `pkgsFor` below for an example)
    overlay = final: prev: packagesWith final;

    # initialize nixpkgs for the specified `system`
    pkgsFor = system:
      import nixpkgs {
        inherit system;
        overlays = [overlay];
      };

    # initialize nixpkgs for cross-compiling from `system` to `crossSystem`
    crossPkgsFor = system: crossSystem: (pkgsFor system).pkgsCross.${crossSystem};

    # build villas and its dependencies for the specified `pkgs`
    packagesWith = pkgs: rec {
      default = villas;

      villas-minimal = pkgs.callPackage ./villas.nix {
        src = ../..;
        version = "minimal";
        inherit (inputs) fpga common;
      };

      villas = villas-minimal.override {
        version = "full";
        withAllExtras = true;
        withAllFormats = true;
        withAllHooks = true;
        withAllNodes = true;
      };

      lib60870 = pkgs.callPackage ./lib60870.nix {
        src = inputs.lib60870;
      };

      libdatachannel = pkgs.callPackage ./libdatachannel.nix {
        src = inputs.libdatachannel;
      };

      libiec61850 = pkgs.callPackage ./libiec61850.nix {
        src = inputs.libiec61850;
      };
    };
  in {
    # standard flake attribute for normal packages (not cross-compiled)
    packages = forSupportedSystems (
      system:
        packagesWith (pkgsFor system)
    );

    # non-standard attribute for cross-compilated packages
    crossPackages = forSupportedCrossSystems (
      system: crossSystem:
        packagesWith (crossPkgsFor system crossSystem)
    );

    # standard flake attribute allowing you to add the villas packages to your nixpkgs
    overlays = {
      default = overlay;
    };

    # standard flake attribute for defining developer environments
    devShells = forSupportedSystems (
      system: let
        pkgs = pkgsFor system;
        shellHook = ''[ -z "$PS1" ] || exec "$SHELL"'';
        hardeningDisable = ["all"];
      in rec {
        default = full;

        minimal = pkgs.mkShell {
          inherit shellHook hardeningDisable;
          name = "minimal";
          inputsFrom = [pkgs.villas-minimal];
        };

        full = pkgs.mkShell {
          inherit shellHook hardeningDisable;
          name = "full";
          inputsFrom = [pkgs.villas];
        };
      }
    );

    # standard flake attribute to add additional checks to `nix flake check`
    checks = forSupportedSystems (
      system: let
        pkgs = pkgsFor system;
      in {
        fmt = pkgs.runCommand "check-fmt" {} ''
          cd ${self}
          ${pkgs.alejandra}/bin/alejandra --check . 2> $out
        '';
      }
    );

    # standard flake attribute specifying the formatter invoked on `nix fmt`
    formatter = forSupportedSystems (system: (pkgsFor system).alejandra);
  };
}
