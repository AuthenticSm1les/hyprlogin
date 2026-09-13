# Upstream divergence (hyprlock -> hyprlogin)

hyprlogin is a fork of [hyprlock](https://github.com/hyprwm/hyprlock) that
serves a different purpose: it is a **greetd greeter**. It runs inside a
Hyprland session as the `greeter` user and logs into a target user, rather
than locking an already-running desktop session.

Because of that, some hyprlock subsystems are dead code here and have been
**removed**. When porting future fixes from upstream hyprlock, do **not**
reintroduce these — skip or adapt the corresponding commits.

## Removed subsystems

### 1. Screencopy + DMABUF/GBM

hyprlock uses `wlr-screencopy` + `linux-dmabuf` + GBM to capture the desktop
behind the lock screen (the blurred-background effect). A greetd greeter has
no desktop behind it, so screencopy only ever captured the greeter's own
empty framebuffer.

Removed:

- `src/renderer/Screencopy.{cpp,hpp}` (deleted)
- `wlr-screencopy-unstable-v1` and `linux-dmabuf-v1` protocols (CMake
  `protocolnew` lines + generated files under `protocols/`)
- dmabuf feedback handlers, `gbm_find_render_node`, `createGBMDevice`,
  `addDmabufListener`/`removeDmabufListener`, and the `linux_dmabuf` /
  `wlr_screencopy_manager` registry binds in `src/core/hyprlock.cpp`
- `dma` state struct, `getScreencopy()`, and the `screencopy` field in
  `src/core/hyprlock.hpp`
- `enqueueScreencopyFrames`, `screencopyToTexture`,
  `resourceIDForScreencopy`, and `m_scFrames` in
  `src/renderer/AsyncResourceManager.{cpp,hpp}`
- the `background:path = "screenshot"` branch and `isScreenshot`/`scAsset`/
  `scResourceID`/`transformedScFB` in `src/renderer/widgets/Background.{cpp,hpp}`
- `general:screencopy_mode` config value
- `gbm` pkg-config dependency in `CMakeLists.txt` (`libdrm` is still used by
  `Framebuffer.cpp` — keep it)

### 2. Grace period

hyprlock's `--grace N` lets you auto-unlock without a password for N seconds
after locking. It is meaningless (and a security hazard) for a greeter.

Removed:

- `--grace` / deprecated `--immediate` CLI flags in `src/main.cpp`
- `m_tGraceEnds`, the constructor `gracePeriod` parameter, the `onKey`
  grace branch, and the cursor-motion grace-unlock branch in
  `src/core/Seat.cpp`

### 3. SIGUSR1 / SIGUSR2 external signals

hyprlock unlocks on `SIGUSR1` and force-re-renders on `SIGUSR2`. A greeter
must not auto-login on an external signal.

Removed:

- `handleUnlockSignal` (SIGUSR1) and `handleForceUpdateSignal` (SIGUSR2) and
  their `registerSignalAction` calls in `src/core/hyprlock.cpp`

`SIGRTMIN` (poll-thread termination, `handlePollTerminate`) is still used
internally — keep it.

## What was kept (not dead)

- `ext-session-lock-v1` / `LockSurface` — this is how the greeter renders
  full-screen surfaces; it holds the lock and releases it on login.
- `fractional-scale-v1`, `viewporter`, `cursor-shape-v1`, `tablet-v2` — used
  for rendering/input.
- All widgets (`background`, `input-field`, `label`, `image`, `shape`) and
  template variables (`$USER`, `$DESC`, `$TIME`, `$FAIL`, ...).
- Optional auth backends: `Greetd` (default), `Pam`, `Fingerprint` + `Dbus`.

## Guidance for porting future hyprlock commits

Skip (or manually adapt) upstream commits that touch any of:

- `Screencopy`, `wlr-screencopy`, `linux-dmabuf`, dmabuf/GBM feedback,
  `general:screencopy_mode`, or `background:path = "screenshot"`
- grace / `m_tGraceEnds` / `--grace` / `--immediate`
- `SIGUSR1` / `SIGUSR2` unlock or force-update

Rendering/EGL, widget, config-parsing, PAM, and fingerprint fixes generally
port cleanly.
