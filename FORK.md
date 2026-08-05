# About this fork

This repository is a fork of [godotengine/godot](https://github.com/godotengine/godot).
It contains the full upstream engine source plus one addition: the
[`steel_streets/`](steel_streets/) Godot project (see
[Steel Streets architecture](steel_streets/ARCHITECTURE.md)).

Use this page as the entry point for fork-specific context; everything else in
the repository is upstream and documented by the
[official Godot documentation](https://docs.godotengine.org).

## Fork state

| | |
| --- | --- |
| Upstream base | `4.7-beta` (`version.py`: major 4, minor 7, patch 0, status `beta`) |
| Fork-only content | `steel_streets/` (added in PR #2) |
| Engine source changes | none — no file outside `steel_streets/` diverges from upstream |
| Branch | `main` |

Because no engine code is modified, upstream changes can be pulled in with a
plain merge or rebase; conflicts should only ever appear if upstream adds a
top-level `steel_streets/` or `FORK.md`.

## Building and testing the engine

The engine builds exactly as upstream does (see
[Getting the source](README.md#getting-the-source) and the
[official build docs](https://docs.godotengine.org/en/latest/contributing/development/compiling/)).
The commands used in this fork's CI/dev environment are:

```bash
# Build an editor binary with the unit tests compiled in (Linux).
# Linking needs roughly 8 GB of RAM.
scons tests=yes target=editor dev_build=yes -j$(nproc)

# Run the engine unit tests headlessly.
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test

# Run a subset of tests.
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case="*AnimationPlayer*"
```

There is no separate lint command; static checks run through `pre-commit`
(`pre-commit install`, config in `.pre-commit-config.yaml`), which is what the
`static_checks.yml` workflow executes.

## Running the bundled game

`steel_streets/` is a standard Godot 4 project, so any Godot 4.3+ binary can run
it — including one built from this repository:

```bash
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

Add `--headless --quit-after 200` to smoke-test that the project boots and its
scripts parse without opening a window.

See [`steel_streets/README.md`](steel_streets/README.md) for controls and
gameplay, and [`steel_streets/ARCHITECTURE.md`](steel_streets/ARCHITECTURE.md)
for how the project is put together.
