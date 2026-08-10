# Fork notes

Context for this fork of [godotengine/godot](https://github.com/godotengine/godot).
Kept short and factual so generated documentation and agents pick up current
state instead of inferring it from upstream history.

## Current state

- Engine version: **4.7 beta** (`version.py`: `major=4`, `minor=7`, `patch=0`,
  `status="beta"`). `CHANGELOG.md` documents changes since 4.6 (released
  2026-01-26).
- C++ standard: **C++17** (`-std=gnu++17` / `/std:c++17`, `SConstruct`).
- Divergence from upstream: the `steel_streets/` demo project
  (see `steel_streets/README.md` and `steel_streets/ARCHITECTURE.md`).
  No engine source is modified by this fork.

## Build, test, lint

```bash
scons tests=yes target=editor dev_build=yes -j$(nproc)   # linking needs ~8 GB
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case="*AnimationPlayer*"
pre-commit run --all-files                               # static checks (also run in CI)
```

CI workflows live in `.github/workflows/` (`static_checks.yml` plus per-platform
`linux_builds.yml`, `windows_builds.yml`, `macos_builds.yml`, `android_builds.yml`,
`ios_builds.yml`, `web_builds.yml`).

## Easily-misremembered facts

- **Physics backends are engine modules**, not GDExtensions:
  `modules/godot_physics_2d`, `modules/godot_physics_3d`, `modules/jolt_physics`.
  The backend is selected through the `physics/2d/physics_engine` and
  `physics/3d/physics_engine` project settings, and the editor sets
  `"Jolt Physics"` for new 3D projects (`editor/editor_node.cpp`).
- **Bullet is not vendored.** It was removed in Godot 4; `thirdparty/` now
  contains Jolt (`jolt_physics`), plus e.g. `embree`, `harfbuzz`, `icu4c`,
  `mbedtls`, `openxr`, `sdl`, `thorvg`, `ufbx`, `volk`, `zstd`.
- **Navigation is modular**: `modules/navigation_2d` and `modules/navigation_3d`
  (split from a single module), backed by `thirdparty/recastnavigation`.
- `steel_streets/` is a plain GDScript project. It is not compiled by SCons and
  is not part of the `--test` suite.
