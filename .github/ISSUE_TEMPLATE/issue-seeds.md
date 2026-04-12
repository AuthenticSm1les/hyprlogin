# Suggested Initial Issues

Use these as the initial issue tracker seed after pushing the repository.

## 1. Failed-auth UI can remain stuck on `Validating...`

- Type: bug
- Summary:
  Wrong-password failure can still leave the greeter visually stuck on `Validating...` even though backend debug shows the greetd transaction and local failure reset complete.
- Why it matters:
  This is the main known blocker for basic login UX.

## 2. Internal rename cleanup

- Type: maintenance
- Summary:
  The fork still contains internal `hyprlock` naming and structure that should be cleaned up where it improves maintainability.
- Why it matters:
  The current mixed naming makes the codebase harder to reason about and contributes to confusion while debugging.

## 3. Debug instrumentation cleanup

- Type: maintenance
- Summary:
  The repository currently carries heavy greetd/UI debug instrumentation that should be reduced once the auth-flow bug is fixed.
- Why it matters:
  The extra logging is useful right now but should not become the long-term default code shape.

## 4. Optional Nix support reintroduction

- Type: feature
- Summary:
  Nix support was removed because the inherited files still targeted upstream `hyprlock`. Reintroduce it only if it is intentionally migrated to `hyprlogin`.
- Why it matters:
  Nix support can be useful later, but stale packaging metadata should not remain in the repository.
