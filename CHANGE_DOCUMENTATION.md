# Change Documentation

## Project State

- `hyprlogin` is a work-in-progress fork of `hyprlock`.
- The goal of the fork is to act as a `greetd` greeter instead of a session lockscreen.
- Repository versioning is intentionally reset to `0.0.0` until the project is ready for a real release.
- The main known unresolved problem is the failed-auth UI path tracked in `todo.txt`.

## What Changed And Why

### Project identity

- `CMakeLists.txt`
  - Changed:
    - Renamed the project and installed binary to `hyprlogin`.
    - Added install rules for greeter sample files.
  - Why:
    - The project needed its own identity and install surface instead of pretending to still be `hyprlock`.

- `src/main.cpp`
  - Changed:
    - Updated command-line version and help output to `hyprlogin`.
    - Added config-driven debug activation.
  - Why:
    - The executable output should match the project name.
    - Greeter debugging should be available without changing the launch command.

### Greetd login backend

- `src/auth/Auth.hpp`
  - Changed:
    - Added a greetd backend type.
  - Why:
    - The auth system needed a dedicated login backend.

- `src/auth/Auth.cpp`
  - Changed:
    - Registered the greetd backend.
  - Why:
    - The greeter must route authentication through greetd rather than the original unlock path.

- `src/auth/Greetd.hpp`
  - Changed:
    - Added the greetd backend interface and response structures.
  - Why:
    - The project needs explicit IPC handling for the greeter flow.

- `src/auth/Greetd.cpp`
  - Changed:
    - Implemented the greetd login flow.
    - Added session start and cancellation handling.
    - Added failure reset handling and debug tracing.
  - Why:
    - A login greeter needs a transaction-based backend instead of lockscreen-style local auth.
    - The backend needed enough logging to diagnose auth and state-transition bugs.

### Config and samples

- `src/config/ConfigManager.cpp`
  - Changed:
    - Added greeter-specific config options.
    - Added active-config and fallback-example resolution.
  - Why:
    - The greeter needs session, prompt, and debug settings that the original project did not have.
    - Users need a predictable system config contract.

- `assets/example.conf`
  - Changed:
    - Reworked the sample into a greeter-oriented config.
  - Why:
    - The shipped sample should represent current greeter behavior, not lockscreen behavior.

- `assets/greetd-config.toml`
  - Changed:
    - Added a sample greeter launcher config.
  - Why:
    - Users need a working reference for integrating the greeter with the login manager.

- `assets/hyprland-greeter.conf`
  - Changed:
    - Added a sample compositor-side greeter config.
  - Why:
    - Users need a working reference for launching the greeter session itself.

- `README.md`
  - Changed:
    - Rewrote the documentation around the greeter model.
    - Added current-state notes and setup guidance.
    - Added a `Known Issues` section.
    - Added a short contributor-oriented setup section.
  - Why:
    - The repository should describe what it actually is today and how it is meant to be used.
    - People helping debug the project need to see the main blockers and a minimal way to get started quickly.

- `CONTRIBUTING.md`
  - Changed:
    - Added a short contributor guide for building and testing the fork.
  - Why:
    - The repository is being shared for debugging help, so contributors need a direct entry point.

### Greeter state and session handling

- `src/core/hyprlock.hpp`
  - Changed:
    - Added greeter-visible state for prompt text, username, and session selection.
  - Why:
    - The original lockscreen state model was too limited for a multi-step login flow.

- `src/core/hyprlock.cpp`
  - Changed:
    - Added session discovery and selection.
    - Added prompt and username state handling.
    - Added UI-side debug logging and immediate state rendering.
  - Why:
    - A greeter needs visible login/session state and clear transitions.
    - The unresolved failed-auth bug required direct visibility into UI state changes.

### Widget behavior

- `src/renderer/widgets/IWidget.cpp`
  - Changed:
    - Added greeter-specific dynamic text substitutions.
  - Why:
    - Labels and prompts need access to login/session state from config.

- `src/renderer/widgets/PasswordInputField.hpp`
  - Changed:
    - Extended the widget state for username input, password input, and debug tracking.
  - Why:
    - The original widget only modeled password-style input.

- `src/renderer/widgets/PasswordInputField.cpp`
  - Changed:
    - Added separate username/password placeholder behavior.
    - Added visible username rendering and masked password rendering.
    - Added widget-level debug logging.
  - Why:
    - The greeter needs two distinct input modes.
    - The failed-auth bug appears to involve widget/render state, not only backend state.

- `src/renderer/widgets/Label.cpp`
  - Changed:
    - Added clickable internal actions.
    - Added dynamic refresh for force-update labels.
  - Why:
    - Prompt and session labels were able to hold stale rendered text.

- `src/renderer/widgets/Image.cpp`
  - Changed:
    - Added clickable internal actions.
  - Why:
    - Internal greeter actions should work on image widgets too.

- `src/renderer/widgets/Shape.cpp`
  - Changed:
    - Added clickable internal actions.
  - Why:
    - Internal greeter actions should work consistently across widget types.

- `src/renderer/Renderer.cpp`
  - Changed:
    - Contains in-progress diagnostic changes related to render/unlock sequencing.
  - Why:
    - The project still needs render-path debugging while stabilization is ongoing.

### Helper code

- `src/helpers/MiscFunctions.hpp`
  - Changed:
    - Added helper declarations for internal greeter actions and command checks.
  - Why:
    - The greeter uses internal actions that did not exist in the original project.

- `src/helpers/MiscFunctions.cpp`
  - Changed:
    - Added helper logic for greeter actions and command validation.
    - Updated temporary naming from the old project name to `hyprlogin`.
  - Why:
    - Internal greeter behavior needed shared helper support.
    - Temporary naming should follow the new project identity.

### Repository hygiene

- `.gitignore`
  - Changed:
    - Added ignores for generated build, package, and log outputs.
  - Why:
    - Generated files should not pollute the tracked repository state.

- `.github/workflows/nix.yml`
  - Changed:
    - Removed the stale Nix workflow.
  - Why:
    - Nix support was removed from the repository, so the inherited workflow no longer matched the actual project surface.

- `.github/ISSUE_TEMPLATE/issue-seeds.md`
  - Changed:
    - Added a seed list for the first GitHub issues to open.
  - Why:
    - The repository is being pushed specifically to get debugging help, so the initial issue tracker should already point collaborators at the right problems.

- `flake.nix`, `flake.lock`, `nix/`
  - Changed:
    - Removed the stale Nix packaging files and directory.
  - Why:
    - They were never migrated from `hyprlock`, still described/package the old project, and added maintenance surface without providing value to the current fork.

- `pam/hyprlock`
  - Changed:
    - Removed the stale PAM service file.
  - Why:
    - It still targeted the original lockscreen project name and was not used by the current greetd-based greeter flow.

- `PKGBUILD`, `.SRCINFO`, `hyprlogin.install`
  - Changed:
    - Removed Arch packaging files from the repository.
  - Why:
    - Arch packaging metadata belongs in the AUR packaging repository, not in the main source repository.

### Bug tracking

- `todo.txt`
  - Changed:
    - Added a persistent bug entry for the unresolved failed-auth UI issue.
  - Why:
    - The current blocker should remain visible across sessions and handoffs.
