# Steel Streets — Architecture

Technical companion to [README.md](README.md). Everything below is derived from
the files in `steel_streets/`; the game does not modify the engine and uses no
C++ or GDExtension code.

## Runtime requirements

- `project.godot` declares `config/features=PackedStringArray("4.3", "GL Compatibility")`,
  so any Godot 4.3+ editor/export template (including the 4.7-beta engine in this
  repository) opens it.
- Renderer: `gl_compatibility` on desktop and mobile.
- 160×144 internal viewport, `stretch/mode=viewport`, `stretch/aspect=keep`,
  window override 640×576, nearest-neighbour texture filtering, 2D pixel snapping.

## Scene flow

All navigation goes through the `GameManager` autoload (`scripts/game_manager.gd`).
No scene calls `change_scene_to_file` directly.

```
title_screen ──start──▶ splash_screen ──2.5 s / any key──▶ level_1
                                                             │
                              boss_defeated (1.5 s delay) ───┼──▶ victory ──any key──▶ title
                              lives == 0 (1.2 s delay) ──────┴──▶ game_over ──any key──▶ title
                              lives > 0 (1.0 s delay) ───────────▶ level_1 (respawn)
```

| Scene | Script | Root type |
| --- | --- | --- |
| `scenes/title_screen.tscn` | `title_screen.gd` | `Control` — calls `GameManager.reset_run()` then `goto_splash()` |
| `scenes/splash_screen.tscn` | `splash_screen.gd` | `Control` — auto-advances after `HOLD_TIME = 2.5` |
| `scenes/level_1.tscn` | `level_1.gd` | `Node2D` — builds the whole level procedurally in `_ready()` |
| `scenes/hud.tscn` | `hud.gd` | `CanvasLayer` — hearts, score, lives, boss bar |
| `scenes/game_over.tscn` / `victory.tscn` | `game_over.gd` / `victory.gd` | `Control` — any input → `goto_title()` |

### GameManager API

```gdscript
const STARTING_LIVES := 3
const STARTING_HP := 5
var lives, score, max_hp, current_hp

signal score_changed(new_score)
signal lives_changed(new_lives)
signal hp_changed(current, maximum)

func reset_run()                # new run
func add_score(amount)
func set_hp(value)              # clamped to [0, max_hp]
func heal(amount)
func take_life() -> bool        # false when no lives remain; refills HP otherwise
func goto_title() / goto_splash() / goto_level() / goto_game_over() / goto_victory()
```

HP lives in `GameManager`, not on the player: `player.gd` calls
`GameManager.set_hp()` and the HUD listens to `hp_changed`.

## Level construction (`level_1.gd`)

`level_1.tscn` contains only an empty `TileMap`, an `Entities` node and a `Bgm`
player. `_ready()` does the rest:

1. `_build_tileset()` — creates a `TileSet` at runtime from
   `assets/sprites/tiles/tileset.png` (8 atlas tiles, 16 px; `A_BRICK`,
   `A_CONCRETE`, `A_ROOF` get collision polygons on physics layer 1).
2. `_paint_background()` / `_paint_foreground()` — TileMap layer 0 is sky and
   building facades, layer 1 is solid ground/rooftops from the `PLATFORMS` table.
3. `_spawn_decor()`, `_spawn_pickups()`, `_spawn_enemies()`, `_spawn_player()`,
   `_spawn_boss()`, `_spawn_hud()` — instantiate scenes at tile coordinates
   listed in the `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`,
   `SHURIKEN_THROWERS`, `BOSS_POS`, `PLAYER_SPAWN` constants.

Level layout is therefore edited in code (the constant tables), not in the
editor. World size is `WORLD_W = 100` × `WORLD_H = 9` tiles (1600×144 px).

Ladders are `Area2D`s built in `_make_ladder()`; they call
`player.enter_ladder()` / `exit_ladder()` on overlap. Spikes are `Area2D`s on
the hazard layer that call `player.take_damage()`.

## Entities

| Script | Extends | Notes |
| --- | --- | --- |
| `player.gd` | `CharacterBody2D` | Group `player`. States: grounded / climbing / dead. Timers drive attack (`ATTACK_TIME 0.30`, `ATTACK_COOLDOWN 0.15`) and i‑frames (`INVINCIBLE_TIME 1.2`). `take_damage(amount, from_position)` applies knockback. `_die()` calls `GameManager.take_life()` then respawns or goes to game over. |
| `enemy_base.gd` | `CharacterBody2D` | Group `enemy`. `@export max_hp / contact_damage / score_value`. Optional child nodes `Sprite`, `Hurtbox`, `Hitbox`, `SfxHit`. `take_damage()` → hit flash → `_die()` (adds score, disables areas, `queue_free`). |
| `foot_soldier.gd` | `enemy_base.gd` | Patrols (`PATROL_SPEED 22`), chases within `CHASE_RANGE 70` (`CHASE_SPEED 35`), turns at ledges via `_has_floor_ahead()`. |
| `shuriken_thrower.gd` | `enemy_base.gd` | Paces ±`PACE_RANGE 14` px around its origin; fires `projectile.tscn` every `FIRE_INTERVAL 1.6` s while the player is within `SIGHT_RANGE 110` px horizontally and 32 px vertically. |
| `boss.gd` | `enemy_base.gd` | `max_hp = 9`, `score_value = 1000`. Timer state machine `enum State { PAUSE, WINDUP, CHARGE, RECOVER }` — not an `AnimationTree`. Emits `boss_hp_changed` and `boss_defeated`. |
| `projectile.gd` | `Area2D` | Moves along `direction * SPEED (110)`; despawns after `LIFETIME 3.0`, on hitting the player, or on hitting `TileMap`/`StaticBody2D`. |
| `question_block.gd` | `Area2D` | Single-use pickup: `GameManager.add_score(score_reward)` and `heal(heal_amount)` on player contact or player attack. |
| `hud.gd` | `CanvasLayer` | Subscribes to the three `GameManager` signals; `attach_boss(boss)` wires the boss bar. |

### Combat handshake

Combat is purely group + `Area2D` based; no `AnimationPlayer` method tracks,
shaders or particles are involved.

- The player's `AttackHitbox` (`Area2D`, group `player_attack`) is enabled only
  while `attack_timer > 0`. An enemy `Hurtbox` receiving `area_entered` from a
  `player_attack` area calls `owner.get_attack_damage()` then `take_damage()`.
- Enemy `Hitbox.body_entered` and `projectile.body_entered` call
  `player.take_damage(contact_damage, global_position)` if the body is in the
  `player` group.
- Hit flash is done by toggling `sprite.modulate` in `enemy_base._process()`.
- Player i‑frames flash via `_update_invincibility_flash()` and short-circuit
  `take_damage()` while `invincible_timer > 0`.

### Physics layers (`project.godot` `[layer_names]`)

| Bit | Name | Used by |
| --- | --- | --- |
| 1 | `world` | TileMap solid tiles |
| 2 | `player` | player body |
| 3 | `player_hitbox` | player `AttackHitbox` |
| 4 | `enemy` | enemy bodies and hurtboxes |
| 5 | `enemy_hitbox` | enemy contact hitboxes, projectiles |
| 6 | `ladder` | ladder areas |
| 7 | `pickup` | question blocks |
| 8 | `hazard` | spikes |

## Animation

There are no `AnimationPlayer` or `AnimationTree` nodes. Every animated entity
owns an `AnimatedSprite2D` whose `SpriteFrames` is built at runtime by
`scripts/anim_util.gd`:

```gdscript
const FRAMES_SPEC := [
    {"name": "idle", "path": "res://assets/sprites/player/idle.png", "count": 2, "w": 16, "h": 24, "fps": 3.0, "loop": true},
]
sprite.sprite_frames = AnimUtil.build(FRAMES_SPEC)
```

`AnimUtil.build()` slices each horizontal strip PNG into `AtlasTexture` frames.
To add an animation: add a strip to `tools/gen_assets.py`, regenerate, and add
an entry to the entity's `FRAMES_SPEC`.

## Input actions

Defined in `project.godot`: `move_left`, `move_right`, `jump`, `attack`,
`climb_up`, `climb_down`, `start`. Each has keyboard and joypad bindings; see
README for the mapping table.

## Assets

All PNGs and WAVs are generated by `tools/gen_assets.py` (Pillow + stdlib
`wave`) using the 4-colour Game Boy palette. Do not hand-edit assets — change
the generator and re-run `python3 steel_streets/tools/gen_assets.py`.
