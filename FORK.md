# About this fork

This repository is a fork of [godotengine/godot](https://github.com/godotengine/godot).
Everything outside `steel_streets/` and this file is upstream engine code; read
`README.md` and `CONTRIBUTING.md` for the engine itself.

## What is fork-specific

| Path | What it is |
| --- | --- |
| `steel_streets/` | "Steel Streets: Searching for Master Plank!" — a Game Boy-style beat-'em-up written in GDScript, added in PR #2 |
| `steel_streets/docs/ARCHITECTURE.md` | Technical reference for that game |

Engine baseline: `version.py` reports **4.7-beta** (branched from upstream
`master` at `6d6e822c68`, "Bump version to 4.7-beta"). No engine source files are
modified by this fork, so upstream merges should stay conflict-free.

## Working in this repo

```bash
# Build an editor binary (linking needs ~8 GB RAM)
scons tests=yes target=editor dev_build=yes -j$(nproc)

# Run the engine unit tests
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test
# Narrow them down: --test-case="*AnimationPlayer*"

# Run the bundled game
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets

# Formatting/static checks (there is no single lint command)
pre-commit install && pre-commit run --all-files
```

`steel_streets/` has no automated tests; it is verified by running the game.
Its assets are generated, not hand-authored — regenerate them with
`python3 steel_streets/tools/gen_assets.py` (requires Pillow) rather than
editing the PNG/WAV files.
