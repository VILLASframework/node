{
  description = "a tool for connecting real-time power grid simulation equipment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";

    src = {
      url = "git+file:.?submodules=1";
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
    packages = forSupportedSystems (system:
      let pkgs = legacyPackages.${system};
      in rec {
        default = villas;

        villas-minimal = pkgs.callPackage ./villas.nix {
          inherit (inputs) src;
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
    devShells = forSupportedSystems (system:
      let pkgs = legacyPackages.${system};
      in rec {
        default = full;

        minimal = pkgs.mkShell {
          name = "minimal";
          shellHook = "exec $SHELL";
          inputsFrom = [ pkgs.villas-minimal ];
        };

        full = pkgs.mkShell {
          name = "full";
          shellHook = "exec $SHELL";
          inputsFrom = [ pkgs.villas ];
        };
      }
    );
  };
}
