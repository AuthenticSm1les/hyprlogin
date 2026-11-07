{
  lib,
  inputs,
  self,
}:
let
  mkDate =
    longDate:
    (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);

  version = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in
{
  default = self.overlays.hyprlogin;

  hyprlogin-with-deps = lib.composeManyExtensions [
    inputs.hyprgraphics.overlays.default
    inputs.hyprlang.overlays.default
    inputs.hyprutils.overlays.default
    inputs.hyprwayland-scanner.overlays.default
    self.overlays.hyprlogin
  ];

  hyprlogin = final: prev: {
    hyprlogin = prev.callPackage ./default.nix {
      stdenv = prev.gcc16Stdenv or prev.stdenv;
      version =
        version
        + "+date="
        + (mkDate (self.lastModifiedDate or "19700101"))
        + "_"
        + (self.shortRev or "dirty");
      shortRev = self.sourceInfo.shortRev or "dirty";
    };
  };
}
