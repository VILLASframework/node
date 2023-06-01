{
  description = "a tool for connecting real-time power grid simulation equipment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";

    common = {
      url = "github:VILLASframework/common?submodules=1";
      flake = false;
    };

    lib60870 = {
      url = "github:mz-automation/lib60870/v2.3.2";
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
    supportedSystems = ["x86_64-linux" "aarch64-linux"];
    forSupportedSystems = lib.genAttrs supportedSystems;
    legacyPackages = forSupportedSystems (
      system:
        import nixpkgs {
          inherit system;
          overlays = [(final: prev: self.packages.${system})];
        }
    );
  in {
    formatter = forSupportedSystems (system: legacyPackages.${system}.alejandra);
    packages = forSupportedSystems (
      system: let
        pkgs = legacyPackages.${system};
      in rec {
        default = villas;

        villas-minimal = pkgs.callPackage ./villas.nix {
          src = ../..;
          common = inputs.common;
        };

        villas = villas-minimal.override {
          withConfig = true;
          withProtobuf = true;
          withAllNodes = true;
        };

        lib60870 = pkgs.callPackage ./lib60870.nix {
          src = inputs.lib60870;
        };

        libiec61850 = pkgs.callPackage ./libiec61850.nix {
          src = inputs.libiec61850;
        };
      }
    );
    devShells = forSupportedSystems (
      system: let
        pkgs = legacyPackages.${system};
        shellHook = ''
          [ -z "$PS1" ] || exec $SHELL
        '';
      in rec {
        default = full;

        minimal = pkgs.mkShell {
          inherit shellHook;
          name = "minimal";
          inputsFrom = [pkgs.villas-minimal];
        };

        full = pkgs.mkShell {
          inherit shellHook;
          name = "full";
          inputsFrom = [pkgs.villas];
        };
      }
    );
    checks = forSupportedSystems (
      system: let
        pkgs = legacyPackages.${system};
      in {
        fmt = pkgs.runCommand "check-fmt" {} ''
          cd ${self}
          ${pkgs.alejandra}/bin/alejandra --check . 2> $out
        '';
      }
    );
  };
}
