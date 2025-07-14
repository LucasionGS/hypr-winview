{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin hyprland {
  pluginName = "winview";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/LucasionGS/hypr-winview";
    description = "Windows overview plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
