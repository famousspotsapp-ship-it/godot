# Fork notes

This repository is a fork of [godotengine/godot](https://github.com/godotengine/godot).
Everything outside the list below is upstream Godot and should be read with the
[official docs](https://docs.godotengine.org/en/latest/contributing/development/).

## What this fork adds

| Path | Description |
| ---- | ----------- |
| `steel_streets/` | "Steel Streets: Searching for Master Plank!", a Game Boy-style beat-'em-up sample project. See [steel_streets/README.md](steel_streets/README.md). |

Nothing else in the tree is patched, so the fork can be rebased on upstream
`master` without touching engine code.

## Version

`version.py` currently tracks upstream **4.7-beta**. The sample project declares
`config/features=PackedStringArray("4.3", ...)`, which is a minimum-version tag:
it opens in any 4.3+ editor, including a binary built from this checkout.

## Building the engine

Upstream SCons build, editor + unit tests, development build:

```bash
scons tests=yes target=editor dev_build=yes -j$(nproc)
```

The result is `bin/godot.linuxbsd.editor.dev.x86_64` (name varies by platform).

Linux build prerequisites:

```bash
sudo apt-get install -y pkg-config build-essential libx11-dev libxcursor-dev \
  libxinerama-dev libgl1-mesa-dev libglu-dev libasound2-dev libpulse-dev \
  libdbus-1-dev libudev-dev libxi-dev libxrandr-dev libxrender-dev \
  libfreetype6-dev libpng-dev zlib1g-dev libspeechd-dev libfontconfig-dev
pip install scons   # a user install lands in ~/.local/bin; make sure it is on PATH
```

Linking needs roughly 8 GB of memory. On a small machine add swap first, and
note that swap is not persisted across reboots unless it is in `/etc/fstab`:

```bash
sudo fallocate -l 8G /swapfile && sudo chmod 600 /swapfile && sudo mkswap /swapfile
sudo swapon /swapfile   # re-run after every reboot
```

## Running tests

The unit tests live in the engine binary (requires `tests=yes`):

```bash
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case="*AnimationPlayer*"
```

## Static checks

There is no `lint` target. Formatting and static checks run through
`pre-commit` (clang-format, black/ruff, header guards, copyright headers, …):

```bash
pip install pre-commit
pre-commit install          # installs the git hook
pre-commit run --all-files  # slow on a full checkout; prefer the hook
```

CI runs the same checks in `.github/workflows/static_checks.yml`.

## CI coverage

`.github/workflows/` is unmodified upstream: it builds and tests the engine on
Linux, Windows, macOS, Android, iOS and Web. **No workflow opens, imports or
exports `steel_streets/`**, so changes to the sample project are not validated
by CI — verify them locally with the commands in
[steel_streets/README.md](steel_streets/README.md).
