# Steel Streets — Architecture

Technical reference for the `steel_streets/` Godot project. For how to run and
play the game, see [README.md](README.md). This document describes how the
project is wired together so that changes can be made without reading every
script first.

Steel Streets is a plain Godot 4 project (`config/features` = `4.3`,
`GL Compatibility`). It does **not** modify or depend on any engine source in
this repository; it lives here only so the engine and the sample game are
versioned together. It can be opened with any stock Godot 4.3+ editor or with
an editor built from this tree (`bin/godot.linuxbsd.editor.*`).

```bash
# From the repo root, using a locally built editor:
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

## Runtime configuration (`project.godot`)

| Setting | Value | Why |
| --- | --- | --- |
| `run/main_scene` | `res://scenes/title_screen.tscn` | Title is the entry point |
| `display/window/size/viewport_*` | 160 x 144 | Native Game Boy resolution |
| `window_width_override` / `window_height_override` | 640 x 576 | 4x integer scale on desktop |
| `window/stretch/mode` / `aspect` | `viewport` / `keep` | Pixel-perfect upscale |
| `rendering/textures/canvas_textures/default_texture_filter` | `0` (Nearest) | No bilinear smearing |
| `2d/snap/snap_2d_transforms_to_pixel`, `snap_2d_vertices_to_pixel` | `true` | Avoid sub-pixel shimmer |
| `renderer/rendering_method` | `gl_compatibility` | Runs anywhere incl. web/mobile |
| `[autoload] GameManager` | `res://scripts/game_manager.gd` | Global run state |

### Input actions

Defined in `[input]`: `move_left`, `move_right`, `jump`, `attack`, `climb_up`,
`climb_down`, `start`. Every action has keyboard and gamepad bindings (see
README "Controls"). Scripts only ever query actions, never raw keycodes.

### 2D physics layers (`[layer_names]`)

| Bit | Name | Used by |
| --- | --- | --- |
| 1 | `world` | TileMap solid tiles |
| 2 | `player` | `Player` body |
| 3 | `player_hitbox` | `Player/AttackHitbox` (Area2D, group `player_attack`) |
| 4 | `enemy` | Enemy bodies + `Hurtbox` areas |
| 5 | `enemy_hitbox` | Enemy `Hitbox` areas and `Projectile` |
| 6 | `ladder` | Ladder `Area2D`s created by `level_1.gd` |
| 7 | `pickup` | `QuestionBlock` |
| 8 | `hazard` | Spike `Area2D`s created by `level_1.gd` |

Rule of thumb: bodies live on `world`/`player`/`enemy`; anything that *deals*
damage is an `Area2D` on a `*_hitbox` layer; anything that *receives* damage is
an `Area2D` (`Hurtbox`) or a body that implements `take_damage()`.

## Scene flow

All scene transitions go through the `GameManager` autoload; no scene calls
`change_scene_to_file` directly.

```
title_screen ──any input──▶ splash_screen ──any input / 2.5 s──▶ level_1
     ▲                                                          │
     │                                    boss_defeated ────────┼──▶ victory
     │                                    lives == 0 ───────────┼──▶ game_over
     └────────────────────────── any input ◀────────────────────┘
```

| Scene | Script | Responsibility |
| --- | --- | --- |
| `scenes/title_screen.tscn` | `scripts/title_screen.gd` | Blinking "press start"; calls `GameManager.reset_run()` then `goto_splash()` |
| `scenes/splash_screen.tscn` | `scripts/splash_screen.gd` | Story card; `goto_level()` |
| `scenes/level_1.tscn` | `scripts/level_1.gd` | Builds the whole level procedurally (see below) |
| `scenes/hud.tscn` | `scripts/hud.gd` | `CanvasLayer` with hearts, lives, score, boss bar |
| `scenes/game_over.tscn` | `scripts/game_over.gd` | Returns to title on input |
| `scenes/victory.tscn` | `scripts/victory.gd` | Returns to title on input |
| `scenes/player.tscn` | `scripts/player.gd` | Player `CharacterBody2D` |
| `scenes/foot_soldier.tscn` | `scripts/foot_soldier.gd` | Melee patroller |
| `scenes/shuriken_thrower.tscn` | `scripts/shuriken_thrower.gd` | Ranged enemy |
| `scenes/boss.tscn` | `scripts/boss.gd` | End-of-level boss |
| `scenes/projectile.tscn` | `scripts/projectile.gd` | Shuriken `Area2D` |
| `scenes/question_block.tscn` | `scripts/question_block.gd` | Score + heal pickup |

## `GameManager` autoload (`scripts/game_manager.gd`)

Single source of truth for persistent run state:

```gdscript
const STARTING_LIVES := 3
const STARTING_HP := 5
var lives, score, max_hp, current_hp

signal score_changed(new_score)
signal lives_changed(new_lives)
signal hp_changed(current, maximum)

reset_run()            # called by the title screen
add_score(amount)
set_hp(value) / heal(amount)
take_life() -> bool    # false when no lives remain
goto_title() / goto_splash() / goto_level() / goto_game_over() / goto_victory()
```

The HUD subscribes to the three signals; gameplay code never touches the HUD
directly. Player HP is stored here (not on the player) so it survives the
respawn that follows losing a life.

## Level construction (`scripts/level_1.gd`)

`level_1.tscn` contains only `TileMap`, `Entities` (Node2D) and `Bgm`
(AudioStreamPlayer). Everything else (including the player's camera) is generated in
`_ready()` from constant tables at the top of the script:

- `TILE = 16`, `WORLD_W = 100`, `WORLD_H = 9`, `GROUND_ROW = 8` — the level is
  100 tiles (1600 px, ~10 screens of 160 px) wide.
- `_build_tileset()` slices `assets/sprites/tiles/tileset.png` into 8 atlas
  tiles (`A_BRICK`, `A_CONCRETE`, `A_ROOF`, `A_BRICK_BG`, `A_WINDOW`, `A_DOOR`,
  `A_SKY_DIAMOND`, `A_SKY_ZIGZAG`). Only `SOLID_TILES` get a physics polygon
  on layer `world`.
- `PLATFORMS` (inclusive tile rectangles), `LADDERS` (`[col, top, bottom]`),
  `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`, `SHURIKEN_THROWERS`, `BOSS_POS`,
  `PLAYER_SPAWN` are plain arrays — edit these to redesign the level.
- Ladders and spikes are not scenes; `_make_ladder()` / `_make_spike()` build
  `Area2D`s in code. Ladder areas call `player.enter_ladder()` /
  `exit_ladder()` on overlap.
- `_spawn_boss()` connects `boss_defeated` to `_on_boss_defeated()` →
  `GameManager.goto_victory()`, and `_attach_boss_to_hud()` gives the HUD the
  boss reference so it can show the boss health bar.

## Entities

### Player (`scripts/player.gd`, `CharacterBody2D`)

- Tunables are `const`s: `SPEED 60`, `JUMP_VELOCITY -190`, `GRAVITY 700`,
  `CLIMB_SPEED 50`, knockback, `INVINCIBLE_TIME 1.2`, `ATTACK_TIME 0.30`,
  `ATTACK_COOLDOWN 0.15`, `ATTACK_DAMAGE 1`.
- `_physics_process` dispatches to `_handle_grounded()` or `_handle_climbing()`
  (climbing is entered when `ladders_overlapping > 0` and up/down is pressed).
- Attacking enables `AttackHitbox/Shape` for `ATTACK_TIME`; enemies' `Hurtbox`
  detect the area (group `player_attack`) and call back
  `get_attack_damage()`.
- `take_damage(amount, from_position)` applies knockback, starts the
  invincibility strobe (`_update_invincibility_flash`), and forwards HP to
  `GameManager.set_hp()`. At 0 HP `_die()` calls `GameManager.take_life()` and
  either reloads the level via `goto_level()` (a respawn is a full level
  rebuild) or calls `goto_game_over()`.
- The player node is in group `player`; enemies use this group to find/target it.

### Enemies (`scripts/enemy_base.gd` and subclasses)

`enemy_base.gd` (`CharacterBody2D`) owns the shared contract:

- `@export max_hp`, `contact_damage`, `score_value`.
- Optional child nodes resolved with `has_node()`: `Sprite`, `Hurtbox`,
  `Hitbox`, `SfxHit`.
- `Hurtbox.area_entered` → `take_damage()` (hit flash via `modulate`);
  `Hitbox.body_entered` → `player.take_damage(contact_damage, global_position)`.
- `_die()` awards `score_value` through `GameManager.add_score()`, disables
  hitboxes and `queue_free()`s. Subclasses override `_die()` when they need
  extra behaviour (the boss emits `boss_defeated`).
- All enemies join group `enemy`.

Subclasses (`extends "res://scripts/enemy_base.gd"`) only add movement/AI:

| Script | Behaviour | Key constants |
| --- | --- | --- |
| `foot_soldier.gd` | Patrols, turns at ledges (`_has_floor_ahead()`), chases within range | `PATROL_SPEED 22`, `CHASE_SPEED 35`, `CHASE_RANGE 70` |
| `shuriken_thrower.gd` | Paces around `origin_x`, fires `projectile.tscn` toward the player | `PACE_RANGE 14`, `FIRE_INTERVAL 1.6`, `SIGHT_RANGE 110` |
| `boss.gd` | State machine `PAUSE → WINDUP → CHARGE → RECOVER`; emits `boss_hp_changed` and `boss_defeated` | `CHARGE_SPEED 70`, timers per state |

### Projectile and pickups

- `projectile.gd` (`Area2D`): moves horizontally by `@export direction` at
  `SPEED 110`, damages the player on `body_entered`, frees itself on contact
  with the player or world geometry, or after `LIFETIME 3.0` s.
- `question_block.gd` (`Area2D`): consumed either by the player's body or by
  the `player_attack` area; grants `score_reward` (50) and `heal_amount` (1)
  via `GameManager`, then frees itself.

## Animation helper (`scripts/anim_util.gd`)

There are no `.tres` `SpriteFrames` resources checked in. Every animated entity
declares a `FRAMES_SPEC` array of
`{name, path, count, w, h, fps, loop}` dictionaries and calls
`AnimUtil.build(FRAMES_SPEC)` in `_ready()`, which slices horizontal strip PNGs
into `AtlasTexture` frames. To add an animation, add a strip PNG and a spec
entry; no editor work is needed.

## Asset pipeline (`tools/gen_assets.py`)

All PNGs under `assets/sprites/` and WAVs under `assets/audio/` are
**generated** and should not be hand-edited. `gen_assets.py` (Python 3 +
Pillow) draws every sprite from ASCII rows using the 4-colour palette
(`GB = [#e0f8d0, #88c070, #346856, #081820]`, `.` = transparent) and
synthesises SFX/BGM with square waves, slides and noise bursts
(`square_tone`, `slide`, `noise_burst`, `write_wav`). UI text images
(`assets/sprites/ui/*.png`) come from a built-in 3x5 pixel font via
`render_text()`.

```bash
pip install Pillow
python3 steel_streets/tools/gen_assets.py   # rewrites assets/ in place
```

The accompanying `*.import` files are Godot's import metadata and are
committed so a fresh checkout opens without a reimport step. `.godot/` is
git-ignored.

## Conventions

- GDScript is fully statically typed (`-> void`, typed `var`s); keep new code
  the same way.
- Gameplay numbers are `const`s at the top of each script rather than exports,
  except for per-instance enemy stats (`max_hp`, `contact_damage`,
  `score_value`) which are `@export`.
- Cross-object communication uses groups (`player`, `enemy`,
  `player_attack`) and duck-typed `has_method()` checks rather than hard
  class references, so scenes can be swapped freely.
- Keep the 160x144 viewport and 4-colour palette; all art must use only the
  four `GB` colours plus transparency.
