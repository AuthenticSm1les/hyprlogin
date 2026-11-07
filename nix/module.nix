self:
{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.programs.hyprlogin;
  greeterUser = config.services.greetd.settings.default_session.user;
  desktops = config.services.displayManager.sessionData.desktops;
in
{
  options.programs.hyprlogin = {
    enable = lib.mkEnableOption "hyprlogin, a greetd greeter running inside Hyprland";

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.hyprlogin;
      defaultText = lib.literalExpression "hyprlogin.packages.\${pkgs.stdenv.hostPlatform.system}.hyprlogin";
      description = "The hyprlogin package to use.";
    };

    hyprlandPackage = lib.mkOption {
      type = lib.types.package;
      default = if config.programs.hyprland.enable then config.programs.hyprland.package else pkgs.hyprland;
      defaultText = lib.literalExpression "config.programs.hyprland.package";
      description = "Hyprland package that runs the greeter session (provides start-hyprland).";
    };

    configFile = lib.mkOption {
      type = lib.types.nullOr lib.types.path;
      default = null;
      example = lib.literalExpression "./hyprlogin.conf";
      description = ''
        hyprlogin.conf to use as the base config. Defaults to the example
        config shipped with the package. The syntax is hyprlock's.
      '';
    };

    extraConfig = lib.mkOption {
      type = lib.types.lines;
      default = "";
      example = ''
        sessions {
            default_user = alice
        }
      '';
      description = "Lines appended to the config; they override the base config.";
    };

    greeterHyprlandConfig = lib.mkOption {
      type = lib.types.path;
      default = "${cfg.package}/share/hyprlogin/hyprland-greeter.lua";
      defaultText = lib.literalExpression "\"\${cfg.package}/share/hyprlogin/hyprland-greeter.lua\"";
      description = "Hyprland config for the greeter session (monitors, keyboard layout). It must start hyprlogin.";
    };
  };

  config = lib.mkIf cfg.enable {
    warnings = lib.optional (!config.programs.hyprland.enable) ''
      programs.hyprlogin: programs.hyprland is not enabled, so no Hyprland
      session will be offered and hyprctl may be missing from PATH.
    '';

    services.greetd = {
      enable = true;
      settings.default_session = {
        command = "${lib.getExe' cfg.hyprlandPackage "start-hyprland"} -- --config ${cfg.greeterHyprlandConfig}";
        user = lib.mkDefault "greeter";
      };
    };

    # Sessions come from the NixOS session registry, not /usr/share.
    environment.etc."hyprlogin/hyprlogin.conf".text = ''
      source = ${if cfg.configFile != null then cfg.configFile else "${cfg.package}/share/hyprlogin/examples/hyprlogin.conf"}

      sessions {
          wayland_path = ${desktops}/share/wayland-sessions
          x11_path = ${desktops}/share/xsessions
      }

      ${cfg.extraConfig}
    '';

    # Cache for sessions:remember_user / remember_session.
    systemd.tmpfiles.settings.hyprlogin."/var/cache/hyprlogin".d = {
      user = greeterUser;
      group = config.users.users.${greeterUser}.group or greeterUser;
      mode = "0750";
    };

    environment.systemPackages = [ cfg.package ];
  };
}
