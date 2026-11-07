{
  description = "GPU-accelerated greetd greeter for Hyprland, forked from hyprlock";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";

    hyprgraphics = {
      url = "github:hyprwm/hyprgraphics";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprutils = {
      url = "github:hyprwm/hyprutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprlang = {
      url = "github:hyprwm/hyprlang";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprwayland-scanner = {
      url = "github:hyprwm/hyprwayland-scanner";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      systems,
      ...
    }@inputs:
    let
      inherit (nixpkgs) lib;
      eachSystem = lib.genAttrs (import systems);
      pkgsFor = eachSystem (
        system:
        import nixpkgs {
          localSystem.system = system;
          overlays = [ self.overlays.hyprlogin-with-deps ];
        }
      );
    in
    {
      overlays = import ./nix/overlays.nix { inherit inputs lib self; };

      packages = eachSystem (system: {
        default = self.packages.${system}.hyprlogin;
        inherit (pkgsFor.${system}) hyprlogin;
      });

      nixosModules = {
        default = self.nixosModules.hyprlogin;
        hyprlogin = import ./nix/module.nix self;
      };

      checks = eachSystem (
        system:
        self.packages.${system}
        // {
          # Evaluates the module on a minimal system and builds its generated files.
          module =
            let
              nixos = lib.nixosSystem {
                inherit system;
                modules = [
                  self.nixosModules.default
                  {
                    programs.hyprland.enable = true;
                    programs.hyprlogin = {
                      enable = true;
                      extraConfig = "sessions:default_user = alice";
                    };
                    boot.loader.grub.enable = false;
                    fileSystems."/".device = "none";
                    system.stateVersion = "26.05";
                  }
                ];
              };
            in
            pkgsFor.${system}.runCommand "hyprlogin-module-check" { } ''
              conf=${nixos.config.environment.etc."hyprlogin/hyprlogin.conf".source}
              grep -q "^source = /nix/store/.*/share/hyprlogin/examples/hyprlogin.conf" $conf
              grep -q "wayland_path = /nix/store/.*/share/wayland-sessions" $conf
              grep -q "sessions:default_user = alice" $conf
              echo '${builtins.unsafeDiscardStringContext nixos.config.services.greetd.settings.default_session.command}' | grep -q "/bin/start-hyprland -- --config /nix/store/.*/hyprland-greeter.lua"
              ${lib.getExe self.packages.${system}.hyprlogin} --version
              touch $out
            '';
        }
      );

      formatter = eachSystem (system: pkgsFor.${system}.nixfmt-tree);
    };
}
