# hyprlogin
Hyprland's simple, yet multi-threaded and GPU-accelerated `greetd` greeter utility.

## Features
 - Runs inside a Hyprland greeter session managed by `greetd`
 - Speaks the `greetd` IPC protocol over `GREETD_SOCK`
 - Supports username, password, and session selection flows
 - Support for fractional-scale
 - Fully GPU accelerated
 - Multi-threaded resource acquisition
 - Custom backgrounds, gradients, blur, animations, shadows, etc.
 - Some of Hyprland's eyecandy: gradient borders, blur, animations, shadows, etc.
 - and more...

## How it looks

![](https://i.ibb.co/8Bd98BP/20240220-00h12m46s.png)

## Status
This fork repurposes `hyprlock` into a graphical greeter. It is no longer a session locker, and it should be run under `greetd` inside a dedicated Hyprland greeter session.

Current state:

- work in progress
- versioning reset to `0.0.0` until the project is ready for a real release
- builds and packages locally
- includes a working `greetd` backend, session discovery, default user/session selection, and sample configs
- still has at least one known unresolved bug in the failed-auth UI path; see `todo.txt`

## Known Issues

- Failed authentication can still leave the greeter visually stuck on `Validating...` even though backend debug shows the greetd transaction and local failure reset complete.
- Internal naming is still mixed in places because the fork is not fully renamed away from the upstream `hyprlock` structure yet.
- Debug instrumentation is still heavier than it should be for a stable release.

Typical `greetd` setup:

```toml
[default_session]
command = "start-hyprland --config /etc/hyprlogin/hyprland.conf"
user = "greeter"
```

Example Hyprland config:

```ini
exec-once = hyprlogin
```

Installed sample files:

- `/usr/share/hyprlogin/examples/hyprlogin.conf`
- `/usr/share/hyprlogin/hyprland-greeter.conf`
- `/usr/share/hyprlogin/greetd-config.toml`

## Install and Use

Arch Linux:

```sh
paru -S hyprlogin-git
```

Main paths after install:

- `/usr/bin/hyprlogin`
- `/etc/hyprlogin/hyprlogin.conf`
- `/usr/share/hyprlogin/examples/hyprlogin.conf`
- `/usr/share/hyprlogin/hyprland-greeter.conf`
- `/usr/share/hyprlogin/greetd-config.toml`

Minimal setup:

1. Install and enable `greetd`.
2. Point `/etc/greetd/config.toml` at the shipped greeter session:

```toml
[terminal]
vt = 1

[default_session]
command = "start-hyprland -- --config /usr/share/hyprlogin/hyprland-greeter.conf"
user = "greeter"
```

3. Copy the sample config and customize it:

```sh
sudo install -Dm644 /usr/share/hyprlogin/examples/hyprlogin.conf /etc/hyprlogin/hyprlogin.conf
```

4. Restart `greetd` or reboot and test the greeter from a fresh login session.

Config resolution:

- `hyprlogin -c /path/to/config` uses the explicit path
- otherwise `hyprlogin` first looks for `/etc/hyprlogin/hyprlogin.conf`
- if that file does not exist, it falls back to `/usr/share/hyprlogin/examples/hyprlogin.conf`

Config compatibility:

- the config grammar stays aligned with `hyprlock`
- existing `hyprlock` widget/category syntax remains valid
- `hyprlogin` only adds extra variables and optional internal actions; it does not require a forked config format
- this keeps future syncs with upstream `hyprlock` mostly constrained to behavior and rendering code rather than a divergent config parser
- optional greeter-only additions include `sessions:default_user`, `sessions:default_session`, `input-field:placeholder_text_username`, `general:debug_mode`, and `general:debug_log_path`

The default `hyprlogin` config supports:

- `Enter` to submit the current username or password prompt
- `Tab`, `Shift+Tab`, left, and right arrows to cycle sessions
- clickable session switching via `hyprlogin:session_prev` and `hyprlogin:session_next`
- distinct username and password placeholders while keeping the visual style close to upstream `hyprlock`

Session discovery behavior:

- Wayland sessions are read from `/usr/share/wayland-sessions`
- X11 sessions are read from `/usr/share/xsessions`
- hidden and non-display desktop entries are skipped
- `TryExec` and `Exec` are checked for availability before a session is shown
- the greeter exports `XDG_SESSION_TYPE`, `XDG_SESSION_DESKTOP`, `DESKTOP_SESSION`, and `XDG_CURRENT_DESKTOP` when possible
- `sessions:default_session` can preselect a session by display name, command, desktop file path, or desktop file basename
- `sessions:default_user` prefills the username field on startup and after failed authentication resets
- `general:debug_mode = true` enables verbose startup, greetd transaction, and auth-state logs without needing the CLI `--verbose` flag
- `general:debug_log_path = /tmp/hyprlogin-debug.log` appends the same greetd debug trace to a file so failed login attempts can be inspected after the greeter returns

## Building

### Deps
You need the following dependencies

- cairo
- hyprgraphics
- hyprlang
- hyprutils
- hyprwayland-scanner
- mesa (required is libgbm, libdrm and the opengl runtime)
- pam
- pango
- sdbus-cpp (>= 2.0.0)
- wayland-client
- wayland-protocols
- xkbcommon

Sometimes distro packages are missing required development files.
Such distros usually offer development versions of library package - commonly suffixed with `-devel` or `-dev`.

### Building

Building:
```sh
cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build
cmake --build ./build --config Release --target hyprlogin -j`nproc 2>/dev/null || getconf _NPROCESSORS_CONF`
```

Installation:
```sh
sudo cmake --install build
```

## Contributing

If you want to help debug or improve the fork:

1. Build it locally with CMake.
2. Run it under a real `greetd` + Hyprland greeter session rather than from an already logged-in desktop session.
3. Reproduce issues with `general:debug_mode` enabled in the config.
4. Read `todo.txt` for currently tracked persistent bugs.
5. Keep the config format compatible with upstream `hyprlock` where possible.
6. Prefer fixing stale upstream leftovers intentionally rather than renaming everything blindly.
7. Use `CHANGE_DOCUMENTATION.md` as the high-level project change summary.

## Repository Notes

- `CHANGE_DOCUMENTATION.md` is the sanitized project change summary
- `todo.txt` contains the current persistent bug list
- generated build/package outputs are intended to stay untracked
