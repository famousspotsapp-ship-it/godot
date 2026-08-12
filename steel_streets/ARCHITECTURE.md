# Steel Streets — Architecture

Reference for working on the game. Everything here is derived from the files in
`steel_streets/`; see [README.md](README.md) for how to run it and the controls.

## Fork / engine context

This directory lives inside a fork of the Godot engine repository. The two are
independent:

- `project.godot` declares `config/features=PackedStringArray("4.3", "GL Compatibility")`,
  so the game runs on any Godot **4.3+** binary — including the engine built from
  this repo (`version.py` is currently `4.7-beta`).
- The rendering method is pinned to `gl_compatibility` (desktop and mobile), and
  `default_texture_filter=0` (nearest) plus `snap_2d_transforms_to_pixel` /
  `snap_2d_vertices_to_pixel` keep the pixel art crisp.
- Display is a 160x144 viewport with `stretch/mode="viewport"`, `aspect="keep"`,
  and a 640x576 window override.
- Nothing in `core/`, `scene/`, or `modules/` was modified for the game; it is a
  pure GDScript project that happens to be vendored in this tree.

## Scene flow

`GameManager` (autoload, `scripts/game_manager.gd`) owns all scene transitions
through `goto_title/goto_splash/goto_level/goto_game_over/goto_victory`, plus the
persistent run state (`lives`, `score`, `current_hp`, `max_hp`).

```
title_screen ──start──> splash_screen ──> level_1 ──boss_defeated──> victory ──any input──> title
                                             │
                                             └──lives exhausted──> game_over ──any input──> title
```

Run constants: `STARTING_LIVES = 3`, `STARTING_HP = 5`. Losing all HP calls
`GameManager.take_life()`; if lives remain the level is reloaded, otherwise the
game-over screen is shown.

## Node / script map

| Script | Extends | Role |
| --- | --- | --- |
| `game_manager.gd` | `Node` (autoload `GameManager`) | Run state, score/lives/HP signals, scene transitions |
| `level_1.gd` | `Node2D` | Builds the whole level procedurally (TileSet, TileMap, decor, entities, camera, HUD) |
| `player.gd` | `CharacterBody2D` | Movement, jump, melee attack window, ladder climbing, i-frames |
| `enemy_base.gd` | `CharacterBody2D` | Shared HP, hit flash, contact damage, death handshake |
| `foot_soldier.gd` | `enemy_base.gd` | Patrol + chase melee grunt |
| `shuriken_thrower.gd` | `enemy_base.gd` | Paces around spawn, fires `projectile.tscn` on a timer |
| `boss.gd` | `enemy_base.gd` | `PAUSE → WINDUP → CHARGE → RECOVER` state machine, boss HP signals |
| `projectile.gd` | `Area2D` | Straight-line shuriken (110 px/s) |
| `question_block.gd` | `Area2D` | Score + `heal_amount` (default 1 HP) pickup |
| `hud.gd` | `CanvasLayer` | Hearts, lives, score, boss bar |
| `anim_util.gd` | `Object` | Builds `SpriteFrames` from horizontal sprite strips |
| `title_screen.gd`, `splash_screen.gd`, `game_over.gd`, `victory.gd` | `Control` | Static screens |

Scripts use `extends "res://scripts/enemy_base.gd"` rather than `class_name`, so
there are **no globally registered game classes** — always `preload`/`extends` by
path.

## Communication

Entities are found and identified by group, not by node path:

- Groups: `player` (the body), `player_attack` (the player's `AttackHitbox` area),
  `enemy`, `boss`, `ladder`.
- `enemy_base.gd` looks for `area.is_in_group("player_attack")` on
  `hurtbox.area_entered`, then calls `owner_node.get_attack_damage()` if present.
- Damage to the player goes through duck-typed `take_damage(amount, from_position)`;
  ladders call `enter_ladder()` / `exit_ladder()` on whatever body they overlap.
- Enemies read the player via `get_tree().get_first_node_in_group("player")`.

Signals are only used for UI-facing state:

```
GameManager.score_changed / lives_changed / hp_changed ──> hud.gd
boss.boss_hp_changed / boss_defeated ──> hud.gd (via level_1 → hud.attach_boss)
boss.boss_defeated ──> level_1._on_boss_defeated ──> GameManager.goto_victory()
```

`level_1` spawns the boss before the HUD is ready, so it hands the boss to the
HUD with `call_deferred("_attach_boss_to_hud", boss)`.

## Collision layers

Layer names come from `[layer_names]` in `project.godot`:

| # | Name | Used by |
| --- | --- | --- |
| 1 | `world` | TileMap physics layer (solid tiles) |
| 2 | `player` | Player body |
| 3 | `player_hitbox` | Player `AttackHitbox` |
| 4 | `enemy` | Enemy bodies and hurtboxes |
| 5 | `enemy_hitbox` | Enemy contact hitboxes, projectiles |
| 6 | `ladder` | Ladder areas created in `level_1.gd` |
| 7 | `pickup` | Question blocks |
| 8 | `hazard` | Spikes created in `level_1.gd` |

Masks as set in the scenes: player body `1` (world); player attack hitbox `72`
(enemy + pickup); enemy body `1`; enemy hurtbox `4` (player_hitbox); enemy hitbox
`2` (player); projectile layer `16` mask `3` (world + player); question block
layer `64` mask `6` (player + player_hitbox). Ladders (`32`) and spikes (`128`)
mask `2`.

## Player mechanics (`player.gd`)

`SPEED 60`, `JUMP_VELOCITY -190`, `GRAVITY 700`, `CLIMB_SPEED 50`,
`INVINCIBLE_TIME 1.2`, `ATTACK_TIME 0.30`, `ATTACK_COOLDOWN 0.15`,
`ATTACK_DAMAGE 1`, knockback `(80, -120)`.

- There is **no combo system and no input buffering** — attacks are gated by
  `attack_timer`/`cooldown_timer` only.
- The attack hitbox is enabled only in the middle of the swing
  (`attack_timer < ATTACK_TIME * 0.75`) and is offset `10 * facing` on X.
- Climbing is driven by an overlap counter (`ladders_overlapping`); horizontal
  input or leaving the ladder cancels it, and jumping off uses 70% jump velocity.
- I-frames strobe `sprite.visible` at ~12 Hz; enemy hit flash modulates the
  sprite for `0.18 s`.

## Enemy tuning

| Enemy | HP | Score | Behaviour |
| --- | --- | --- | --- |
| Foot soldier | 2 | 100 | Patrols at 22 px/s, chases at 35 px/s within 70 px (and ±24 px vertically); turns at walls or ledges using a downward `PhysicsRayQueryParameters2D` probe |
| Shuriken thrower | 3 | 150 | Paces ±14 px around spawn at 12 px/s, fires every 1.6 s when the player is within 110 px |
| Boss | 9 | 1000 | `PAUSE 1.0 → WINDUP 0.5 → CHARGE 1.0 (70 px/s) → RECOVER 0.7` |

AI runs in `_physics_process` with hand-rolled state, gravity of `700`, and
`move_and_slide()`. There is **no NavigationAgent2D / navigation server usage**.

## Procedural level generation (`level_1.gd`)

The level is not authored in the `.tscn`; `_ready()` builds it:

1. `_build_tileset()` creates a `TileSet` (16 px tiles, one physics layer on
    collision layer 1) with a single `TileSetAtlasSource` over
    `assets/sprites/tiles/tileset.png` (11 tiles in a 1-row strip). The source is
    added to the `TileSet` **before** `create_tile()` so tiles inherit the physics
    layer; full-tile collision polygons are then set on `A_BRICK`, `A_CONCRETE`,
    `A_ROOF`.
2. `_paint_background()` (TileMap layer 0) and `_paint_foreground()` (layer 1,
    added at runtime) paint sky, brick wall, windows, doors and the `PLATFORMS`
    rectangles.
3. `_spawn_decor()` builds ladders and spikes as code-created `Area2D` nodes,
    `_spawn_pickups()`, `_spawn_enemies()`, `_spawn_player()` (plus a smoothed
    `Camera2D` limited to the world bounds), `_spawn_boss()`, `_spawn_hud()`.

World is `WORLD_W = 100` x `WORLD_H = 9` tiles (~6 screens), ground at row 8,
player spawn `(32, 100)`, boss at tile `(92, 7)`. Layout data lives in the
`PLATFORMS`, `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`,
`SHURIKEN_THROWERS` constants — edit those to change the level.

The project uses `TileMap` (not the newer `TileMapLayer` nodes).

## Assets

Every PNG and WAV under `assets/` is generated by `tools/gen_assets.py`
(Pillow + the stdlib `wave` module) — note the `tools/` subdirectory, not the
project root:

```bash
cd steel_streets && python3 tools/gen_assets.py
```

Sprite sheets are horizontal strips; `anim_util.gd` slices them into
`AtlasTexture` frames at runtime via `AnimUtil.build(FRAMES_SPEC)`, where each
entry declares `name`, `path`, `count`, `w`, `h`, `fps`, `loop`. Adding an
animation means adding a strip in `gen_assets.py` **and** an entry in the
consuming script's `FRAMES_SPEC`. Static title/UI strings are baked to PNGs by
`gen_assets.py` using a hand-rolled 5x7 bitmap font (`FONT_5x7`), so changing that
text means regenerating the assets.

## Engine-side commands (this fork)

```bash
scons tests=yes target=editor dev_build=yes -j$(nproc)   # build the editor binary
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test # engine unit tests
./bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets  # run the game
```

There are no automated tests for the game itself; it is verified by playing it.
