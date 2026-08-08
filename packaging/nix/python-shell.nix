# SPDX-FileCopyrightText: 2026 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
#
# Development shell for the VILLASnode Python package.
#
# Provides an editable install of the package (pointing at the checked-out
# source tree) together with its development dependencies, provisioned via
# uv2nix/pyproject.nix from python/uv.lock.
{
  pkgs,
  villas-node-python,
  # Extra packages to make available in the shell.
  extraPackages ? [ ],
}:
let
  pythonSet = villas-node-python.pythonSet;

  # Install the workspace packages in editable mode pointing at the
  # checked-out source tree, so changes apply without a rebuild.
  editableOverlay = villas-node-python.workspace.mkEditablePyprojectOverlay {
    root = "$REPO_ROOT/python";
  };

  editablePythonSet = pythonSet.overrideScope editableOverlay;

  virtualenv = editablePythonSet.mkVirtualEnv "villas-node-dev-env" {
    villas-node = [ "dev" ];
  };
in
pkgs.mkShell {
  name = "villas-python-devShell";
  hardeningDisable = [ "all" ];
  packages =
    with pkgs;
    extraPackages
    ++ [
      virtualenv
      uv
      ruff
    ];

  env = {
    # Prevent uv from managing its own virtual environment; uv2nix does.
    UV_NO_SYNC = "1";
    UV_PYTHON = editablePythonSet.python.interpreter;
    UV_PYTHON_DOWNLOADS = "never";
  };

  shellHook = ''
    unset PYTHONPATH
    export REPO_ROOT=$(git rev-parse --show-toplevel)

    # Activate the uv2nix-provided virtual environment so that
    # python/pytest/ruff resolve to it and the editable install of
    # villas-node (pointing at $REPO_ROOT/python) is importable.
    export VIRTUAL_ENV="${virtualenv}"
    export PATH="$VIRTUAL_ENV/bin:$PATH"
  '';
}
