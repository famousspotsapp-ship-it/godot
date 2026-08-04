# About this fork

This repository is a fork of [godotengine/godot](https://github.com/godotengine/godot).
Everything outside the directories listed below is upstream engine source and is
kept unmodified so the fork can be rebased on upstream `master` cleanly.

## What this fork adds

| Path | Description |
| :--- | :--- |
| `steel_streets/` | A standalone Godot 4 game project ("Steel Streets: Searching for Master Plank!"). Not an engine module, not compiled into the binary — it is a plain project directory opened with a Godot binary. See [steel_streets/README.md](steel_streets/README.md) and [steel_streets/ARCHITECTURE.md](steel_streets/ARCHITECTURE.md). |
| `FORK.md` | This file. |

There are no engine-side patches: no new modules under `modules/`, no changes to
`core/`, `scene/`, `servers/`, or the build scripts.

## Engine version

The engine source in this fork is **4.7-beta** (`version.py`). `steel_streets/`
declares `config/features=PackedStringArray("4.3", "GL Compatibility")` in its
`project.godot`, meaning it was authored against 4.3 and opens with any 4.3+
editor, including a binary built from this tree. Opening the project with a
newer editor will offer to bump the feature tags; that change is not required
for the project to run.

## Building and running

Build an editor binary (Linux example — linking needs roughly 8 GB of RAM):

```bash
scons tests=yes target=editor dev_build=yes -j$(nproc)
```

Run the engine unit tests:

```bash
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test
# Filter: --test-case="*AnimationPlayer*"
```

Run the bundled game with the binary you just built:

```bash
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

There is no repository-wide lint command; formatting is enforced by
`.pre-commit-config.yaml` (`pre-commit install`, then `pre-commit run --all-files`).
The pre-commit hooks cover the engine's C++/Python style. The GDScript and
Python under `steel_streets/` is not covered by clang-format.

## Contribution notes

Upstream's [CONTRIBUTING.md](CONTRIBUTING.md) applies to any change under the
engine directories. Changes confined to `steel_streets/` do not need to follow
the engine's C++ conventions, but should keep the game project self-contained
(no `res://` references outside `steel_streets/`).
