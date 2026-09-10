# Steel Streets: Searching for Master Plank!

A side-scrolling beat-'em-up platformer built for Godot 4.x that channels the
look and feel of the original Game Boy *Teenage Mutant Ninja Turtles* games
(*Fall of the Foot Clan*, *Back from the Sewers*).

The whole game renders into a 160x144 internal viewport scaled up with
nearest-neighbor filtering and is restricted to the classic Game Boy
green palette:

| Light    | Lite     | Dark     | Black    |
| -------- | -------- | -------- | -------- |
| `#e0f8d0` | `#88c070` | `#346856` | `#081820` |

## Running

1. Install [Godot 4.3+](https://godotengine.org/download), or build the
   editor from this repository (`scons target=editor`). `project.godot`
   declares the `4.3` feature tag and the `GL Compatibility` renderer, and
   the project runs unchanged on the in-tree `4.7` engine.
2. Open the editor and import this folder, or run from the command line:
   ```bash
   godot --path steel_streets
   # or, with an editor built from this repo:
   ./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
   ```
   The main scene (`scenes/title_screen.tscn`) launches automatically.

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
└── tools/
    └── gen_assets.py      # regenerates every PNG/WAV from code (Pillow + wave)
```

All sprites and audio are generated programmatically by
`tools/gen_assets.py` so the entire art and sound pipeline is reproducible:

```bash
python3 tools/gen_assets.py
```

(Requires Pillow: `pip install Pillow`.)

## Gameplay

- 3 lives, 5 HP per life. Damage triggers ~1.2 s of invincibility frames.
- Defeat foot soldiers, ranged shuriken throwers, and the boss to score
  points and reach the victory screen.
- Punch question blocks (or walk into them) for score and a small heal.
- A 5–6-screen-wide level with rooftop platforming and ladders to climb.

Beating the boss takes the player to the victory screen; running out of
lives takes them to the game-over screen, both of which return to the
title on any input.

## Architecture

### Scene flow

All scene transitions go through the `GameManager` autoload
(`scripts/game_manager.gd`), which owns the run state (`lives`, `score`,
`current_hp`, `max_hp`) and exposes `goto_*()` helpers:

```
title_screen ──any input──▶ splash_screen ──2.5 s / input──▶ level_1
     ▲                                                          │
     └──── any input ──── game_over  ◀── take_life() == false ──┤
     └──── any input ──── victory    ◀── boss_defeated ─────────┘
```

`title_screen.gd` calls `GameManager.reset_run()` so every run starts fresh.

### GameManager signals

| Signal | Emitted by | Consumed by |
| ------ | ---------- | ----------- |
| `score_changed(new_score)` | `add_score()` | `hud.gd` |
| `lives_changed(new_lives)` | `take_life()` / `reset_run()` | `hud.gd` |
| `hp_changed(current, maximum)` | `set_hp()` / `heal()` | `hud.gd` |

The boss additionally emits `boss_hp_changed(current, maximum)` and
`boss_defeated`; `level_1.gd` wires these into the HUD via
`hud.attach_boss(boss)`, which shows a dedicated boss health bar.

### Level generation

`level_1.gd` builds the whole level procedurally in `_ready()`: it creates a
`TileSet` from `assets/sprites/tiles/tileset.png` (`_build_tileset()`), paints
background and solid foreground layers on a `TileMap` (`TILE = 16`,
`WORLD_W = 100`, `WORLD_H = 9`), then spawns decor, pickups, enemies, the
player, the boss and the HUD from the position tables (`PLATFORMS`,
`LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`, `SHURIKEN_THROWERS`,
`BOSS_POS`, `PLAYER_SPAWN`) at the top of the script. To change the level
layout, edit those tables rather than the `.tscn`.

### Entities

- `player.gd` (`CharacterBody2D`) — movement, jump, melee attack hitbox,
  ladder climbing, knockback and ~1.2 s invincibility. Joins the `player`
  group; its attack `Area2D` joins the `player_attack` group.
- `enemy_base.gd` (`CharacterBody2D`) — shared enemy behaviour: `max_hp`,
  `contact_damage`, `score_value` exports, hurtbox/hitbox handling,
  `take_damage()`, hit-flash and `_die()`. Subclasses override
  `_physics_process()` for AI:
  - `foot_soldier.gd` — patrol, turn at ledges, chase within `CHASE_RANGE`.
  - `shuriken_thrower.gd` — paces and lobs `projectile.gd` shurikens.
  - `boss.gd` — `PAUSE → WINDUP → CHARGE → RECOVER` state machine.
- `question_block.gd` (`Area2D`) — one-shot pickup giving `score_reward` and
  `heal_amount`, triggered by player body or attack area overlap.
- `hud.gd` (`CanvasLayer`) — hearts, lives, score, boss bar.

All sprite animations are defined in code as `FRAMES_SPEC` arrays and turned
into `SpriteFrames` by `anim_util.gd` (`AnimUtil.build()`), which slices
horizontal strip PNGs into `AtlasTexture` frames. Adding an animation means
adding a strip PNG in `tools/gen_assets.py` and a matching `FRAMES_SPEC`
entry.

### Physics layers

`project.godot` names the 2D physics layers used by every scene:

| Layer | Name | Layer | Name |
| ----- | ---- | ----- | ---- |
| 1 | `world` | 5 | `enemy_hitbox` |
| 2 | `player` | 6 | `ladder` |
| 3 | `player_hitbox` | 7 | `pickup` |
| 4 | `enemy` | 8 | `hazard` |

### Rendering settings

The Game Boy look comes from `project.godot`: a 160x144 viewport with
`window/stretch/mode="viewport"` and `aspect="keep"`, a 640x576 default
window, `default_texture_filter=0` (nearest), pixel snapping enabled, and
the `gl_compatibility` renderer on desktop and mobile.

## Development notes

- Generated `.godot/` and `.import/` directories are ignored; the committed
  `*.import` files pin import settings for every asset.
- The `tools/gen_assets.py` script is the single source of truth for art and
  audio. Edit the pixel-row strings or tone functions there and re-run it;
  never hand-edit the PNG/WAV files.
- The game has no automated tests; verify changes by playing the project
  from the editor or with `godot --path steel_streets`.
