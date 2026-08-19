# Steel Streets: Searching for Master Plank!

A side-scrolling beat-'em-up platformer built for Godot 4.3+ that channels the
look and feel of the original Game Boy *Teenage Mutant Ninja Turtles* games
(*Fall of the Foot Clan*, *Back from the Sewers*).

The whole game renders into a 160x144 internal viewport scaled up with
nearest-neighbor filtering and is restricted to the classic Game Boy
green palette:

| Light    | Lite     | Dark     | Black    |
| -------- | -------- | -------- | -------- |
| `#e0f8d0` | `#88c070` | `#346856` | `#081820` |

## Running

1. Use an editor build from this repository (currently 4.7-beta), or install
   [Godot 4.3+](https://godotengine.org/download).
2. Open the editor and import this folder, or run from the command line:
   ```bash
   godot --path steel_streets
   # or, with a build produced by this repo:
   ./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
   ```
   The main scene (`scenes/title_screen.tscn`) launches automatically.

The project declares `config/features=("4.3", "GL Compatibility")`, so it loads
in any 4.3+ editor. `scenes/level_1.tscn` still uses the deprecated `TileMap`
node rather than `TileMapLayer`.

## Controls

| Action | Keyboard | Gamepad |
| ------ | -------- | ------- |
| Move left / right | `A` / `D` or `←` / `→` | left stick / d-pad |
| Jump | `Space` or `W` | A button |
| Attack (melee) | `Z` or `J` | X button |
| Climb up / down (on ladder) | `W` / `S` or `↑` / `↓` | d-pad |
| Start / advance screen | `Enter` or `Space` | Start |

## Project layout

```
steel_streets/
├── project.godot          # input map, autoload, viewport scaling
├── icon.svg
├── scenes/                # title, splash, level, game over, victory + entity scenes
├── scripts/               # GDScript files (autoload, player, enemies, HUD, ...)
├── assets/
│   ├── sprites/           # 4-color GB pixel art (player, enemies, tiles, UI)
│   └── audio/             # chiptune-style WAV SFX + a short BGM loop
├── tools/
│   └── gen_assets.py      # regenerates every PNG/WAV from code (Pillow + wave)
└── docs/
    └── ARCHITECTURE.md    # GameManager API, entity stats, physics layers, level build
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the technical reference:
scene flow, movement/combat tunables, collision layers, and how `level_1.gd`
builds the level procedurally.

All sprites and audio are generated programmatically by
`tools/gen_assets.py` so the entire art and sound pipeline is reproducible:

```bash
python3 steel_streets/tools/gen_assets.py
```

(Requires Pillow: `pip install Pillow`.)

## Gameplay

- 3 lives, 5 HP per life. Damage triggers ~1.2 s of invincibility frames.
- Defeat foot soldiers, ranged shuriken throwers, and the boss to score
  points and reach the victory screen.
- Punch question blocks (or walk into them) for score and a small heal.
- A 100-tile-wide level (~10 Game Boy screens) with rooftop platforming,
  ladders to climb, and spike hazards.

Beating the boss takes the player to the victory screen; running out of
lives takes them to the game-over screen, both of which return to the
title on any input.
