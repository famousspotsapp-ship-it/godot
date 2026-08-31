# Steel Streets — architecture reference

Implementation-level map of the bundled demo project, for anyone (human or
agent) that needs accurate context before touching it. Every claim here points
at the file that defines it.

## Engine version

The project is developed and run against the engine built from this
repository (`version.py`: **4.7-beta**). `project.godot` declares
`config/features=PackedStringArray("4.3", "GL Compatibility")`, i.e. 4.3 is the
minimum compatible feature level, not the engine the team builds. Run it with a
locally built binary:

```bash
bin/godot.linuxbsd.editor.dev.x86_64 --path steel_streets
```

## Scene flow

`run/main_scene` is `scenes/title_screen.tscn` — the splash screen is the
*second* scene, not the entry point. All transitions go through the
`GameManager` autoload; no scene calls `change_scene_to_file()` directly.

```
title_screen  --any input-->  splash_screen  --2.5s or any input-->  level_1
level_1  --boss_defeated-->  victory  --any input-->  title_screen
player death, lives left  -->  level_1 (reload)
player death, no lives    -->  game_over  --any input-->  title_screen
```

| Transition | Defined in |
| --- | --- |
| title → splash | `scripts/title_screen.gd` (`_advance`) |
| splash → level (auto after `HOLD_TIME = 2.5`) | `scripts/splash_screen.gd` |
| level → victory | `scripts/level_1.gd` `_on_boss_defeated` |
| death → level / game over | `scripts/player.gd` `_die` |
| victory / game over → title | `scripts/victory.gd`, `scripts/game_over.gd` |

## GameManager (autoload)

`scripts/game_manager.gd` owns all run state and the five navigation helpers.
It is a plain `Node`, registered as `GameManager` in `[autoload]`.

- State: `lives` (`STARTING_LIVES = 3`), `score`, `max_hp`/`current_hp`
  (`STARTING_HP = 5`).
- Signals: `score_changed(new_score)`, `lives_changed(new_lives)`,
  `hp_changed(current, maximum)` — the HUD is driven entirely by these.
- Mutators: `reset_run()`, `add_score()`, `set_hp()`, `heal()`,
  `take_life() -> bool` (false when the run is over).
- Navigation: `goto_title()`, `goto_splash()`, `goto_level()`,
  `goto_game_over()`, `goto_victory()`.

`title_screen.gd` calls `reset_run()`, so run state is cleared on the title
screen rather than at level load.

## Entities

`scripts/enemy_base.gd` (`CharacterBody2D`) implements health, contact damage,
hit flash, and the death handshake (award `score_value`, disable hitboxes,
`queue_free()`). Enemies extend it by path, not by `class_name`:
`extends "res://scripts/enemy_base.gd"`.

| Entity | Script | Tuning |
| --- | --- | --- |
| Player | `scripts/player.gd` (`CharacterBody2D`) | `SPEED 60`, `JUMP_VELOCITY -190`, `GRAVITY 700`, `CLIMB_SPEED 50`, `INVINCIBLE_TIME 1.2`, `ATTACK_TIME 0.30`, `ATTACK_DAMAGE 1` |
| Foot soldier | `scripts/foot_soldier.gd` | `max_hp 2`, `score_value 100`, patrol 22 / chase 35, `CHASE_RANGE 70`, ledge check via `intersect_ray` |
| Shuriken thrower | `scripts/shuriken_thrower.gd` | `max_hp 3`, `score_value 150`, paces ±14 px, fires every `1.6 s` within `SIGHT_RANGE 110` |
| Boss | `scripts/boss.gd` | `max_hp 9`, `score_value 1000`, `State { PAUSE, WINDUP, CHARGE, RECOVER }`, emits `boss_hp_changed` / `boss_defeated` |
| Shuriken | `scripts/projectile.gd` (`Area2D`) | `SPEED 110`, `LIFETIME 3.0`, `DAMAGE 1` |
| Question block | `scripts/question_block.gd` (`Area2D`) | `score_reward 50`, `heal_amount 1`, consumed on body or `player_attack` overlap |

Group names carry the contracts: `player`, `enemy`, `boss`, `player_attack`.
Damage flows through duck-typed methods — `take_damage(amount, from_position)`
on the player, `get_attack_damage()` on attack owners.

## Physics layers

Declared in `project.godot` `[layer_names]`:

| # | Name | | # | Name |
| --- | --- | --- | --- | --- |
| 1 | `world` | | 5 | `enemy_hitbox` |
| 2 | `player` | | 6 | `ladder` |
| 3 | `player_hitbox` | | 7 | `pickup` |
| 4 | `enemy` | | 8 | `hazard` |

## Level 1 is generated in code

`scripts/level_1.gd` builds the whole level at `_ready()` — there is no
hand-authored tilemap. It constructs the `TileSet` (16 px tiles, one physics
layer), paints a background and a foreground layer, then spawns ladders,
spikes, pickups, enemies, the boss, the player, and the HUD from const tables
(`PLATFORMS`, `LADDERS`, `SPIKES`, `PICKUPS`, `FOOT_SOLDIERS`,
`SHURIKEN_THROWERS`, `BOSS_POS`, `PLAYER_SPAWN`). World size is
`WORLD_W 100 × WORLD_H 9` tiles with the ground at row 8.

Level layout changes belong in those tables, not in `scenes/level_1.tscn`.

## HUD

`scripts/hud.gd` (`CanvasLayer`) rebuilds the heart row on every `hp_changed`,
formats score as `%05d`, and shows the boss bar only after `level_1.gd` calls
`attach_boss()`, which connects `boss_hp_changed` / `boss_defeated`.

## Rendering and assets

- 160×144 viewport, `window/stretch/mode="viewport"` with `aspect="keep"`,
  640×576 window override, nearest-neighbour filtering
  (`default_texture_filter=0`) and 2D pixel snapping — all in `project.godot`.
- Renderer is `gl_compatibility` (also for mobile).
- Four-colour Game Boy palette: `#e0f8d0`, `#88c070`, `#346856`, `#081820`.
- Every PNG and WAV is generated by `tools/gen_assets.py` (Pillow + `wave`);
  regenerate with `python3 steel_streets/tools/gen_assets.py`.
- `scripts/anim_util.gd` is an `Object` with a single static `build(specs)`
  that turns horizontal sprite strips into `SpriteFrames` using
  `AtlasTexture` regions. Each entity declares its own `FRAMES_SPEC` const.
