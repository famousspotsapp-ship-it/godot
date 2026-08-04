# Steel Streets: Searching for Master Plank!

A side-scrolling beat-'em-up platformer built in Godot 4.3 that channels the
look and feel of the original Game Boy *Teenage Mutant Ninja Turtles* games
(*Fall of the Foot Clan*, *Back from the Sewers*).

The whole game renders into a 160x144 internal viewport scaled up with
nearest-neighbor filtering and is restricted to the classic Game Boy
green palette:

| Light    | Lite     | Dark     | Black    |
| -------- | -------- | -------- | -------- |
| `#e0f8d0` | `#88c070` | `#346856` | `#081820` |

## Running

1. Install [Godot 4.3+](https://godotengine.org/download), or build the engine
   from this repository (see [../FORK.md](../FORK.md)).
2. Open the editor and import this folder, or run from the repository root:
   ```bash
   godot --path steel_streets
   # or, with a binary built from this tree:
   ./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
   ```
   The main scene (`scenes/title_screen.tscn`) launches automatically.

The project declares the `4.3` feature tag while the engine source in this fork
is 4.7-beta; opening it in a newer editor offers to bump the tag, which is not
required for the project to run.

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
├── ARCHITECTURE.md        # scene graph, collision layers, level builder
├── icon.svg
├── scenes/                # title, splash, level, game over, victory + entity scenes
├── scripts/               # GDScript files (autoload, player, enemies, HUD, ...)
├── assets/
│   ├── sprites/           # 4-color GB pixel art (player, enemies, tiles, UI)
│   └── audio/             # chiptune-style WAV SFX + a short BGM loop
└── tools/
    └── gen_assets.py      # regenerates every PNG/WAV from code (Pillow + wave)
```

All sprites and audio are generated programmatically by
`tools/gen_assets.py` so the entire art and sound pipeline is reproducible:

```bash
python3 steel_streets/tools/gen_assets.py
```

(Requires Pillow: `pip install Pillow`.) Do not hand-edit the generated PNG/WAV
files — change the generator and re-run it.

For scene/script structure, the collision-layer map, the animation approach, and
the procedural level builder, see [ARCHITECTURE.md](ARCHITECTURE.md).

## Gameplay

- 3 lives, 5 HP per life. Damage triggers ~1.2 s of invincibility frames.
- Defeat foot soldiers, ranged shuriken throwers, and the boss to score
  points and reach the victory screen.
- Punch question blocks (or walk into them) for score and a small heal.
- A 5–6-screen-wide level with rooftop platforming and ladders to climb.

Beating the boss takes the player to the victory screen; running out of
lives takes them to the game-over screen, both of which return to the
title on any input.
