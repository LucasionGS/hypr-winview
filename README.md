# winview

This repository contains the winview plugin for Windows - a window overview utility.

# Plugin list
 - winview -> adds a window overview utility

# Install
> [!IMPORTANT]
> hyprland-plugins only officially supports installation via `hyprpm`.
> hyprland-plugins follows hyprland-git and requires you to be on hyprland-git
> or tagged >= v0.33.1.

## Install with `hyprpm`

To install these plugins, from the command line run:
```bash
hyprpm update
```
Then add this repository:
```bash
hyprpm add https://github.com/hyprwm/hyprland-plugins
```
then enable the desired plugin with
```bash
hyprpm enable <plugin-name>
```

See the respective README's in the subdirectories for configuration options.

See [the plugins wiki](https://wiki.hyprland.org/Plugins/Using-Plugins/#installing--using-plugins) and `hyprpm -h` for more details.

## Install on Nix

To use these plugins, it's recommended that you are already using the
[Hyprland flake](https://github.com/hyprwm/Hyprland).
First, add this flake to your inputs:

```nix
inputs = {
  # ...
  hyprland.url = "github:hyprwm/Hyprland";
  hyprland-plugins = {
    url = "github:hyprwm/hyprland-plugins";
    inputs.hyprland.follows = "hyprland";
  };

  # ...
};
```

The `inputs.hyprland.follows` guarantees the plugins will always be built using
your locked Hyprland version, thus you will never get version mismatches that
lead to errors.

After that's done, you can use the plugins with the Home Manager module like
this:

```nix
{inputs, pkgs, ...}: {
  wayland.windowManager.hyprland = {
    enable = true;
    # ...
    plugins = [
      inputs.hyprland-plugins.packages.${pkgs.system}.winview
      # ...
    ];
  };
}
```

If you don't use Home Manager:

```nix
{ lib, pkgs, inputs, ... }:
with lib; let
  hyprPluginPkgs = inputs.hyprland-plugins.packages.${pkgs.system};
  hypr-plugin-dir = pkgs.symlinkJoin {
    name = "hyrpland-plugins";
    paths = with hyprPluginPkgs; [
      winview
      #...plugins
    ];
  };
in
{
  environment.sessionVariables = { HYPR_PLUGIN_DIR = hypr-plugin-dir; };
}
```

And in `hyprland.conf`

```hyprlang
# load all the plugins you installed
exec-once = hyprctl plugin load "$HYPR_PLUGIN_DIR/lib/libwinview.so"
```

# Contributing

Feel free to open issues and MRs with fixes.

If you want your plugin added here, contact vaxry beforehand.
