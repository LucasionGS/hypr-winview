{
  description = "Official Hyprland Plugins";

  inputs = {
    hyprland.url = "github:hyprwm/Hyprland";
    nixpkgs.follows = "hyprland/nixpkgs";
    systems.follows = "hyprland/systems";
  };

  outputs = {
    self,
    hyprland,
    nixpkgs,
    systems,
    ...
  }: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);

    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = [
          self.overlays.hyprland-plugins
          hyprland.overlays.hyprland-packages
        ];
      });
  in {
    packages = eachSystem (system: {
      inherit
        (pkgsFor.${system}.hyprlandPlugins)
        winview
        ;
    });

    overlays = {
      default = self.overlays.hyprland-plugins;

      hyprland-plugins = final: prev: let
        inherit (final) callPackage;
      in {
        hyprlandPlugins =
          (prev.hyprlandPlugins
            or {})
          // {
            winview = callPackage ./winview {};
          };
      };
    };

    checks = eachSystem (system: self.packages.${system});

    devShells = eachSystem (system:
      with pkgsFor.${system}; {
        default = mkShell.override {stdenv = gcc14Stdenv;} {
          name = "hyprland-plugins";
          buildInputs = [hyprland.packages.${system}.hyprland];
          inputsFrom = [hyprland.packages.${system}.hyprland];
        };
      });
  };
}
