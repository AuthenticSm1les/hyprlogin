#!/usr/bin/env bash
# List hyprlock commits not yet in hyprlogin and whether each would apply.
#
#   scripts/upstream-status.sh [base]
#
# base defaults to the merge base with upstream/main. Each commit is
# test-picked onto HEAD in a throwaway worktree; your checkout is untouched.
#
#   ported    already here (same patch, or "cherry picked from commit <sha>")
#   skipped   listed in scripts/upstream-skip with the reason
#   n/a       only touches things hyprlogin removed (see HYPRLOCK-DIVERGENCE.md)
#   clean     applies as-is: git cherry-pick -x <sha>
#   conflict  needs a manual port; the conflicting files are listed
set -euo pipefail

REMOTE=upstream
URL=https://github.com/hyprwm/hyprlock.git

# Paths that only exist upstream, or that hyprlogin maintains separately (its
# own nix/ and flake); commits touching nothing else are skipped.
NA_REGEX='^(src/renderer/Screencopy\.|src/renderer/mtx\.hpp|src/renderer/Shared\.hpp|src/auth/(Pam|Fingerprint)\.|src/core/Dbus\.|protocols/wlr-screencopy|nix/|flake\.(nix|lock)|\.github/|VERSION$|hyprlock\.pam$|pam/)'

cd "$(git rev-parse --show-toplevel)"

git remote get-url "$REMOTE" >/dev/null 2>&1 || git remote add "$REMOTE" "$URL"
git fetch --quiet "$REMOTE"

BASE=${1:-$(git merge-base HEAD "$REMOTE/main")}
PORTED=$(git log --format=%B HEAD | sed -n 's/.*cherry picked from commit \([0-9a-f]\{7,40\}\).*/\1/p')
SKIPS=$(grep -v '^#' scripts/upstream-skip 2>/dev/null || true)

WT=$(mktemp -d)
trap 'git worktree remove --force "$WT" >/dev/null 2>&1 || rm -rf "$WT"' EXIT
git worktree add --quiet --detach "$WT" HEAD

declare -A EQUIV=()
while read -r sign sha; do
    [[ $sign == - ]] && EQUIV[$sha]=1
done < <(git cherry HEAD "$REMOTE/main" "$BASE")

for sha in $(git rev-list --reverse --no-merges "$BASE..$REMOTE/main"); do
    short=$(git rev-parse --short "$sha")
    subject=$(git log -1 --format=%s "$sha")

    if [[ -n ${EQUIV[$sha]:-} ]] || grep -q "^${sha:0:7}" <<<"$PORTED"; then
        status=ported
    elif reason=$(grep "^${sha:0:7}" <<<"$SKIPS"); then
        status="skipped: ${reason#* }"
    elif ! git diff-tree --no-commit-id --name-only -r "$sha" | grep -qvE "$NA_REGEX"; then
        status=n/a
    else
        git -C "$WT" reset --quiet --hard HEAD
        if git -C "$WT" cherry-pick --no-commit "$sha" >/dev/null 2>&1; then
            if git -C "$WT" diff --cached --quiet; then status=ported; else status=clean; fi
        else
            files=$(git -C "$WT" diff --name-only --diff-filter=U | tr '\n' ' ')
            status="conflict: ${files% }"
            git -C "$WT" cherry-pick --abort >/dev/null 2>&1 || true
        fi
        git -C "$WT" reset --quiet --hard HEAD
    fi

    printf '%s  %-8s  %s\n' "$short" "${status%%:*}" "$subject"
    if [[ $status == *:* ]]; then printf '           %s\n' "${status#*: }"; fi
done
