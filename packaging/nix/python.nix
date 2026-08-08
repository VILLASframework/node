# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
#
# Build the VILLASnode Python package using uv2nix/pyproject.nix.
#
# The Python project lives in ./python and is managed with uv (see
# python/pyproject.toml & python/uv.lock). This expression loads the uv
# workspace, generates a pyproject.nix overlay from uv.lock and builds the
# package set from it.
{
  src,
  pkgs,
  lib,
  python3,
  # protoc matching the locked Python protobuf runtime (6.32.x -> protoc 32.x),
  # so the regenerated bindings are version-compatible with the runtime.
  protobuf_32,
  uv2nix,
  pyproject-nix,
  pyproject-build-systems,
}:
let
  # The uv workspace root is the python/ sub-directory which holds both
  # pyproject.toml & uv.lock.
  workspace = uv2nix.lib.workspace.loadWorkspace { workspaceRoot = src + "/python"; };

  # Generate a pyproject.nix overlay from uv.lock.
  overlay = workspace.mkPyprojectOverlay {
    # Prefer binary wheels: they are much more likely to "just work".
    sourcePreference = "wheel";
  };

  # Instantiate the pyproject.nix build infrastructure for our interpreter.
  pythonBase = pkgs.callPackage pyproject-nix.build.packages {
    python = python3;
  };

  # Project-specific build fixups.
  pyprojectOverrides =
    final: prev:
    let
      inherit (final) resolveBuildSystem;
      inherit (builtins) mapAttrs;

      # Build-system dependencies for packages built from sdist.
      # uv.lock does not record build systems, so declare them here.
      buildSystemOverrides = {
        linuxfd = {
          setuptools = [ ];
        };
        libconf = {
          setuptools = [ ];
        };
      };
    in
    mapAttrs (
      name: spec:
      prev.${name}.overrideAttrs (old: {
        nativeBuildInputs = (old.nativeBuildInputs or [ ]) ++ resolveBuildSystem spec;
      })
    ) buildSystemOverrides
    // {
      # Regenerate the protobuf bindings from the canonical .proto definition
      # instead of shipping the checked-in generated file.
      villas-node = prev.villas-node.overrideAttrs (old: {
        # 'editables' is required by hatchling to build editable (PEP 660)
        # wheels, as used by the development shell.
        nativeBuildInputs = (old.nativeBuildInputs or [ ]) ++ resolveBuildSystem { editables = [ ]; };
        postPatch = (old.postPatch or "") + ''
          ${protobuf_32}/bin/protoc \
            --proto_path ${src}/lib/formats \
            --python_out=villas/node \
            ${src}/lib/formats/villas.proto
        '';
      });
    };

  # Compose base set + build systems + the uv.lock generated packages.
  pythonSet = pythonBase.overrideScope (
    lib.composeManyExtensions [
      pyproject-build-systems.overlays.wheel
      overlay
      pyprojectOverrides
    ]
  );
in
pythonSet
// {
  # The package set, exposed for further overriding & for the dev shell.
  inherit pythonSet workspace;

  # A ready-to-use virtual environment with the package installed.
  virtualenv = pythonSet.mkVirtualEnv "villas-node-env" workspace.deps.default;

  # A virtual environment including the development dependencies.
  virtualenv-dev = pythonSet.mkVirtualEnv "villas-node-dev-env" {
    villas-node = [ "dev" ];
  };
}
