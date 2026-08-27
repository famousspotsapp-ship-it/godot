# Working in this repository

Fork of the Godot Engine. This file captures the context that is specific to
this fork or is not obvious from the upstream docs; upstream contributor
documentation still applies (see `CONTRIBUTING.md`).

## Current state

* Engine version: **4.7-beta** (`version.py`).
* Fork-specific content: `steel_streets/` — a standalone Game Boy-style Godot
  game project added on top of upstream. It is not part of the engine build.
  See `steel_streets/README.md` (how to run) and
  `steel_streets/ARCHITECTURE.md` (scene flow, physics layers, combat).

## Build, test, lint

```bash
# Editor build with unit tests compiled in (linking needs ~8 GB of RAM/swap).
scons tests=yes target=editor dev_build=yes -j$(nproc)

# Run the test suite headless.
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test
# Single suite: --test-case="*AnimationPlayer*"

# Run the bundled game with the build you just made.
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

There is no dedicated lint command. Static checks run through pre-commit:

```bash
pre-commit install          # once
pre-commit run --all-files  # or --files <changed files>
```

Relevant hooks: `clang-format` for C/C++/GLSL, `ruff-check` + `ruff-format` +
`mypy` for Python (this includes `steel_streets/tools/gen_assets.py`),
`codespell` for prose, plus the local `doc/` and header-guard validators.

## CI

`.github/workflows/` mirrors upstream: `static_checks.yml` (pre-commit, docs,
formatting) plus per-platform build jobs (`linux_builds.yml`,
`windows_builds.yml`, `macos_builds.yml`, `android_builds.yml`,
`ios_builds.yml`, `web_builds.yml`). `static_checks.yml` is the fastest signal
and the one most likely to fail on a docs- or Python-only change.

## Generated codebase wiki

The engine pages of the generated wiki are reliable for orientation. The
`steel_streets` pages are generated from engine C++ sources only — no `.gd`,
`.tscn` or `project.godot` files were indexed — so their specifics (node base
classes, physics-layer table) can be wrong. Treat
`steel_streets/ARCHITECTURE.md` as the source of truth for the game project and
update it in the same PR as any gameplay change.
