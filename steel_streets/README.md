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

1. Install [Godot 4.3+](https://godotengine.org/download).
2. Open the editor and import this folder, or run from the command line:
   ```bash
   godot --path steel_streets
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

## Architecture

Everything is plain GDScript; there is no engine-side C++ for this game and it
does not depend on any custom module. It is a regular Godot project that
happens to live inside the engine source tree.

### Scene flow

`GameManager` (`scripts/game_manager.gd`) is the only autoload. It owns the
run state (`lives`, `score`, `current_hp`/`max_hp`), emits `score_changed`,
`lives_changed` and `hp_changed`, and is the single place that switches
scenes via `get_tree().change_scene_to_file()`:

```
title_screen ──start──▶ splash_screen ──(2.5 s / any key)──▶ level_1
                                                            │
                                     boss defeated ◀────────┤────────▶ out of lives
                                          │                                   │
                                       victory ──any key──▶ title ◀──any key── game_over
```

`level_1` is reloaded (not the whole game) when the player loses a life but
still has lives left (`GameManager.take_life()` returns `true`).

### Scripts

| Script | Node type | Role |
| ------ | --------- | ---- |
| `game_manager.gd` | `Node` (autoload) | Run state, score/lives/HP signals, scene transitions |
| `level_1.gd` | `Node2D` | Builds the level procedurally at `_ready()`: creates the `TileSet` from `tileset.png`, paints background/foreground tile layers from the `PLATFORMS`/`LADDERS`/`SPIKES` tables, then spawns pickups, enemies, the player, the boss and the HUD |
| `player.gd` | `CharacterBody2D` | Movement, jump, ladder climbing, melee attack hitbox, knockback, i-frames, death → `GameManager.take_life()` |
| `enemy_base.gd` | `CharacterBody2D` | Shared enemy base: HP, contact damage, hit flash, death (adds `score_value`, disables areas, `queue_free`) |
| `foot_soldier.gd` | extends `enemy_base` | Patrols, turns at ledges, chases within `CHASE_RANGE` |
| `shuriken_thrower.gd` | extends `enemy_base` | Paces around its origin and fires `projectile.tscn` every `FIRE_INTERVAL` while the player is in sight |
| `boss.gd` | extends `enemy_base` | `PAUSE → WINDUP → CHARGE → RECOVER` state machine; emits `boss_hp_changed` / `boss_defeated` (victory) |
| `projectile.gd` | `Area2D` | Straight-line shuriken with lifetime; damages the player on contact |
| `question_block.gd` | `Area2D` | One-shot pickup: score + heal when punched or touched |
| `hud.gd` | `CanvasLayer` | Hearts, score, lives; `attach_boss()` shows the boss HP bar |
| `title_screen.gd`, `splash_screen.gd`, `game_over.gd`, `victory.gd` | `Control` | Static screens that advance on `start` / any input |
| `anim_util.gd` | `Object` (static) | `AnimUtil.build(FRAMES_SPEC)` turns horizontal sprite strips into a `SpriteFrames` resource at runtime |

Every animated entity declares a `FRAMES_SPEC` array (`name`, `path`, `w`,
`h`, `count`, `fps`, `loop`) and builds its `SpriteFrames` in `_ready()`, so
no `.tres` animation resources are checked in.

### Collision layers and combat

Physics layers are named in `project.godot` (`[layer_names]`):

| Bit | Layer | Used by |
| --- | ----- | ------- |
| 1 | `world` | Tile collision |
| 2 | `player` | Player body |
| 3 | `player_hitbox` | Player attack `Area2D` (group `player_attack`) |
| 4 | `enemy` | Enemy bodies and hurtboxes |
| 5 | `enemy_hitbox` | Enemy contact hitboxes, projectiles |
| 6 | `ladder` | Ladder areas (`enter_ladder()` / `exit_ladder()` on the player) |
| 7 | `pickup` | Question blocks |
| 8 | `hazard` | Spikes |

Damage is exchanged through groups rather than type checks: an enemy
`Hurtbox` that overlaps an area in group `player_attack` calls
`get_attack_damage()` on the area's parent; an enemy `Hitbox` that overlaps a
body in group `player` calls `take_damage(amount, from_position)` on it.

### Rendering settings

`project.godot` forces the GB look engine-wide: `gl_compatibility` renderer,
160x144 viewport with `stretch/mode="viewport"` and `aspect="keep"`,
`default_texture_filter=0` (nearest) and 2D transform/vertex pixel snapping.
The palette itself is baked into the PNGs by `tools/gen_assets.py`.

## Gameplay

- 3 lives, 5 HP per life. Damage triggers ~1.2 s of invincibility frames.
- Defeat foot soldiers, ranged shuriken throwers, and the boss to score
  points and reach the victory screen.
- Punch question blocks (or walk into them) for score and a small heal.
- A 5–6-screen-wide level with rooftop platforming and ladders to climb.

Beating the boss takes the player to the victory screen; running out of
lives takes them to the game-over screen, both of which return to the
title on any input.
