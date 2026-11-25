# KuroCat

Desktop cat pet that reacts to keyboard and mouse input. Three **independent native ports** (macOS Swift, Windows C++17, Linux C++17) — no shared runtime code. Docs and user-facing strings are mostly Chinese; `README.en.md` is the English README.

## Layout

- `Sources/` — macOS (Swift, no Xcode project). Skin model (`SkinSpec`, `SkinLibrary`) lives in `CatPhysics.swift`, not a separate file.
- `windows/src/`, `linux/src/` — each has `KeyMap` / `InputEngine` / `CatPhysics` / `CatRenderer` / `SkinLibrary` / `main.cpp`.
- `shared/` — only Windows + Linux share it: `SkinSpec.hpp` (`.kurocat` parser), `CoatPatterns.hpp`.
- `skins/` — built-in coats as `.kurocat`; `docs/skin-format.md` is the format spec.
- `scripts/`, `tests/` — regression checks (see below).

## Build

- macOS: `./build.sh` → universal `KuroCat.app` (arm64 + x86_64 via `swiftc`, then `lipo` and ad-hoc/dev codesign).
- Windows: `cd windows && build.bat` (or CMake).
- Linux: `cd linux && ./build.sh` (needs GTK3, Cairo, X11, Xi dev packages); `./build_deb.sh` for the `.deb`.
- Release: pushing a `v*` tag runs `.github/workflows/build.yml` for all platforms.

## Checks (none launch the app)

- `bash scripts/check-skins.sh` — macOS renderer vs. a baseline commit, plus `generate-coat-patterns.py --check`.
- `bash scripts/check-eye-closure.sh` — macOS eyelid pixel regression.
- `bash scripts/check-skins-linux.sh` — Linux renderer byte-compared to baseline `e30e0c1` and strict parser tests (`tests/skin-format/parse_test.cpp`).
- These scripts `git show` pinned baseline commits, so they need full git history.

## Working conventions

- **Cross-platform changes:** behavior is hand-ported three times. A change to physics, animation or rendering usually needs the same change in `Sources/`, `windows/src/` and `linux/src/`. Say which ports you touched and which you left.
- **Built-in coats are generated:** `skins/*.kurocat` and the coat pattern sources come from `scripts/generate-coat-patterns.py`. Edit the generator and rerun it rather than hand-editing the outputs.
- **Skin format:** parser is strict (bad type/color/missing field rejects the file; unknown fields are ignored; `formatVersion > 1` rejected). If you change the format, update `docs/skin-format.md`, `shared/SkinSpec.hpp` and the Swift `SkinSpec` together.
- **README tree:** keep the project-structure tree in `README.md` and `README.en.md` in sync when adding top-level directories.
- **Privacy:** input is processed in memory only; never log or persist keystrokes. The only network call is the release update check.
- Commit style: Conventional Commits (`feat:`, `fix:`, `docs:` …).

## Syncing this fork with upstream

This repo is a fork of `chanke17/KuroCat` (`origin` = `siyuanligit/KuroCat`). Last synced at upstream v1.4.3 (merge commit `d98e6b2`).

1. One-time: `git remote add upstream https://github.com/chanke17/KuroCat`. Then `git fetch upstream` (also fetches release tags).
2. Look before merging: `git rev-list --left-right --count HEAD...upstream/main`, `git log --oneline HEAD..upstream/main`, and a dry run with `git merge-tree --write-tree --name-only HEAD upstream/main` (lists conflicting files, touches nothing).
3. Merge on a branch, not `main`: `git switch -c upstream-sync upstream/main`, then `git merge --no-commit --no-ff main`. Resolve, `git add`, commit, then `git switch main && git merge --ff-only upstream-sync`. Push `main` to `origin` only when asked.
4. Resolve toward upstream's structure and re-apply our features on top of it, rather than the reverse:
   - Keyboard colours are `KeyboardSkinSpec` (macOS `CatPhysics.swift`, Windows/Linux `shared/KeyboardSkinSpec.hpp`). A fork keyboard theme (e.g. Graphite = `builtin:3`) must be added there on all three ports, plus the Settings window combos and their index offsets.
   - Coat/keyboard/tablet options live in upstream's Settings window (`OverlayWindow.swift`, `windows/src/main.cpp`, `linux/src/main.cpp`), not the old menus. New coats need the English name, eye pairing and combo offsets there too.
   - Built-in coats: keep both sides in `scripts/generate-coat-patterns.py`, then run it and `--check`; never hand-merge `skins/*.kurocat`.
   - macOS render loop: ours is the one-shot adaptive timer (`scheduleRenderTick`/`renderTick`). Upstream edits to the old 60 Hz loop must be ported into `renderTick` by hand (e.g. its `isChangingLie` fix), not taken as-is.
5. Check: `swiftc -typecheck Sources/*.swift`, `bash scripts/check-skins.sh`, `python3 scripts/generate-coat-patterns.py --check`. Windows and Linux cannot be compiled on a Mac; say so rather than claiming they build.

Known environment issues (not caused by merges):
- `scripts/check-eye-closure.sh` fails to compile on `main` too (pinned baseline lacks `.graphite`; `check-skins.sh` patches this, this script does not). `check-skins-linux.sh` needs GTK3/Cairo.
- `./build.sh` fails at the x86_64 link with only Command Line Tools installed. For a local arm64 build, run the `swiftc` line from `build.sh` with `-target arm64-apple-macos11.0` and sign with the **Apple Development** identity (`codesign --force --deep --sign "<identity>" --identifier com.chanke.kurocat`). An ad-hoc signed build does not match the existing Input Monitoring grant, and the permission looks stuck. Quit other running copies first (same bundle id).
