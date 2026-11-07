{
  lib,
  stdenv,
  cmake,
  pkg-config,
  cairo,
  gtest,
  libdrm,
  libGL,
  libxkbcommon,
  hyprgraphics,
  hyprlang,
  hyprutils,
  hyprwayland-scanner,
  pango,
  wayland,
  wayland-protocols,
  wayland-scanner,
  version ? "git",
  shortRev ? "",
}:
stdenv.mkDerivation {
  pname = "hyprlogin";
  inherit version;

  src = lib.cleanSource ../.;

  # The shipped greetd and greeter Hyprland configs assume /usr; point them
  # at this package instead.
  postPatch = ''
    substituteInPlace assets/greetd-config.toml \
      --replace-fail "/usr/share/hyprlogin" "$out/share/hyprlogin"
    substituteInPlace assets/hyprland-greeter.lua \
      --replace-fail '"hyprlogin ' "\"$out/bin/hyprlogin "
  '';

  nativeBuildInputs = [
    cmake
    pkg-config
    hyprwayland-scanner
    wayland-scanner
  ];

  buildInputs = [
    cairo
    libdrm
    libGL
    libxkbcommon
    hyprgraphics
    hyprlang
    hyprutils
    pango
    wayland
    wayland-protocols
  ];

  cmakeFlags = lib.mapAttrsToList lib.cmakeFeature {
    HYPRLOGIN_COMMIT = shortRev;
    HYPRLOGIN_VERSION_COMMIT = ""; # Intentionally left empty (hyprlogin --version always prints the commit)
  } ++ [ (lib.cmakeBool "BUILD_TESTING" true) ];

  doCheck = true;
  nativeCheckInputs = [ gtest ];

  meta = {
    homepage = "https://github.com/AuthenticSm1les/hyprlogin";
    description = "GPU-accelerated greetd greeter for Hyprland, forked from hyprlock";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
    mainProgram = "hyprlogin";
  };
}
