# hyprlogin

A GPU-accelerated graphical greeter for [greetd](https://sr.ht/~kennylevinsen/greetd/), built on Hyprland.

hyprlogin is a fork of [hyprlock](https://github.com/hyprwm/hyprlock), repurposed from a screen locker into a login screen. It runs inside a minimal Hyprland session started by greetd, authenticates over the greetd IPC protocol, and launches the session you pick.

> [!NOTE]
> hyprlogin is a work in progress. It is usable day to day, but expect rough edges and verbose logging.

![hyprlogin screenshot](https://i.ibb.co/8Bd98BP/20240220-00h12m46s.png)

## Features

- Native greetd backend (username, password and session selection)
- Discovers Wayland and X11 sessions automatically
- Remembers the last user and session, tuigreet style (`sessions:remember_user`, `sessions:remember_session`)
- Preselected user and session (`sessions:default_user`, `sessions:default_session`)
- hyprlock-compatible config: existing hyprlock themes work unchanged
- Fully GPU-accelerated, multi-threaded resource loading, fractional scaling
- Hyprland eye candy: blur, gradients, shadows, animations
- Optional fingerprint authentication

## Requirements

- [greetd](https://sr.ht/~kennylevinsen/greetd/)
- [Hyprland](https://hypr.land) (provides `start-hyprland` and `hyprctl`)

## Installation

### Arch Linux (AUR)

```sh
sudo pacman -S --needed greetd hyprland
paru -S hyprlogin-git
```

### From source

Install the build dependencies (on Arch, these are all in the official repos):

| Dependency | Notes |
|---|---|
| cmake ≥ 3.27, pkgconf, a C++23 compiler | |
| hyprlang ≥ 0.6.0 | |
| hyprutils ≥ 0.11.0 | |
| hyprgraphics ≥ 0.1.6 | |
| hyprwayland-scanner | |
| wayland, wayland-protocols ≥ 1.35 | |
| mesa | EGL, libdrm |
| cairo, pango | |
| libxkbcommon | |
| sdbus-cpp ≥ 2.0.0 | |
| pam | |

On distros that split development headers, install the `-dev` / `-devel` variants.

Then build and install:

```sh
git clone https://github.com/AuthenticSm1les/hyprlogin.git
cd hyprlogin
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

## Setup

After installing, these files are available:

| Path | Purpose |
|---|---|
| `/usr/bin/hyprlogin` | The greeter |
| `/usr/share/hyprlogin/greetd-config.toml` | Ready-to-use greetd config |
| `/usr/share/hyprlogin/hyprland-greeter.lua` | Hyprland config for the greeter session |
| `/usr/share/hyprlogin/examples/hyprlogin.conf` | Example greeter theme |

### 1. Configure greetd

Back up your current greetd config and replace it with the one shipped by hyprlogin:

```sh
sudo cp /etc/greetd/config.toml /etc/greetd/config.toml.bak
sudo cp /usr/share/hyprlogin/greetd-config.toml /etc/greetd/config.toml
```

It contains:

```toml
[terminal]
vt = 1

[default_session]
command = "start-hyprland -- --config /usr/share/hyprlogin/hyprland-greeter.lua"
user = "greeter"
```

To customise the greeter's Hyprland session (monitors, keyboard layout, …), copy `hyprland-greeter.lua` somewhere like `/etc/hyprlogin/`, edit it, and update the `--config` path above.

### 2. Create your greeter config (optional)

hyprlogin works out of the box with the example config. To customise it:

```sh
sudo install -Dm644 /usr/share/hyprlogin/examples/hyprlogin.conf /etc/hyprlogin/hyprlogin.conf
sudoedit /etc/hyprlogin/hyprlogin.conf
```

### 3. Enable greetd

Disable any display manager you currently use (for example `gdm`, `sddm` or `lightdm`), then enable greetd:

```sh
sudo systemctl disable sddm.service   # replace with your current display manager
sudo systemctl enable greetd.service
```

### 4. Reboot

```sh
reboot
```

If the greeter does not come up, switch to another TTY with <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F2</kbd>, log in, and see [Troubleshooting](#troubleshooting).

## Usage

| Key | Action |
|---|---|
| <kbd>Enter</kbd> | Submit username / password |
| <kbd>Tab</kbd> / <kbd>Shift</kbd>+<kbd>Tab</kbd>, <kbd>←</kbd> / <kbd>→</kbd> | Cycle sessions |
| <kbd>Esc</kbd> on an empty password field | Return to username entry |
| <kbd>Esc</kbd>, <kbd>Ctrl</kbd>+<kbd>U</kbd>, <kbd>Ctrl</kbd>+<kbd>Backspace</kbd> | Clear the current field |

Clicking the session label in the example config also switches sessions.

### Command-line options

```
-c, --config <path>   Use a specific config file
-v, --verbose         Verbose logging
-q, --quiet           Disable logging
    --no-fade-in      Disable the fade-in animation
    --display <name>  Wayland display to connect to
-V, --version         Print version and exit
-h, --help            Show help
```

## Configuration

### Config file location

hyprlogin uses the first config it finds:

1. The path given with `-c` / `--config`
2. `/etc/hyprlogin/hyprlogin.conf`
3. `/usr/share/hyprlogin/examples/hyprlogin.conf`

### Syntax

The config format is identical to hyprlock's, so the [hyprlock wiki](https://wiki.hypr.land/Hypr-Ecosystem/hyprlock/) applies to all widgets (`background`, `input-field`, `label`, `image`, `shape`) and general options. hyprlogin adds the following:

| Option | Default | Description |
|---|---|---|
| `sessions:default_user` | *(empty)* | Prefill the username field |
| `sessions:default_session` | *(empty)* | Preselect a session by name, command, `.desktop` path or `.desktop` file name (e.g. `hyprland.desktop`) |
| `sessions:remember_user` | `true` | Preselect the user who last logged in successfully. Overrides `sessions:default_user` once something is remembered |
| `sessions:remember_session` | `true` | Preselect the session last started. Overrides `sessions:default_session`; falls back to it if the remembered session no longer exists |
| `sessions:remember_dir` | `/var/cache/hyprlogin` | Where `lastuser` and `lastsession` are stored. Must be writable by the greeter user (the package creates it via `tmpfiles.d`) |
| `sessions:wayland_path` | `/usr/share/wayland-sessions` | Where Wayland sessions are discovered |
| `sessions:x11_path` | `/usr/share/xsessions` | Where X11 sessions are discovered |
| `input-field:placeholder_text_username` | `<i>Input Username</i>` | Placeholder shown while asking for the username |
| `label:hide_when_empty` | `false` | Hide a label whose text resolves to an empty string |
| `general:debug_mode` | `false` | Verbose startup, greetd and auth logging |
| `general:debug_log_path` | `/tmp/hyprlogin-debug.log` | File the debug log is appended to |

Extra variables usable in labels:

| Variable | Value |
|---|---|
| `$GREETD_USER` | Username being logged in |
| `$GREETD_SESSION` | Currently selected session |
| `$GREETD_PROMPT` | Current greetd prompt |

Clickable actions (for `onclick`): `hyprlogin:session_next`, `hyprlogin:session_prev`.

### Sessions

Sessions are read from the `.desktop` files in the Wayland and X11 session directories. Hidden entries and entries whose `TryExec` / `Exec` binary is missing are skipped. If `startx` is installed, a "Default X11 Session" entry is added as well.

### File permissions

The greeter runs as the `greeter` user. Config files and wallpapers must be readable by that user. Files inside your home directory are usually **not** readable (home directories are typically mode `700`), so put wallpapers in a shared location such as `/usr/share/backgrounds/`. If the wallpaper can't be read, the background falls back to its configured color and a warning is logged.

## Troubleshooting

1. Enable debug logging in `/etc/hyprlogin/hyprlogin.conf`:

   ```ini
   general {
       debug_mode = true
   }
   ```

2. Reboot (or `sudo systemctl restart greetd` from a TTY) and try to log in.
3. Read the logs:

   ```sh
   cat /tmp/hyprlogin-debug.log
   journalctl -b -u greetd
   ```

Always test from a real greetd session. Running `hyprlogin` from inside an already logged-in desktop will not work, because greetd sets `GREETD_SOCK` only for its greeter.

To go back to your previous display manager, restore `/etc/greetd/config.toml.bak` or disable `greetd.service` and re-enable your old one.

## Contributing

Bug reports and pull requests are welcome. When reporting an issue, include the output from `debug_mode` and `journalctl -b -u greetd`.

When porting changes from upstream hyprlock, read [HYPRLOCK-DIVERGENCE.md](HYPRLOCK-DIVERGENCE.md) first. Some subsystems have been removed on purpose and should not come back. Keep the config format compatible with hyprlock.

## License

BSD 3-Clause. See [LICENSE](LICENSE).
