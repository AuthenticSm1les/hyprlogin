# Upstream divergence (hyprlock -> hyprlogin)

hyprlogin is a fork of [hyprlock](https://github.com/hyprwm/hyprlock) that
serves a different purpose: it is a **greetd greeter**. It runs inside a
Hyprland session as the `greeter` user and logs into a target user, rather
than locking an already-running desktop session.

The fork is kept close to upstream so hyprlock fixes can keep being ported.
Two rules make that work:

1. **hyprlogin code lives in `src/greeter/`.** Everything else is hyprlock.
2. **Upstream files only get hooks.** Where hyprlock code has to call into
   the greeter, it is a single call on `g_pGreeter`, marked `// hyprlogin`.
   Don't reformat, rename or restructure upstream code around it.

## Layout

| Path | Contents |
|---|---|
| `src/greeter/Greeter.*` | `CGreeter` (`g_pGreeter`): prompt, target user, session selection, default and remembered user, Escape/Tab/arrow keys, `hyprlogin:*` onclick commands, `$GREETD_*` variables, `general:exit_command`, and registration of all hyprlogin config options |
| `src/greeter/Sessions.*` | `.desktop` session discovery, matching and session environment |
| `src/greeter/Greetd.*` | greetd IPC auth backend, the only `IAuthImplementation` |
| `src/greeter/RememberCache.*` | `lastuser` / `lastsession` cache in `/var/cache/hyprlogin` |
| `src/greeter/Logging.*` | `--debug` / `--log-file` flags and their `HYPRLOGIN_DEBUG` / `HYPRLOGIN_LOG_FILE` environment variables |
| everything else | hyprlock, minus the removed subsystems below |

### Hooks in upstream files

List them with:

```sh
grep -rn '// hyprlogin' src --exclude-dir=greeter
```

| File | Hook |
|---|---|
| `main.cpp` | `GreeterLogging::registerArgs` / `setup` before the config is loaded; create `g_pGreeter` and call `init()` after |
| `helpers/Log.hpp` | `debug()`, `setDebug()`, and `setLogFd()` + `writeLogFile()` (the `--log-file` output, also written with `--quiet`) |
| `config/ConfigManager.cpp` | `CGreeter::registerConfig(m_config)` before `commence()` |
| `core/hyprlock.cpp` | `handleKeySym` gives keys to `g_pGreeter->handleKeySym` first; `releaseSessionLock` calls `onSessionLockReleased` |
| `core/hyprlock.hpp` | `getInputBuffer` / `setInputBuffer` (the buffer holds the username or the password) |
| `widgets/IWidget.cpp` | `formatString` calls `g_pGreeter->replaceVariables` |
| `widgets/{Label,Image,Shape}.cpp` | `onClick` tries `g_pGreeter->handleInternalCommand` before spawning |
| `widgets/PasswordInputField.cpp` | `inputHidden` comes from `g_pGreeter->isSecretInput()` |

Two widget features are still implemented inside upstream widget files
because they are rendering code: the visible username text in
`PasswordInputField` (`updateInputState`, `inputText`) and
`label:hide_when_empty` plus live re-formatting in `CLabel::draw`.

## Removed subsystems

These are dead or wrong for a greeter. Don't port upstream commits that
only touch them; `scripts/upstream-status.sh` reports such commits as
`n/a` or `skipped`.

### Screencopy + DMABUF/GBM

hyprlock captures the desktop behind the lock screen for its blurred
background. A greeter has no desktop behind it.

Removed: `src/renderer/Screencopy.*`, `mtx.hpp`, `Shared.hpp`, the
`wlr-screencopy-unstable-v1` and `linux-dmabuf-v1` protocols, dmabuf
feedback and GBM device code in `hyprlock.cpp`, the screencopy paths in
`AsyncResourceManager` and `Background`, `background:path = screenshot`,
`general:screencopy_mode`, the `wl_shm` bind, and the `gbm` dependency.

### PAM and fingerprint backends

hyprlock authenticates the locked user in-process. In a greeter both
backends were wrong: `CPam` authenticated the *greeter* user, `CFingerprint`
verified the greeter user's prints over fprintd, and on success both only
exited the greeter without starting a session. greetd owns PAM, so
fingerprint login works through `pam_fprintd` in `/etc/pam.d/greetd`,
which the greetd backend handles.

Removed: `src/auth/Pam.*`, `src/auth/Fingerprint.*`, `src/core/Dbus.*`
and the D-Bus fd in the event loop, `auth:*` options, the `$PAMFAIL`,
`$PAMPROMPT`, `$FPRINTFAIL`, `$FPRINTPROMPT`, `$USER` and `$DESC` label
variables, and the `pam` and `sdbus-c++` dependencies.

### Grace period and signals

`--grace` auto-unlocks without a password, and `SIGUSR1` unlocks on an
external signal: both are security holes in a greeter. `SIGUSR2` forced a
label refresh, which nothing sends to a greeter.

`SIGRTMIN` (poll-thread termination) is still used internally.

## Porting workflow

```sh
git remote add upstream https://github.com/hyprwm/hyprlock.git   # once
git config remote.upstream.tagOpt --no-tags                       # once: hyprlock tags clash with ours
git config rerere.enabled true                                    # once
scripts/upstream-status.sh
```

The script test-picks every upstream commit since the fork onto `HEAD` in
a throwaway worktree and prints one of:

- `ported`: already here (same patch, or a `cherry picked from commit` trailer)
- `skipped`: listed in `scripts/upstream-skip` with a reason
- `n/a`: only touches removed subsystems, CI, Nix or `VERSION`
- `clean`: apply with `git cherry-pick -x <sha>`
- `conflict`: port by hand; the conflicting files are listed

Always keep the `(cherry picked from commit <sha>)` trailer, also on manual
ports (add it to the message yourself), so the script recognises them.
When you decide not to port a commit, add it to `scripts/upstream-skip`.
