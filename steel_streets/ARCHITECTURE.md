# Steel Streets — Architecture

Technical reference for the `steel_streets/` demo project. For how to run it
and the controls, see [README.md](README.md). This document describes how the
project is put together so it can be extended or debugged quickly.

The demo is a plain Godot project (GDScript + `.tscn` scenes) that lives
alongside the engine source. It does **not** modify or depend on any engine
internals beyond the public scripting API; it targets the `gl_compatibility`
renderer and a fixed 160x144 viewport (`project.godot` → `[display]`).

## Scene flow

`GameManager` (autoload, `scripts/game_manager.gd`) is the only place that
changes scenes. Every screen calls one of its `goto_*` helpers:

```
title_screen.tscn ──start──> splash_screen.tscn ──(2.5 s or input)──> level_1.tscn
                                                                        │
                        ┌──────── boss_defeated (+1.5 s) ───────────────┤
                        ▼                                               │
                  victory.tscn                       player dies, lives left ──> level_1.tscn (reload)
                        │                            player dies, no lives ──> game_over.tscn
                        └──── any input ───> title_screen.tscn <──── any input ────┘
```

| Scene | Script | Role |
| --- | --- | --- |
| `scenes/title_screen.tscn` | `title_screen.gd` | Blinking "press start" screen; `start` action → `goto_splash()`. |
| `scenes/splash_screen.tscn` | `splash_screen.gd` | Story card; auto-advances after `HOLD_TIME` (2.5 s) or on input → `goto_level()`. |
| `scenes/level_1.tscn` | `level_1.gd` | The whole playable level, built procedurally (see below). |
| `scenes/game_over.tscn` / `scenes/victory.tscn` | `game_over.gd` / `victory.gd` | End cards; any input → `goto_title()`. |

## Run state: `GameManager`

Persistent state that must survive `change_scene_to_file()` lives on the
autoload:

- `lives` (starts at `STARTING_LIVES = 3`), `score`, `max_hp` / `current_hp`
  (`STARTING_HP = 5`).
- Signals: `score_changed(new_score)`, `lives_changed(new_lives)`,
  `hp_changed(current, maximum)`. The HUD subscribes to these; gameplay code
  never touches HUD nodes directly.
- Mutators: `reset_run()`, `add_score()`, `set_hp()` (clamped), `heal()`,
  `take_life()` (returns `false` when the run is over, otherwise refills HP).

## Level construction: `level_1.gd`

`level_1.tscn` is intentionally tiny; almost everything is generated in
`_ready()` so the tilemap does not have to be hand-authored:

1. `_build_tileset()` creates a `TileSet` from
   `assets/sprites/tiles/tileset.png` (16x16 atlas, one row; atlas coords are
   the `A_*` constants). Tiles in `SOLID_TILES` get a physics polygon on
   layer 1 (`world`).
2. `_paint_background()` / `_paint_foreground()` fill two `TileMap` layers:
   layer 0 is decorative (sky/brick backdrop, windows, doors), layer 1 holds
   the collidable ground and platforms listed in `PLATFORMS`
   (`[x0, y0, x1, y1, atlas]` rectangles in tile units).
3. `_spawn_decor()`, `_spawn_pickups()`, `_spawn_enemies()`, `_spawn_player()`,
   `_spawn_boss()`, `_spawn_hud()` instantiate the packed scenes from the
   data tables `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`,
   `SHURIKEN_THROWERS`, `BOSS_POS`, `PLAYER_SPAWN`.
4. Ladders and spikes are created purely in code (`_make_ladder()`,
   `_make_spike()`) as `Area2D`s on the `ladder` / `hazard` layers; the
   ladder callbacks call `Player.enter_ladder()` / `exit_ladder()`.
5. The boss's `boss_defeated` signal is wired to `_on_boss_defeated()`, which
   waits 1.5 s and calls `GameManager.goto_victory()`.

World size is `WORLD_W = 100` × `WORLD_H = 9` tiles of `TILE = 16` px
(ground at `GROUND_ROW = 8`). To change the layout, edit the constant tables;
no scene editing is required.

## Entities

All entities are `CharacterBody2D` scenes whose `AnimatedSprite2D` frames are
built at runtime by `anim_util.gd`:

- `AnimUtil.build(specs)` turns a list of
  `{name, path, count, w, h, fps, loop}` dictionaries into a `SpriteFrames`
  resource by slicing horizontal strips in `assets/sprites/<entity>/` into
  `AtlasTexture` frames. Each entity declares its own `FRAMES_SPEC` constant.

### Player (`player.gd`)

State is a handful of timers and flags rather than a state machine:
`_handle_grounded()` (walk/jump/attack) or `_handle_climbing()` (while
`climbing`), then `_update_attack_state()`, `_update_animation()`,
`_update_invincibility_flash()`.

- Attack: `_start_attack()` enables `$AttackHitbox` (in group
  `player_attack`) for `ATTACK_TIME` (0.30 s) with a `ATTACK_COOLDOWN`
  (0.15 s). Enemies read damage through `get_attack_damage()`.
- Damage: `take_damage(amount, from_position)` applies knockback away from
  the source, sets `INVINCIBLE_TIME` (1.2 s) i-frames, and updates HP through
  `GameManager.set_hp()`. At 0 HP `_die()` calls `GameManager.take_life()`
  and either reloads the level or goes to game over.
- Tuning constants: `SPEED`, `JUMP_VELOCITY`, `GRAVITY`, `CLIMB_SPEED`,
  `KNOCKBACK_X/Y`.

### Enemies (`enemy_base.gd` and subclasses)

`enemy_base.gd` owns HP, hit flash, contact damage and the death handshake:

- `$Hurtbox` (`Area2D`) listens for areas in group `player_attack` →
  `take_damage()`.
- `$Hitbox` (`Area2D`) deals `contact_damage` to bodies in group `player`.
- `_die()` adds `score_value` via `GameManager`, disables both areas
  (deferred) and `queue_free()`s.
- Exported knobs: `max_hp`, `contact_damage`, `score_value`.

| Script | Behaviour |
| --- | --- |
| `foot_soldier.gd` | Patrols at `PATROL_SPEED`, turns at ledges (`_has_floor_ahead()`), chases at `CHASE_SPEED` within `CHASE_RANGE`. |
| `shuriken_thrower.gd` | Paces in place (`PACE_RANGE`) and fires `projectile.tscn` every `FIRE_INTERVAL` when the player is within `SIGHT_RANGE`. |
| `boss.gd` | State machine `PAUSE → WINDUP → CHARGE → RECOVER` (timers `PAUSE_TIME`, `WINDUP_TIME`, `CHARGE_TIME`, `RECOVER_TIME`); always faces the player. Emits `boss_hp_changed(current, maximum)` and `boss_defeated`. |
| `projectile.gd` | Straight-line shuriken (`SPEED`, `LIFETIME`, `DAMAGE`); damages the player on contact and frees itself. |
| `question_block.gd` | Pickup: consumed on player body contact or player attack; grants `score_reward` and `heal_amount`. |

### HUD (`hud.gd`, `CanvasLayer`)

Renders hearts (`heart.png` / `heart_empty.png`), lives and a zero-padded
score, driven entirely by `GameManager` signals. `attach_boss(boss)` (called
from `level_1.gd`) connects to the boss's signals and shows the boss bar.

## Physics layers

Defined in `project.godot` → `[layer_names]`:

| # | Name | Used by |
| - | --- | --- |
| 1 | `world` | Tilemap solids |
| 2 | `player` | Player body |
| 3 | `player_hitbox` | Player attack area |
| 4 | `enemy` | Enemy bodies |
| 5 | `enemy_hitbox` | Enemy contact/attack areas, projectiles |
| 6 | `ladder` | Ladder areas |
| 7 | `pickup` | Question blocks |
| 8 | `hazard` | Spikes |

Groups used for lookups: `player`, `player_attack`, `enemy`.

## Asset pipeline: `tools/gen_assets.py`

Every PNG and WAV under `assets/` is produced by this single script (Pillow +
the stdlib `wave` module), so the art and audio are fully reproducible:

- Sprites are drawn as rows of characters (`.` transparent, `0`–`3` palette
  index) and converted with `img_from_rows()`; strips are concatenated per
  animation (`gen_player()`, `gen_foot_soldier()`, `gen_shuriken_thrower()`,
  `gen_boss()`, `gen_projectile()`).
- Tiles (`tile_*()`) are assembled into `tiles/tileset.png` by
  `gen_tileset()`; UI elements and bitmap text come from `gen_ui()` /
  `gen_text_assets()`.
- SFX/BGM are synthesised with `square_tone()`, `slide()`, `noise_burst()` and
  written by `write_wav()` in `gen_audio()`.

Run `python3 steel_streets/tools/gen_assets.py` from the repo root after
changing any generator; commit the regenerated files together with the
`.import` sidecars Godot produces on next open.

## Engine version

`project.godot` declares `config/features=PackedStringArray("4.3", "GL
Compatibility")`; the project uses only APIs available since Godot 4.3 and
runs unchanged on the engine built from this repository (see `version.py`).
