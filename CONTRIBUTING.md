# Contributing

`hyprlogin` is still a work in progress. The current goal is to make the greetd-based login flow stable before doing broader cleanup.

## Setup

1. Install the build dependencies listed in `README.md`.
2. Build locally:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

3. Test it under a real `greetd` + Hyprland greeter session.

## Current priorities

1. Fix the failed-auth UI path that can remain visually stuck on `Validating...`.
2. Reduce or remove temporary debug instrumentation once the auth flow is stable.
3. Continue cleaning upstream `hyprlock` naming leftovers where it improves maintainability.

## Guidelines

- Keep config syntax compatible with upstream `hyprlock` when possible.
- Prefer targeted fixes over broad renames while the login flow is still unstable.
- Update `todo.txt` when you confirm a persistent bug or resolve one.
- Keep `CHANGE_DOCUMENTATION.md` current when making meaningful repository-facing changes.
