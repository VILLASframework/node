# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  config,
  lib,
  pkgs,
  ...
}: let
  cfg = config.services.villas.node;

  villasNodeCfg = pkgs.writeText "villas-node.conf" (builtins.toJSON cfg.config);
in
  with lib; {
    options = {
      services.villas.node = {
        enable = mkEnableOption (lib.mdDoc "VILLASnode is a client/server application to connect simulation equipment and software.");

        package = mkPackageOption pkgs "villas-node" {};

        configPath = mkOption {
          type = types.nullOr types.path;
          description = mdDoc ''
            A path to a VILLASnode configuration file.
          '';
          default = null;
        };

        config = mkOption {
          type = types.attrsOf types.anything;
          default = {
            nodes = {};
            paths = [];
            idle_stop = false; # Do not stop daemon because of empty paths
          };

          description = mdDoc ''
            Contents of the VILLASnode configuration file,
            {file}`villas-node.json`.

            See: https://villas.fein-aachen.org/docs/node/config/
          '';
        };
      };
    };

    config = mkIf cfg.enable {
      assertions = [
        {
          assertion = cfg.config != null && cfg.config != {};
          message = "You must provide services.villas.node.config.";
        }
      ];

      # configuration file indirection is needed to support reloading
      environment.etc."villas/node.json" = mkIf (builtins.isNull cfg.configPath) {
        source = villasNodeCfg;
      };

      systemd.services.villas-node = {
        description = "VILLASnode";
        after = ["network.target"];
        wantedBy = ["multi-user.target"];
        serviceConfig = {
          Type = "simple";
          ExecStart = let
            cfgPath =
              if isNull cfg.configPath
              then "/etc/villas/node.json"
              else cfg.configPath;
          in "${lib.getExe cfg.package} node ${cfgPath}";
        };
      };
    };
  }
