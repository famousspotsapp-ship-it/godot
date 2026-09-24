---
name: godot-repo-workflow
description: How this fork of Godot is laid out, how to build and test the engine, and how to run the bundled Steel Streets demo. Use when working anywhere in famousspotsapp-ship-it/godot.
---

# Godot repo workflow (famousspotsapp-ship-it/godot)

## What this repo is

- Unmodified mirror of upstream `godotengine/godot` `master` (4.7 dev cycle;
  check `version.py` for the exact `major/minor/status`).
- Plus one fork-specific addition: `steel_streets/`, a pure-GDScript Game
  Boy-style demo game. It never touches engine code.
- Fork-specific docs: `steel_streets/README.md` (run/controls) and
  `steel_streets/ARCHITECTURE.md` (scene flow, autoload, collision layers,
  level builder, asset pipeline).

When a task mentions "the game", "Steel Streets", player/enemies/boss/HUD,
work in `steel_streets/`. Engine changes belong in `core/`, `scene/`,
`servers/`, `editor/`, `modules/`, `platform/`, `drivers/` as upstream.

## Build the engine

```bash
scons tests=yes target=editor dev_build=yes -j$(nproc)
# Output: bin/godot.linuxbsd.editor.dev.x86_64
```

Linking needs ~8 GB RAM. Incremental rebuilds after a small C++ change are
fast; a clean build is long — do not clean unless necessary.

## Run engine unit tests

```bash
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test
# Scope to one area:
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case="*AnimationPlayer*"
```

Tests live in `tests/` (doctest) and `modules/gdscript/tests/scripts/`
(`.gd` + `.out` expected-output pairs).

## Lint / style

No single lint command. Use pre-commit (`.pre-commit-config.yaml`):
clang-format for C++, ruff for Python, codespell, copyright headers, etc.

```bash
pre-commit run --files <changed files>
```

GDScript in `steel_streets/` has no linter in this repo; follow the existing
style (tabs, typed declarations, `##` doc comments).

## Run / smoke-test Steel Streets

```bash
# Import assets (creates steel_streets/.godot, which is git-ignored):
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets --headless --import
# Run the game headless for a few frames to catch script errors:
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets --headless --quit-after 60
# Run with a window (requires a display):
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

Any downloaded Godot 4.3+ binary also works in place of the built one.

Regenerate all sprites/SFX (they are code-generated, not hand-drawn):

```bash
pip install Pillow
python3 steel_streets/tools/gen_assets.py
```

## Gotchas

- `steel_streets/project.godot` declares `config/features=["4.3", ...]`;
  opening it in an editor built from this tree will rewrite that tag on save.
  Avoid committing that churn unless intended.
- `level_1.gd` uses the deprecated `TileMap` node API
  (`add_layer`, `get_layers_count`), not `TileMapLayer`.
- Upstream PR numbers in `git log` (e.g. `#118906`) refer to
  `godotengine/godot`, not this fork.
