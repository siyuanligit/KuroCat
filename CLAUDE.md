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
